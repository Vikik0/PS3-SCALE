#include "../client/ps3/ghc_protocol.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    uint8_t relay_id[GHC_RELAY_ID_SIZE];
    uint8_t relay_key[GHC_RELAY_KEY_SIZE];
    uint8_t output[GHC_MAX_DATAGRAM];
    size_t output_length = 0;
    static const uint8_t payload[] = {'t', 'e', 's', 't'};
    static const uint8_t expected_mac[GHC_MAC_SIZE] = {
        0x2a, 0x77, 0xb7, 0x6f, 0xda, 0x59, 0x3d, 0x5e,
        0x75, 0xdb, 0xca, 0x83, 0x5d, 0x78, 0xa7, 0x0e
    };
    static const uint8_t server_packet[] = {
        0x47, 0x48, 0x43, 0x32, 0x02, 0x01, 0x02, 0x03, 0x04,
        0x70, 0x65, 0x65, 0x72,
        0xba, 0x60, 0xf8, 0xc9, 0x88, 0xd0, 0xf0, 0x85,
        0x5a, 0x12, 0x17, 0x63, 0xd5, 0xdd, 0x9a, 0x72
    };
    GhcServerPacket decoded;
    uint8_t tampered_server_packet[sizeof(server_packet)];
    size_t i;

    for (i = 0; i < sizeof(relay_id); ++i) relay_id[i] = (uint8_t)i;
    memset(relay_key, 0x4b, sizeof(relay_key));

    assert(ghc_encode_client_packet(
        relay_id, relay_key, 0x01020304u, payload, sizeof(payload),
        output, sizeof(output), &output_length
    ) == 0);
    assert(output_length == GHC_CLIENT_OVERHEAD + sizeof(payload));
    assert(memcmp(output, "GHC2", 4) == 0);
    assert(memcmp(output + 4, relay_id, sizeof(relay_id)) == 0);
    assert(memcmp(output + 24, payload, sizeof(payload)) == 0);
    assert(memcmp(output + output_length - GHC_MAC_SIZE, expected_mac, GHC_MAC_SIZE) == 0);

    assert(ghc_decode_server_packet(
        relay_key, server_packet, sizeof(server_packet), &decoded
    ) == 0);
    assert(decoded.sender_slot == GHC_PLAYER_TWO);
    assert(decoded.sequence == 0x01020304u);
    assert(decoded.payload_length == 4);
    assert(memcmp(decoded.payload, "peer", 4) == 0);

    memcpy(tampered_server_packet, server_packet, sizeof(server_packet));
    tampered_server_packet[sizeof(tampered_server_packet) - 1] ^= 0x01;
    assert(ghc_decode_server_packet(
        relay_key, tampered_server_packet, sizeof(tampered_server_packet), &decoded
    ) != 0);

    return 0;
}
