#include "relay_client.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    CbRelayClient client;
    const uint8_t sent[] = { 0x43, 0x42, 0x01, 0x02, 0x03, 0x04 };
    const uint8_t expected_sender[16] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
    };
    uint8_t received[64];
    uint8_t sender[16];
    size_t received_length = 0;
    char control[128];
    int result;

    memset(&client, 0, sizeof(client));
    if (cb_relay_join(&client, "127.0.0.1", 8766, "ABC123",
                      "PS3 Player", 3000) != 0) {
        fprintf(stderr, "PS3 relay join/ticket handshake failed\n");
        return 1;
    }

    if (!client.connected || strcmp(client.room_code, "ABC123") != 0 ||
        strcmp(client.session_id, "00112233445566778899aabbccddeeff") != 0) {
        fprintf(stderr, "PS3 relay session metadata mismatch\n");
        cb_relay_leave(&client);
        return 2;
    }

    if (cb_relay_send_packet(&client, sent, sizeof(sent)) != 0) {
        fprintf(stderr, "PS3 relay binary send failed\n");
        cb_relay_leave(&client);
        return 3;
    }

    result = cb_relay_receive_packet(&client, received, sizeof(received),
                                     &received_length, sender, control,
                                     sizeof(control), 3000);
    if (result != 1 || received_length != sizeof(sent) ||
        memcmp(received, sent, sizeof(sent)) != 0 ||
        memcmp(sender, expected_sender, sizeof(sender)) != 0) {
        fprintf(stderr, "PS3 relay CBR1 envelope decode failed\n");
        cb_relay_leave(&client);
        return 4;
    }

    cb_relay_leave(&client);
    if (client.connected) {
        fprintf(stderr, "PS3 relay leave did not close the client\n");
        return 5;
    }

    puts("PS3 guest ticket and CBR1 relay integration test passed.");
    return 0;
}
