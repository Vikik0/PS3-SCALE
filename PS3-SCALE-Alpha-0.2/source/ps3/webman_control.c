#include "ps3scale/webman.h"
#include "ps3scale/webman_command.h"

#include <arpa/inet.h>
#include <net/net.h>
#include <net/netctl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sysmodule/sysmodule.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WEBMAN_HTTP_PATH_MAX 1536
#define WEBMAN_HTTP_REQUEST_MAX 2048
#define WEBMAN_CONNECT_TIMEOUT_SEC 3
#define WEBMAN_IO_TIMEOUT_SEC 2

static int network_modules_ready(void)
{
    if (sysModuleLoad(SYSMODULE_NET) < 0)
        return -1;
    if (sysModuleLoad(SYSMODULE_NETCTL) < 0) {
        sysModuleUnload(SYSMODULE_NET);
        return -1;
    }
    if (netInitialize() < 0) {
        sysModuleUnload(SYSMODULE_NETCTL);
        sysModuleUnload(SYSMODULE_NET);
        return -1;
    }
    if (netCtlInit() < 0) {
        netDeinitialize();
        sysModuleUnload(SYSMODULE_NETCTL);
        sysModuleUnload(SYSMODULE_NET);
        return -1;
    }
    return 0;
}

static void network_modules_shutdown(void)
{
    netCtlTerm();
    netDeinitialize();
    sysModuleUnload(SYSMODULE_NETCTL);
    sysModuleUnload(SYSMODULE_NET);
}

static int connect_with_timeout(int sock, const struct sockaddr *address,
                                socklen_t address_length)
{
    fd_set write_set;
    struct timeval timeout;
    int flags;
    int result;
    int socket_error = 0;
    socklen_t error_length = sizeof(socket_error);

    flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    result = connect(sock, address, address_length);
    if (result != 0 && errno != EINPROGRESS && errno != EWOULDBLOCK &&
        errno != EALREADY) {
        (void)fcntl(sock, F_SETFL, flags);
        return -1;
    }

    if (result != 0) {
        FD_ZERO(&write_set);
        FD_SET(sock, &write_set);
        timeout.tv_sec = WEBMAN_CONNECT_TIMEOUT_SEC;
        timeout.tv_usec = 0;

        result = select(sock + 1, NULL, &write_set, NULL, &timeout);
        if (result <= 0) {
            (void)fcntl(sock, F_SETFL, flags);
            return -1;
        }

        if (getsockopt(sock, SOL_SOCKET, SO_ERROR,
                       &socket_error, &error_length) != 0 ||
            socket_error != 0) {
            (void)fcntl(sock, F_SETFL, flags);
            return -1;
        }
    }

    if (fcntl(sock, F_SETFL, flags) < 0)
        return -1;
    return 0;
}

static int http_status_is_success(const char *response)
{
    const char *space;
    int status;

    if (!response || strncmp(response, "HTTP/", 5) != 0)
        return 0;
    space = strchr(response, ' ');
    if (!space)
        return 0;
    status = atoi(space + 1);
    return status >= 200 && status < 400;
}

/* Do not rely only on SO_RCVTIMEO: some PS3 socket builds ignore it. */
static int wait_for_readable(int sock)
{
    fd_set read_set;
    struct timeval timeout;
    int result;

    FD_ZERO(&read_set);
    FD_SET(sock, &read_set);
    timeout.tv_sec = WEBMAN_IO_TIMEOUT_SEC;
    timeout.tv_usec = 0;

    do {
        result = select(sock + 1, &read_set, NULL, NULL, &timeout);
    } while (result < 0 && errno == EINTR);

    return result > 0 && FD_ISSET(sock, &read_set) ? 0 : -1;
}

static int webman_http_get(const char *path, char *response,
                           size_t response_size, int read_response_body)
{
    int sock = -1;
    int ret;
    int result = PS3SCALE_WEBMAN_ERROR_INVALID_REQUEST;
    int network_ready = 0;
    size_t used = 0;
    size_t sent = 0;
    size_t request_length;
    struct sockaddr_in server;
    struct timeval timeout;
    char request[WEBMAN_HTTP_REQUEST_MAX];

    if (!path || path[0] != '/' || strlen(path) > WEBMAN_HTTP_PATH_MAX ||
        !response || response_size < 16)
        return PS3SCALE_WEBMAN_ERROR_INVALID_REQUEST;
    response[0] = '\0';

    /* Match the lifecycle used by the HTTPS catalog worker. The previous
       implementation called netInitialize() without loading NET/NETCTL or
       starting netCtl, which made the local webMAN request fail on some boots. */
    if (network_modules_ready() != 0)
        return PS3SCALE_WEBMAN_ERROR_NETWORK_INIT;
    network_ready = 1;

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        result = PS3SCALE_WEBMAN_ERROR_SOCKET;
        goto cleanup;
    }

    timeout.tv_sec = WEBMAN_IO_TIMEOUT_SEC;
    timeout.tv_usec = 0;
