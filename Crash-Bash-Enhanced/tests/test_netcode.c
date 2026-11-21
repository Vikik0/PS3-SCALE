#include "cb_netcode.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    CBNetInput sent = { 0x10203040u, 0xA0B0C0D0u, 0xA55Au, 0x1234u };
    CBNetInput received;
    uint8_t packet[CB_NET_PACKET_BYTES];
    uint8_t damaged[CB_NET_PACKET_BYTES];

    memset(&received, 0, sizeof(received));
    assert(cb_net_encode_input(&sent, packet, sizeof(packet)) == 0);
    assert(memcmp(packet, "CBIN", 4) == 0);
    assert(cb_net_decode_input(packet, sizeof(packet), &received) == 0);
    assert(received.sequence == sent.sequence);
    assert(received.frame == sent.frame);
    assert(received.buttons == sent.buttons);
    assert(received.acknowledged_sequence == sent.acknowledged_sequence);

    assert(cb_net_decode_input(packet, sizeof(packet) - 1, &received) == -1);
    assert(cb_net_decode_input(packet, sizeof(packet), NULL) == -1);
    assert(cb_net_encode_input(&sent, packet, sizeof(packet) - 1) == -1);

    memcpy(damaged, packet, sizeof(damaged));
    damaged[16] ^= 0x01u;
    assert(cb_net_decode_input(damaged, sizeof(damaged), &received) == -1);

    memcpy(damaged, packet, sizeof(damaged));
    damaged[4] = 99u;
    assert(cb_net_decode_input(damaged, sizeof(damaged), &received) == -1);

    assert(cb_net_crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u);
    puts("deterministic input packet codec tests passed.");
    return 0;
}
