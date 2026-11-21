#include "ghc_lobby.h"

#include <cell/http.h>
#include <cell/libnet.h>
#include <cell/ssl.h>
#include <cell/sysmodule.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define GHC_HTTP_POOL_SIZE (256u * 1024u)
#define GHC_SSL_POOL_SIZE  (128u * 1024u)
#define GHC_HTTP_RESPONSE_CAPACITY 2048u
#define GHC_HTTP_TIMEOUT_US 8000000LL

static uint8_t g_http_pool[GHC_HTTP_POOL_SIZE] __attribute__((aligned(16)));
static uint8_t g_ssl_pool[GHC_SSL_POOL_SIZE] __attribute__((aligned(16)));

static CellHttpClientId g_http_client = NULL;
static uint8_t g_runtime_started = 0;

static uint8_t g_owned_net_module = 0;
static uint8_t g_owned_http_util_module = 0;
static uint8_t g_owned_ssl_module = 0;
static uint8_t g_owned_https_module = 0;
static uint8_t g_owned_http_module = 0;
static uint8_t g_owned_net_stack = 0;
static uint8_t g_owned_ssl_runtime = 0;
static uint8_t g_owned_http_runtime = 0;
static uint8_t g_owned_https_runtime = 0;

static int gh_load_module(uint16_t module_id, uint8_t *owned) {
    int result;
    if (!owned) return -1;
    result = cellSysmoduleLoadModule(module_id);
    if (result == CELL_SYSMODULE_LOADED) {
        *owned = 1;
        return 0;
    }
    if (result == CELL_SYSMODULE_ERROR_DUPLICATED) {
        *owned = 0;
        return 0;
    }
    return result;
}

static void gh_unload_module(uint16_t module_id, uint8_t *owned) {
    if (owned && *owned) {
        (void)cellSysmoduleUnloadModule(module_id);
        *owned = 0;
    }
}

static int gh_ssl_verify(
    uint32_t verify_error,
    CellSslCert const certificates[],
    int certificate_count,
    const char *hostname,
    CellHttpSslId id,
    void *user_arg
) {
    (void)certificates;
    (void)certificate_count;
    (void)hostname;
    (void)id;
    (void)user_arg;
    return verify_error == (uint32_t)CELL_HTTPS_VERIFY_ERROR_NONE ? 0 : -1;
}

static void gh_runtime_rollback(void) {
    if (g_http_client) {
        (void)cellHttpDestroyClient(g_http_client);
        g_http_client = NULL;
    }
    if (g_owned_https_runtime) {
        (void)cellHttpsEnd();
        g_owned_https_runtime = 0;
    }
    if (g_owned_http_runtime) {
        (void)cellHttpEnd();
        g_owned_http_runtime = 0;
    }
    if (g_owned_ssl_runtime) {
        (void)cellSslEnd();
        g_owned_ssl_runtime = 0;
    }

    gh_unload_module(CELL_SYSMODULE_HTTP, &g_owned_http_module);
    gh_unload_module(CELL_SYSMODULE_HTTPS, &g_owned_https_module);
    gh_unload_module(CELL_SYSMODULE_SSL, &g_owned_ssl_module);
    gh_unload_module(CELL_SYSMODULE_HTTP_UTIL, &g_owned_http_util_module);

    if (g_owned_net_stack) {
        (void)sys_net_finalize_network();
        g_owned_net_stack = 0;
    }
    gh_unload_module(CELL_SYSMODULE_NET, &g_owned_net_module);
    g_runtime_started = 0;
}

