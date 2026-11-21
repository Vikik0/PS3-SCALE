#include "ws_client.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    CbWsClient client;
    const uint8_t sent_binary[] = { 0x01, 0x23, 0x45, 0x67, 0x89 };
    uint8_t received[64];
    size_t received_length = 0;
    int opcode = 0;
    int result;

    memset(&client, 0, sizeof(client));
    if (cb_ws_connect(&client, "127.0.0.1", 8765, "/test",
                      "cbash-v1", 2000) != 0) {
        fprintf(stderr, "WebSocket handshake failed\n");
        return 1;
    }

    if (cb_ws_send_binary(&client, sent_binary, sizeof(sent_binary)) != 0) {
        fprintf(stderr, "WebSocket binary send failed\n");
        cb_ws_close(&client);
        return 2;
    }

    result = cb_ws_receive(&client, received, sizeof(received),
                           &received_length, &opcode, 2000);
    if (result != 1 || opcode != CB_WS_OPCODE_BINARY ||
        received_length != sizeof(sent_binary) ||
        memcmp(received, sent_binary, sizeof(sent_binary)) != 0) {
        fprintf(stderr, "WebSocket binary echo mismatch\n");
        cb_ws_close(&client);
        return 3;
    }

    if (cb_ws_send_text(&client, "PING", 4) != 0) {
        fprintf(stderr, "WebSocket text send failed\n");
        cb_ws_close(&client);
        return 4;
    }

    result = cb_ws_receive(&client, received, sizeof(received),
                           &received_length, &opcode, 2000);
    if (result != 1 || opcode != CB_WS_OPCODE_TEXT ||
        received_length != 4 || memcmp(received, "PONG", 4) != 0) {
        fprintf(stderr, "WebSocket text echo mismatch\n");
        cb_ws_close(&client);
        return 5;
    }

    cb_ws_close(&client);
    puts("PS3 WebSocket client host test passed.");
    return 0;
}
