#include "ws_client.h"

#include <stdio.h>
#include <string.h>

static int run_echo_test(CbWsClient *client, int secure)
{
    const uint8_t sent_binary[] = { 0x01, 0x23, 0x45, 0x67, 0x89 };
    uint8_t received[64];
    size_t received_length = 0;
    int opcode = 0;
    int result;
    int connect_result;

    if (secure) {
        connect_result = cb_wss_connect(client, "localhost", 8767, "/test",
                                        "cbash-v1", 3000);
    } else {
        connect_result = cb_ws_connect(client, "127.0.0.1", 8765, "/test",
                                       "cbash-v1", 2000);
    }
    if (connect_result != 0) {
        fprintf(stderr, "%s handshake failed\n", secure ? "WSS" : "WS");
        return 1;
    }

    if (cb_ws_send_binary(client, sent_binary, sizeof(sent_binary)) != 0) {
        fprintf(stderr, "%s binary send failed\n", secure ? "WSS" : "WS");
        cb_ws_close(client);
        return 2;
    }

    result = cb_ws_receive(client, received, sizeof(received),
                           &received_length, &opcode, 3000);
    if (result != 1 || opcode != CB_WS_OPCODE_BINARY ||
        received_length != sizeof(sent_binary) ||
        memcmp(received, sent_binary, sizeof(sent_binary)) != 0) {
        fprintf(stderr, "%s binary echo mismatch\n", secure ? "WSS" : "WS");
        cb_ws_close(client);
        return 3;
    }

    if (!secure) {
        if (cb_ws_send_text(client, "PING", 4) != 0) {
            fprintf(stderr, "WS text send failed\n");
            cb_ws_close(client);
            return 4;
        }
        result = cb_ws_receive(client, received, sizeof(received),
                               &received_length, &opcode, 2000);
        if (result != 1 || opcode != CB_WS_OPCODE_TEXT ||
            received_length != 4 || memcmp(received, "PONG", 4) != 0) {
            fprintf(stderr, "WS text echo mismatch\n");
            cb_ws_close(client);
            return 5;
        }
    }

    cb_ws_close(client);
    return 0;
}

int main(void)
{
    CbWsClient client;
    int result;

    memset(&client, 0, sizeof(client));
    client.fd = -1;
    result = run_echo_test(&client, 0);
    if (result != 0) return result;

    memset(&client, 0, sizeof(client));
    client.fd = -1;
    result = run_echo_test(&client, 1);
    if (result != 0) return result;

    puts("PS3 WS/WSS client host tests passed.");
    return 0;
}
