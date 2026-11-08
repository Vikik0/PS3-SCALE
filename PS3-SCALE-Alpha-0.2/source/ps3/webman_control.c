#include "ps3scale/webman.h"
#include "ps3scale/webman_command.h"

#include <arpa/inet.h>
#include <net/net.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
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
                           size_t response_size)
{
    int sock = -1;
    int ret;
    int result = -1;
    size_t used = 0;
    size_t sent = 0;
    size_t request_length;
    struct sockaddr_in server;
    struct timeval timeout;
    char request[WEBMAN_HTTP_REQUEST_MAX];

    if (!path || path[0] != '/' || strlen(path) > WEBMAN_HTTP_PATH_MAX)
        return -1;
    if (response && response_size > 0)
        response[0] = '\0';

    if (netInitialize() != 0)
        return -1;

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
        goto cleanup;

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
    if (inet_pton(AF_INET, "127.0.0.1", &server.sin_addr) != 1)
        goto cleanup;
    if (connect_with_timeout(sock, (struct sockaddr *)&server,
                             sizeof(server)) != 0)
        goto cleanup;

    ret = snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n",
        path);
    if (ret < 0 || (size_t)ret >= sizeof(request))
        goto cleanup;
    request_length = (size_t)ret;

    while (sent < request_length) {
        ret = (int)write(sock, request + sent, request_length - sent);
        if (ret < 0 && errno == EINTR)
            continue;
        if (ret <= 0)
            goto cleanup;
        sent += (size_t)ret;
    }

    if (response && response_size > 1) {
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

            /* Read a small response body too; PS3MAPI reports its version there. */
            if (used > 512)
                break;
        }
        response[used] = '\0';
        if (http_status_is_success(response))
            result = 0;
    }

cleanup:
    if (sock >= 0) {
        (void)shutdown(sock, SHUT_RDWR);
        close(sock);
    }
    netDeinitialize();
    return result;
}

int ps3scale_webman_available(void)
{
    char response[1024];
    return webman_http_get("/cpursx.ps3", response, sizeof(response));
}

int ps3scale_webman_ps3mapi_available(void)
{
    char response[2048];

    if (webman_http_get("/ps3mapi.ps3?SERVER%20GETVERSION",
                        response, sizeof(response)) != 0)
        return -1;

    if (!strstr(response, "version") && !strstr(response, "response"))
        return -1;
    return 0;
}

int ps3scale_webman_start_game(const ps3scale_game_t *game)
{
    char request_path[WEBMAN_HTTP_PATH_MAX + 1];
    char response[1024];

    if (ps3scale_webman_build_start_path(game, request_path,
                                         sizeof(request_path)) != 0)
        return -1;

    return webman_http_get(request_path, response, sizeof(response));
}
