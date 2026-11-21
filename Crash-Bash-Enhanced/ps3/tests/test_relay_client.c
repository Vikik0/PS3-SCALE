#include "relay_client.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    CbRelayClient client;
    const uint8_t sent[] = { 0x43, 0x42, 0x01, 0x02, 0x03, 0x04 };
    const uint8_t batch_a[] = { 0x10, 0x11, 0x12 };
    const uint8_t batch_b[] = { 0x20, 0x21 };
    const uint8_t expected_sender[16] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
    };
    CbRelayPacket batch[2];
    uint8_t recipient_a[16] = { 0x11 };
    uint8_t recipient_b[16] = { 0x22 };
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
        strcmp(client.session_id, "00112233445566778899aabbccddeeff") != 0 ||
        strcmp(client.host_session_id, "ffeeddccbbaa99887766554433221100") != 0) {
        fprintf(stderr, "PS3 relay session metadata mismatch\n");
        cb_relay_leave(&client);
        return 2;
    }
    {
        char host_address[32];
        unsigned short host_port = 0;
        if (cb_relay_host_address(&client, host_address, sizeof(host_address), &host_port) != 0 ||
            strncmp(host_address, "10.", 3) != 0 || host_port < 20000 || host_port > 59999) {
            fprintf(stderr, "PS3 relay host virtual address failed\n");
            cb_relay_leave(&client);
            return 3;
        }
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

    memset(batch, 0, sizeof(batch));
    memcpy(batch[0].recipient_id, recipient_a, sizeof(recipient_a));
    batch[0].payload = batch_a;
    batch[0].length = sizeof(batch_a);
    memcpy(batch[1].recipient_id, recipient_b, sizeof(recipient_b));
    batch[1].payload = batch_b;
    batch[1].length = sizeof(batch_b);
    if (cb_relay_send_batch(&client, batch, 2) != 0) {
        fprintf(stderr, "PS3 relay batch send failed\n");
        cb_relay_leave(&client);
        return 5;
    }

    result = cb_relay_receive_packet(&client, received, sizeof(received),
                                     &received_length, sender, control,
                                     sizeof(control), 3000);
    if (result != 1 || received_length != sizeof(batch_a) ||
        memcmp(received, batch_a, sizeof(batch_a)) != 0 ||
        memcmp(sender, expected_sender, sizeof(sender)) != 0) {
        fprintf(stderr, "PS3 relay first batch packet decode failed\n");
        cb_relay_leave(&client);
        return 6;
    }

    result = cb_relay_receive_packet(&client, received, sizeof(received),
                                     &received_length, sender, control,
                                     sizeof(control), 3000);
    if (result != 1 || received_length != sizeof(batch_b) ||
        memcmp(received, batch_b, sizeof(batch_b)) != 0 ||
        memcmp(sender, expected_sender, sizeof(sender)) != 0) {
        fprintf(stderr, "PS3 relay second batch packet decode failed\n");
        cb_relay_leave(&client);
        return 7;
    }

    cb_relay_leave(&client);
    if (client.connected) {
        fprintf(stderr, "PS3 relay leave did not close the client\n");
        return 5;
    }

    puts("PS3 guest ticket and CBR1 relay integration test passed.");
    return 0;
}