int ghc_lobby_runtime_start(void) {
    int result;

    if (g_runtime_started) return GHC_LOBBY_OK;

    result = gh_load_module(CELL_SYSMODULE_NET, &g_owned_net_module);
    if (result != 0) goto failed;

    /*
     * If the game already owns libnet, do not initialize or finalize its
     * shared network stack from this plugin. A newly loaded module is ours.
     */
    if (g_owned_net_module) {
        result = sys_net_initialize_network();
        if (result != 0) goto failed;
        g_owned_net_stack = 1;
    }

    result = gh_load_module(CELL_SYSMODULE_HTTP_UTIL, &g_owned_http_util_module);
    if (result != 0) goto failed;
    result = gh_load_module(CELL_SYSMODULE_SSL, &g_owned_ssl_module);
    if (result != 0) goto failed;
    result = gh_load_module(CELL_SYSMODULE_HTTPS, &g_owned_https_module);
    if (result != 0) goto failed;
    result = gh_load_module(CELL_SYSMODULE_HTTP, &g_owned_http_module);
    if (result != 0) goto failed;

    result = cellSslInit(g_ssl_pool, sizeof(g_ssl_pool));
    if (result == 0) {
        g_owned_ssl_runtime = 1;
    } else if (result != (int)CELL_SSL_ERROR_ALREADY_INITIALIZED) {
        goto failed;
    }

    result = cellHttpInit(g_http_pool, sizeof(g_http_pool));
    if (result == 0) {
        g_owned_http_runtime = 1;
    } else if (result != (int)CELL_HTTP_ERROR_ALREADY_INITIALIZED) {
        goto failed;
    }

    result = cellHttpsInit(0, NULL);
    if (result == 0) {
        g_owned_https_runtime = 1;
    } else {
        goto failed;
    }

    result = cellHttpCreateClient(&g_http_client);
    if (result != 0) goto failed;

    (void)cellHttpClientSetAutoRedirect(g_http_client, false);
    (void)cellHttpClientSetAutoAuthentication(g_http_client, false);
    (void)cellHttpClientSetKeepAlive(g_http_client, false);
    (void)cellHttpClientSetConnTimeout(g_http_client, GHC_HTTP_TIMEOUT_US);
    (void)cellHttpClientSetSendTimeout(g_http_client, GHC_HTTP_TIMEOUT_US);
    (void)cellHttpClientSetRecvTimeout(g_http_client, GHC_HTTP_TIMEOUT_US);
    result = cellHttpClientSetSslCallback(g_http_client, gh_ssl_verify, NULL);
    if (result != 0) goto failed;

    g_runtime_started = 1;
    return GHC_LOBBY_OK;

failed:
    gh_runtime_rollback();
    return result;
}

void ghc_lobby_runtime_stop(void) {
    gh_runtime_rollback();
}

static int gh_parse_https_url(
    const char *url,
    char *hostname,
    size_t hostname_capacity,
    char *path,
    size_t path_capacity,
    uint32_t *port
) {
    const char *authority;
    const char *authority_end;
    const char *path_start;
    const char *colon = NULL;
    size_t host_length;
    size_t path_length;
    uint32_t parsed_port = 443;
    const char *cursor;

    if (!url || !hostname || !path || !port) return GHC_LOBBY_ERR_BAD_URL;
    if (strncmp(url, "https://", 8) != 0) return GHC_LOBBY_ERR_BAD_URL;

    authority = url + 8;
    authority_end = authority;
    while (*authority_end && *authority_end != '/' && *authority_end != '?' && *authority_end != '#') {
        if (*authority_end == '@') return GHC_LOBBY_ERR_BAD_URL;
        if (*authority_end == ':') colon = authority_end;
        ++authority_end;
    }
    if (authority_end == authority) return GHC_LOBBY_ERR_BAD_URL;

    host_length = colon ? (size_t)(colon - authority) : (size_t)(authority_end - authority);
    if (host_length == 0 || host_length >= hostname_capacity) return GHC_LOBBY_ERR_BAD_URL;
    memcpy(hostname, authority, host_length);
    hostname[host_length] = '\0';

    if (colon) {
        cursor = colon + 1;
        if (cursor == authority_end) return GHC_LOBBY_ERR_BAD_URL;
        parsed_port = 0;
        while (cursor < authority_end) {
            if (*cursor < '0' || *cursor > '9') return GHC_LOBBY_ERR_BAD_URL;
            parsed_port = parsed_port * 10u + (uint32_t)(*cursor - '0');
            if (parsed_port > 65535u) return GHC_LOBBY_ERR_BAD_URL;
            ++cursor;
        }
        if (parsed_port == 0) return GHC_LOBBY_ERR_BAD_URL;
    }

    path_start = authority_end;
    if (*path_start == '#') return GHC_LOBBY_ERR_BAD_URL;
    if (*path_start == '?') return GHC_LOBBY_ERR_BAD_URL;
    if (*path_start == '\0') {
        if (path_capacity < 2) return GHC_LOBBY_ERR_BAD_URL;
        path[0] = '/';
        path[1] = '\0';
    } else {
        path_length = strlen(path_start);
        if (path_length >= path_capacity) return GHC_LOBBY_ERR_BAD_URL;
        memcpy(path, path_start, path_length + 1);
    }

    *port = parsed_port;
    return 0;
}