#ifdef SO_RCVTIMEO
    (void)setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
#ifdef SO_SNDTIMEO
    (void)setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif

    memset(&server, 0, sizeof(server));
#ifdef __PSL1GHT__
    server.sin_len = sizeof(server);
#endif
    server.sin_family = AF_INET;
    server.sin_port = htons(80);
    if (inet_pton(AF_INET, "127.0.0.1", &server.sin_addr) != 1) {
        result = PS3SCALE_WEBMAN_ERROR_CONNECT;
        goto cleanup;
    }
    if (connect_with_timeout(sock, (struct sockaddr *)&server,
                             sizeof(server)) != 0) {
        result = PS3SCALE_WEBMAN_ERROR_CONNECT;
        goto cleanup;
    }

    ret = snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\nHost: 127.0.0.1\r\n"
        "User-Agent: PS3SCALE/0.29\r\nConnection: close\r\n\r\n",
        path);
    if (ret < 0 || (size_t)ret >= sizeof(request)) {
        result = PS3SCALE_WEBMAN_ERROR_INVALID_REQUEST;
        goto cleanup;
    }
    request_length = (size_t)ret;

    while (sent < request_length) {
        ret = (int)write(sock, request + sent, request_length - sent);
        if (ret < 0 && errno == EINTR)
            continue;
        if (ret <= 0) {
            result = PS3SCALE_WEBMAN_ERROR_SEND;
            goto cleanup;
        }
        sent += (size_t)ret;
    }

    while (used + 1 < response_size) {
        if (wait_for_readable(sock) != 0)
            break;
        ret = (int)read(sock, response + used, response_size - used - 1);
        if (ret < 0 && errno == EINTR)
            continue;
        if (ret <= 0)
            break;
        used += (size_t)ret;
        response[used] = '\0';

        /*
         * Game-launch and basic webMAN probes only need the HTTP headers;
         * a launch can keep the connection busy while webMAN mounts the game.
         * The PS3MAPI version probe is different: it must read the JSON body,
         * otherwise a headers-only TCP packet creates a false "MAPI offline".
         */
        if (strstr(response, "\r\n\r\n") != NULL) {
            if (!read_response_body)
                break;
            {
                const char *body = strstr(response, "\r\n\r\n") + 4;
                if (strstr(body, "\"response\"") && strchr(body, '}'))
                    break;
            }
        }
    }
    response[used] = '\0';
    if (used == 0 || strncmp(response, "HTTP/", 5) != 0) {
        result = PS3SCALE_WEBMAN_ERROR_RESPONSE;
    } else if (http_status_is_success(response)) {
        result = 0;
    } else {
        result = PS3SCALE_WEBMAN_ERROR_HTTP_STATUS;
    }

cleanup:
    if (sock >= 0) {
        (void)shutdown(sock, SHUT_RDWR);
        close(sock);
    }
    if (network_ready)
        network_modules_shutdown();
    return result;
}

int ps3scale_webman_available(void)
{
    char response[1024];
    return webman_http_get("/cpursx.ps3", response, sizeof(response), 0);
}

int ps3scale_webman_ps3mapi_available(void)
{
    char response[2048];
    const char *body;

    if (webman_http_get("/ps3mapi.ps3?SERVER%20GETVERSION",
                        response, sizeof(response), 1) != 0)
        return -1;

    body = strstr(response, "\r\n\r\n");
    if (!body)
        return -1;
    body += 4;
    if (!strstr(body, "\"response\"") || !strchr(body, '}'))
        return -1;
    return 0;
}

int ps3scale_webman_start_game(const ps3scale_game_t *game)
{
    char request_path[WEBMAN_HTTP_PATH_MAX + 1];
    char response[1024];

    if (ps3scale_webman_build_start_path(game, request_path,
                                         sizeof(request_path)) != 0)
        return PS3SCALE_WEBMAN_ERROR_INVALID_REQUEST;

    return webman_http_get(request_path, response, sizeof(response), 0);
}


int ps3scale_webman_start_app(const char *title_id)
{
    char request_path[64];
    char response[1024];
