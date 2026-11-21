#include "relay_client.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    CbRelayClient client;
    CbRelayBatchQueue coalesce_queue;
    const uint8_t input_old[] = { 0x43, 0x42, 0x4e, 0x31, 0x08, 0x01 };
    const uint8_t input_new[] = { 0x43, 0x42, 0x4e, 0x31, 0x08, 0x02, 0x03 };
    const uint8_t record[] = { 0x43, 0x42, 0x4e, 0x31, 0x09, 0x04 };
    const uint8_t sent[] = { 0x43, 0x42, 0x01, 0x02, 0x03, 0x04 };
    const uint8_t batch_a[] = { 0x10, 0x11, 0x12 };
    const uint8_t batch_b[] = { 0x20, 0x21 };
    const uint8_t expected_sender[16] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
    };
    const uint8_t expected_host[16] = {
        0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88,
        0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00
    };
    uint8_t mapped_peer[16];
    uint32_t host_address_network;
    unsigned short host_port_host;
    CbRelayPacket batch[2];
    CbRelayBatchQueue queue;
    uint8_t recipient_a[16] = { 0x11 };
    uint8_t recipient_b[16] = { 0x22 };
    uint8_t received[64];
    uint8_t sender[16];
    size_t received_length = 0;
    char control[128];
    int result;

    memset(&client, 0, sizeof(client));
    memset(&coalesce_queue, 0, sizeof(coalesce_queue));
    if (cb_relay_queue_packet(&coalesce_queue, recipient_a, input_old, sizeof(input_old)) != 0 ||
        cb_relay_queue_packet(&coalesce_queue, recipient_a, input_new, sizeof(input_new)) != 0 ||
        coalesce_queue.count != 1u ||
        coalesce_queue.packets[0].length != sizeof(input_new) ||
        memcmp(coalesce_queue.packets[0].payload, input_new, sizeof(input_new)) != 0 ||
        cb_relay_queue_packet(&coalesce_queue, recipient_b, input_new, sizeof(input_new)) != 0 ||
        cb_relay_queue_packet(&coalesce_queue, recipient_a, record, sizeof(record)) != 0 ||
        coalesce_queue.count != 3u) {
        fprintf(stderr, "PS3 relay retransmission coalescing failed\n");
        return 1;
    }

    if (cb_relay_join(&client, "localhost", 8766, "ABC123",
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
            strncmp(host_address, "10.", 3) != 0 || host_port == 0) {
            fprintf(stderr, "PS3 relay host virtual address failed\n");
            cb_relay_leave(&client);
            return 3;
        }
    }

    result = cb_relay_receive_packet(&client, received, sizeof(received),
                                     &received_length, sender, control,
                                     sizeof(control), 3000);
    if (result != 2 || strncmp(control, "{\"type\":\"room_update\"", 21u) != 0) {
        fprintf(stderr, "PS3 relay initial room roster control failed\n");
        cb_relay_leave(&client);
        return 3;
    }
    if (cb_relay_session_address(expected_host, &host_address_network,
                                 &host_port_host) != 0 ||
        cb_relay_peer_for_address(&client, host_address_network, host_port_host,
                                  mapped_peer) != 0 ||
        memcmp(mapped_peer, expected_host, sizeof(mapped_peer)) != 0) {
        fprintf(stderr, "PS3 relay room roster did not map the host session\n");
        cb_relay_leave(&client);
        return 3;
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
        fprintf(stderr,
            "PS3 relay CBR1 decode failed: result=%d length=%lu expected=%lu first=%02x%02x%02x%02x sender=%02x%02x%02x%02x\\n",
            result, (unsigned long)received_length, (unsigned long)sizeof(sent),
            received[0], received[1], received[2], received[3],
            sender[0], sender[1], sender[2], sender[3]);
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

    memset(&queue, 0, sizeof(queue));
    for (int frame = 0; frame < 7; ++frame) {
        uint8_t value = (uint8_t)(0x40 + frame);
        if (cb_relay_queue_packet(&queue, recipient_a, &value, 1) != 0) {
            fprintf(stderr, "PS3 relay frame queue failed\n");
            cb_relay_leave(&client);
            return 8;
        }
        result = cb_relay_batch_tick(&client, &queue, 7);
        if (result != 0 || (frame < 6 && queue.count != (size_t)(frame + 1)) ||
            (frame == 6 && queue.count != 0)) {
            fprintf(stderr, "PS3 relay seven-frame batch cadence failed\n");
            cb_relay_leave(&client);
            return 9;
        }
    }

    for (int frame = 0; frame < 7; ++frame) {
        result = cb_relay_receive_packet(&client, received, sizeof(received),
                                         &received_length, sender, control,
                                         sizeof(control), 3000);
        if (result != 1 || received_length != 1 ||
            received[0] != (uint8_t)(0x40 + frame) ||
            memcmp(sender, expected_sender, sizeof(sender)) != 0) {
            fprintf(stderr, "PS3 relay queued datagram decode failed\n");
            cb_relay_leave(&client);
            return 10;
        }
    }
    result = cb_relay_receive_packet(&client, received, sizeof(received),
                                     &received_length, sender, control,
                                     sizeof(control), 3000);
    if (result != 2 || strcmp(control, "BATCH:7") != 0) {
        fprintf(stderr, "PS3 relay did not send seven datagrams in one batch\n");
        cb_relay_leave(&client);
        return 11;
    }

    cb_relay_leave(&client);
    if (client.connected) {
        fprintf(stderr, "PS3 relay leave did not close the client\n");
        return 12;
    }

    if (cb_relay_host(&client, "localhost", 8766, "PS3 Host", 3000) != 0 ||
        !client.connected || strcmp(client.room_code, "HOST42") != 0 ||
        strcmp(client.session_id, "ffeeddccbbaa99887766554433221100") != 0 ||
        strcmp(client.host_session_id, client.session_id) != 0) {
        fprintf(stderr, "PS3 host-room creation and ticket connection failed\n");
        cb_relay_leave(&client);
        return 13;
    }
    if (cb_relay_start_match(&client, 3000) != 0) {
        fprintf(stderr, "PS3 host START command failed\n");
        cb_relay_leave(&client);
        return 14;
    }
    if (cb_relay_session_address(expected_sender, &host_address_network,
                                 &host_port_host) != 0 ||
        cb_relay_peer_for_address(&client, host_address_network, host_port_host,
                                  mapped_peer) != 0 ||
        memcmp(mapped_peer, expected_sender, sizeof(mapped_peer)) != 0) {
        fprintf(stderr, "PS3 host did not learn the joined guest from room_update\n");
        cb_relay_leave(&client);
        return 14;
    }

    cb_relay_leave(&client);
    if (client.connected) {
        fprintf(stderr, "PS3 host relay leave did not close the client\n");
        return 14;
    }

    puts("PS3 host/guest ticket, CBU1 batching, and CBR1 relay tests passed.");
    return 0;
}