static int gh_build_url(
    const char *base_url,
    const char *suffix,
    char *output,
    size_t output_capacity
) {
    size_t base_length;
    size_t suffix_length;
    size_t trim = 0;

    if (!base_url || !suffix || !output || output_capacity == 0) return -1;
    base_length = strlen(base_url);
    suffix_length = strlen(suffix);
    if (base_length == 0 || suffix_length == 0) return -1;
    while (base_length > 0 && base_url[base_length - 1] == '/') {
        --base_length;
        ++trim;
    }
    (void)trim;
    if (base_length + suffix_length + 1 > output_capacity) return -1;
    memcpy(output, base_url, base_length);
    memcpy(output + base_length, suffix, suffix_length + 1);
    return 0;
}

static int gh_header_safe(const char *value) {
    if (!value) return 0;
    while (*value) {
        if (*value == '\r' || *value == '\n') return 0;
        ++value;
    }
    return 1;
}

static int gh_http_post_json(
    const char *url,
    const char *bearer_token,
    const char *body,
    char *response,
    size_t response_capacity,
    size_t *response_length,
    int *http_status
) {
    char hostname[128];
    char path[512];
    uint32_t port = 443;
    CellHttpUri uri;
    CellHttpTransId transaction = NULL;
    CellHttpHeader content_type = { "Content-Type", "application/json" };
    CellHttpHeader accept = { "Accept", "application/json" };
    CellHttpHeader authorization;
    size_t body_length;
    size_t sent_total = 0;
    size_t response_total = 0;
    size_t received = 0;
    int result;

    if (!g_runtime_started || !g_http_client) return GHC_LOBBY_ERR_NOT_INITIALIZED;
    if (!body || !response || response_capacity < 2 || !response_length || !http_status) return -1;
    if (bearer_token && !gh_header_safe(bearer_token)) return -1;

    result = gh_parse_https_url(url, hostname, sizeof(hostname), path, sizeof(path), &port);
    if (result != 0) return result;

    memset(&uri, 0, sizeof(uri));
    uri.scheme = "https";
    uri.hostname = hostname;
    uri.path = path;
    uri.port = port;

    result = cellHttpCreateTransaction(&transaction, g_http_client, CELL_HTTP_METHOD_POST, &uri);
    if (result != 0) return result;

    result = cellHttpRequestAddHeader(transaction, &content_type);
    if (result != 0) goto cleanup;
    result = cellHttpRequestAddHeader(transaction, &accept);
    if (result != 0) goto cleanup;

    if (bearer_token) {
        char authorization_value[GHC_LOBBY_TOKEN_HEX_LENGTH + 8];
        size_t token_length = strlen(bearer_token);
        if (token_length != GHC_LOBBY_TOKEN_HEX_LENGTH
            || token_length + sizeof("Bearer ") > sizeof(authorization_value)) {
            result = -1;
            goto cleanup;
        }
        memcpy(authorization_value, "Bearer ", 7);
        memcpy(authorization_value + 7, bearer_token, token_length + 1);
        authorization.name = "Authorization";
        authorization.value = authorization_value;
        result = cellHttpRequestAddHeader(transaction, &authorization);
        if (result != 0) goto cleanup;
    }

    body_length = strlen(body);
    result = cellHttpRequestSetContentLength(transaction, body_length);
    if (result != 0) goto cleanup;

    while (sent_total < body_length) {
        size_t sent = 0;
        result = cellHttpSendRequest(
            transaction,
            body + sent_total,
            body_length - sent_total,
            &sent
        );
        if (result != 0) goto cleanup;
        if (sent == 0) {
            result = CELL_HTTP_ERROR_INTERNAL;
            goto cleanup;
        }
        sent_total += sent;
    }

    response[0] = '\0';
    do {
        received = 0;
        result = cellHttpRecvResponse(
            transaction,
            response + response_total,
            response_capacity - response_total - 1,
            &received
        );
        if (result != 0) goto cleanup;
        response_total += received;
        if (received == 0) break;
        if (response_total >= response_capacity - 1) {
            result = GHC_LOBBY_ERR_RESPONSE_TOO_LARGE;
            goto cleanup;
        }
    } while (1);

    response[response_total] = '\0';
    *response_length = response_total;
    result = cellHttpResponseGetStatusCode(transaction, (int32_t *)http_status);

cleanup:
    if (transaction) (void)cellHttpDestroyTransaction(transaction);
    return result;
}

