#include "ps3scale/webman.h"

#include <arpa/inet.h>
#include <net/net.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>

static int webman_http_get(const char *path, char *response, size_t response_size)
{
    int sock;
    int ret;
    size_t used = 0;
    struct sockaddr_in server;
    struct timeval timeout;
    char request[768];

    if (!path || path[0] != '/' || strlen(path) > 512)
        return -1;
    if (response && response_size > 0)
        response[0] = '\0';

    if (netInitialize() != 0)
        return -1;

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        netDeinitialize();
        return -1;
    }

    timeout.tv_sec = 4;
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
        close(sock);
        netDeinitialize();
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) != 0) {
        close(sock);
        netDeinitialize();
        return -1;
    }

    snprintf(request, sizeof(request),
        "GET %s HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n", path);
    ret = (int)write(sock, request, strlen(request));
    if (ret <= 0) {
        close(sock);
        netDeinitialize();
        return -1;
    }

    if (response && response_size > 1) {
        while (used + 1 < response_size) {
            ret = (int)read(sock, response + used, response_size - used - 1);
            if (ret <= 0)
                break;
            used += (size_t)ret;
        }
        response[used] = '\0';
    }

    shutdown(sock, SHUT_RDWR);
    close(sock);
    netDeinitialize();

    if (response && (strstr(response, "404") || strstr(response, "Not Found")))
        return -1;
    return 0;
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

    /* A live PS3MAPI endpoint returns a JSON response with server version. */
    if (!strstr(response, "200 OK") ||
        (!strstr(response, "version") && !strstr(response, "response")))
        return -1;
    return 0;
}

int ps3scale_webman_start_mounted_game(void)
{
    char response[1024];

    /* Sound popup is best-effort; launching the mounted title is the key action. */
    (void)webman_http_get(
        "/popup.ps3?PS3%20SCALE%20-%20Starting%20game&snd=1",
        response, sizeof(response));
    sleep(1);

    return webman_http_get("/play.ps3", response, sizeof(response));
}
