#ifndef CB_WS_CLIENT_H
#define CB_WS_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#define CB_WS_MAX_FRAME 28672
#define CB_WS_OPCODE_TEXT 1
#define CB_WS_OPCODE_BINARY 2

typedef struct CbWsClient {
    int fd;
    uint8_t rx_buffer[32768];
    size_t rx_start;
    size_t rx_end;
    uint32_t rng_state;
    int open;
} CbWsClient;

/* Minimal RFC 6455 client for ws:// endpoints (plain TCP, no TLS). */
int cb_ws_connect(CbWsClient *client, const char *host, unsigned short port,
                  const char *path, const char *subprotocols, int timeout_ms);
int cb_ws_send_binary(CbWsClient *client, const void *payload, size_t length);
int cb_ws_send_text(CbWsClient *client, const char *payload, size_t length);

/* Returns 1 for a text/binary message, 0 for timeout/control frame, -1 on close/error. */
int cb_ws_receive(CbWsClient *client, void *payload, size_t capacity,
                  size_t *length, int *opcode, int timeout_ms);
void cb_ws_close(CbWsClient *client);

#endif