static int gh_room_path(const char *room_code, const char *action, char *path, size_t capacity) {
    size_t i;
    int written;
    if (!room_code || !path || capacity == 0) return -1;
    if (strlen(room_code) != 6) return -1;
    for (i = 0; i < 6; ++i) {
        char c = room_code[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return -1;
    }
    written = snprintf(path, capacity, "/v1/rooms/%s%s%s", room_code,
                       action ? "/" : "", action ? action : "");
    if (written < 0 || (size_t)written >= capacity) return -1;
    return 0;
}

static int gh_authenticated_post(
    const char *base_url,
    const GhcGuestSession *session,
    const char *path,
    const char *body,
    int expected_status,
    int *http_status
) {
    char url[768];
    char response[GHC_HTTP_RESPONSE_CAPACITY];
    size_t response_length = 0;
    int status = 0;
    int result;

    if (!session || !session->token[0] || !path) return -1;
    if (gh_build_url(base_url, path, url, sizeof(url)) != 0) return GHC_LOBBY_ERR_BAD_URL;

    result = gh_http_post_json(
        url,
        session->token,
        body,
        response,
        sizeof(response),
        &response_length,
        &status
    );
    if (http_status) *http_status = status;
    if (result != 0) return result;
    return status == expected_status ? GHC_LOBBY_OK : GHC_LOBBY_ERR_HTTP_STATUS;
}

int ghc_lobby_create_guest_session(
    const char *https_base_url,
    GhcGuestSession *session,
    int *http_status
) {
    char url[768];
    char response[GHC_HTTP_RESPONSE_CAPACITY];
    size_t response_length = 0;
    int status = 0;
    int result;

    if (!session || gh_build_url(https_base_url, "/v1/sessions", url, sizeof(url)) != 0) {
        return GHC_LOBBY_ERR_BAD_URL;
    }
    result = gh_http_post_json(
        url,
        NULL,
        "{\"mode\":\"guest\"}",
        response,
        sizeof(response),
        &response_length,
        &status
    );
    if (http_status) *http_status = status;
    if (result != 0) return result;
    if (status != 201) return GHC_LOBBY_ERR_HTTP_STATUS;
    return ghc_lobby_parse_guest_session(response, session) == 0
        ? GHC_LOBBY_OK : GHC_LOBBY_ERR_BAD_RESPONSE;
}

int ghc_lobby_create_room(
    const char *https_base_url,
    const GhcGuestSession *session,
    char room_code[7],
    int *http_status
) {
    char url[768];
    char response[GHC_HTTP_RESPONSE_CAPACITY];
    size_t response_length = 0;
    int status = 0;
    int result;

    if (!session || !room_code || gh_build_url(https_base_url, "/v1/rooms", url, sizeof(url)) != 0) {
        return GHC_LOBBY_ERR_BAD_URL;
    }
    result = gh_http_post_json(
        url,
        session->token,
        "{}",
        response,
        sizeof(response),
        &response_length,
        &status
    );
    if (http_status) *http_status = status;
    if (result != 0) return result;
    if (status != 201) return GHC_LOBBY_ERR_HTTP_STATUS;
    return ghc_lobby_parse_room_code(response, room_code) == 0
        ? GHC_LOBBY_OK : GHC_LOBBY_ERR_BAD_RESPONSE;
}

int ghc_lobby_join_room(
    const char *https_base_url,
    const GhcGuestSession *session,
    const char room_code[7],
    int *http_status
) {
    char path[96];
    if (gh_room_path(room_code, "join", path, sizeof(path)) != 0) return -1;
    return gh_authenticated_post(https_base_url, session, path, "{}", 200, http_status);
}

int ghc_lobby_set_ready(
    const char *https_base_url,
    const GhcGuestSession *session,
    const char room_code[7],
    int ready,
    int *http_status
) {
    char path[96];
    if (gh_room_path(room_code, "ready", path, sizeof(path)) != 0) return -1;
    return gh_authenticated_post(
        https_base_url,
        session,
        path,
        ready ? "{\"ready\":true}" : "{\"ready\":false}",
        200,
        http_status
    );
}

int ghc_lobby_start_mission(
    const char *https_base_url,
    const GhcGuestSession *session,
    const char room_code[7],
    int *http_status
) {
    char path[96];
    if (gh_room_path(room_code, "start", path, sizeof(path)) != 0) return -1;
    return gh_authenticated_post(https_base_url, session, path, "{}", 200, http_status);
}

int ghc_lobby_leave_room(
    const char *https_base_url,
    const GhcGuestSession *session,
    const char room_code[7],
    int *http_status
) {
    char path[96];
    if (gh_room_path(room_code, "leave", path, sizeof(path)) != 0) return -1;
    return gh_authenticated_post(https_base_url, session, path, "{}", 200, http_status);
}
