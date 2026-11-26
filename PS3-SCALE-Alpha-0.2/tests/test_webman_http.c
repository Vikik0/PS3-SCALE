#define _POSIX_C_SOURCE 200809L

#include "ps3scale/webman.h"

#include <arpa/inet.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define TEST_PORT 19876

typedef struct {
    int listener;
    int delay_ms;
    const char *first;
    const char *second;
    int split_delay_ms;
    char request[2048];
} mock_server_t;

static void sleep_ms(int milliseconds)
{
    struct timespec delay;
    delay.tv_sec = milliseconds / 1000;
    delay.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
    (void)nanosleep(&delay, NULL);
}

static void send_all(int fd, const char *data)
{
    size_t sent = 0;
    size_t length = strlen(data);
    while (sent < length) {
        ssize_t result = send(fd, data + sent, length - sent, 0);
        if (result <= 0)
            return;
        sent += (size_t)result;
    }
}

static void *serve_once(void *argument)
{
    mock_server_t *server = (mock_server_t *)argument;
    int client = accept(server->listener, NULL, NULL);
    size_t used = 0;

    if (client < 0)
        return NULL;
    while (used + 1 < sizeof(server->request)) {
        ssize_t count = recv(client, server->request + used,
                             sizeof(server->request) - used - 1, 0);
        if (count <= 0)
            break;
        used += (size_t)count;
        server->request[used] = '\0';
        if (strstr(server->request, "\r\n\r\n"))
            break;
    }

    if (server->delay_ms > 0)
        sleep_ms(server->delay_ms);
    if (server->first)
        send_all(client, server->first);
    if (server->second) {
        if (server->split_delay_ms > 0)
            sleep_ms(server->split_delay_ms);
        send_all(client, server->second);
    }
    (void)shutdown(client, SHUT_RDWR);
    close(client);
    return NULL;
}

static void start_server(mock_server_t *server, pthread_t *thread)
{
    struct sockaddr_in address;
    int reuse = 1;

    memset(server, 0, sizeof(*server));
    server->listener = socket(AF_INET, SOCK_STREAM, 0);
    assert(server->listener >= 0);
    assert(setsockopt(server->listener, SOL_SOCKET, SO_REUSEADDR,
                      &reuse, sizeof(reuse)) == 0);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(TEST_PORT);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(server->listener, (struct sockaddr *)&address,
                sizeof(address)) == 0);
    assert(listen(server->listener, 1) == 0);
    assert(pthread_create(thread, NULL, serve_once, server) == 0);
}

static void finish_server(mock_server_t *server, pthread_t thread)
{
    assert(pthread_join(thread, NULL) == 0);
    close(server->listener);
}

static ps3scale_game_t installed_game(void)
{
    ps3scale_game_t game;
    memset(&game, 0, sizeof(game));
    game.kind = PS3SCALE_GAME_PS3_HDD;
    snprintf(game.title_id, sizeof(game.title_id), "BLES00891");
    snprintf(game.path, sizeof(game.path), "/dev_hdd0/game/BLES00891");
    snprintf(game.title, sizeof(game.title), "Driver San Francisco");
    return game;
}

static void test_http_200_launch(void)
{
    static const char response[] =
        "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    mock_server_t server;
    pthread_t thread;
    ps3scale_game_t game = installed_game();

    start_server(&server, &thread);
    server.first = response;
    assert(ps3scale_webman_start_game(&game) == 0);
    finish_server(&server, thread);
    if (!strstr(server.request, "GET /play.ps3?BLES00891 HTTP/1.0")) {
        fprintf(stderr, "Unexpected webMAN launch request: [%s]\\n", server.request);
        assert(!"webMAN launch request path mismatch");
    }
}

static void test_redirect_is_not_launch_success(void)
{
    static const char response[] =
        "HTTP/1.1 302 Found\r\nLocation: /index.ps3\r\n"
        "Content-Length: 0\r\nConnection: close\r\n\r\n";
    mock_server_t server;
    pthread_t thread;
    ps3scale_game_t game = installed_game();

    start_server(&server, &thread);
    server.first = response;
    assert(ps3scale_webman_start_game(&game) ==
           PS3SCALE_WEBMAN_ERROR_HTTP_STATUS);
    finish_server(&server, thread);
}

static void test_ps3mapi_json_split_across_packets(void)
{
    static const char first[] =
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
        "Content-Length: 14\r\nConnection: close\r\n\r\n"
        "{\"response\":";
    static const char second[] = "0}";
    mock_server_t server;
    pthread_t thread;

    start_server(&server, &thread);
    server.first = first;
    server.second = second;
    server.split_delay_ms = 25;
    assert(ps3scale_webman_ps3mapi_available() == 0);
    finish_server(&server, thread);
    assert(strstr(server.request, "GET /ps3mapi.ps3?SERVER%20GETVERSION HTTP/1.0") != NULL);
}

static long elapsed_ms(const struct timespec *start, const struct timespec *end)
{
    return (long)(end->tv_sec - start->tv_sec) * 1000L +
           (long)(end->tv_nsec - start->tv_nsec) / 1000000L;
}

static void test_missing_response_is_bounded(void)
{
    mock_server_t server;
    pthread_t thread;
    ps3scale_game_t game = installed_game();
    struct timespec start, end;
    int result;

    start_server(&server, &thread);
    server.delay_ms = 3000;
    (void)clock_gettime(CLOCK_MONOTONIC, &start);
    result = ps3scale_webman_start_game(&game);
    (void)clock_gettime(CLOCK_MONOTONIC, &end);
    assert(result == PS3SCALE_WEBMAN_ERROR_RESPONSE);
    assert(elapsed_ms(&start, &end) < 2200L);
    finish_server(&server, thread);
}

int main(void)
{
    test_http_200_launch();
    test_redirect_is_not_launch_success();
    test_ps3mapi_json_split_across_packets();
    test_missing_response_is_bounded();
    puts("webMAN HTTP transport integration tests: PASS (4 cases)");
    return 0;
}
