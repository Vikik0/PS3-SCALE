#pragma once
#include <stddef.h>
#include <stdint.h>

#define GHC_MAGIC "GHC2"
#define GHC_RELAY_ID_SIZE 16
#define GHC_RELAY_KEY_SIZE 32
#define GHC_MAC_SIZE 16
#define GHC_SEQUENCE_SIZE 4
#define GHC_CLIENT_HEADER_SIZE 24
#define GHC_SERVER_HEADER_SIZE 9
#define GHC_CLIENT_OVERHEAD 40
#define GHC_SERVER_OVERHEAD 25
#define GHC_MAX_DATAGRAM 1200
#define GHC_DEFAULT_UDP_PORT 28991

typedef enum {
    GHC_PLAYER_ONE = 1,
    GHC_PLAYER_TWO = 2
} GhcPlayerSlot;

typedef struct {
    uint8_t relay_id[GHC_RELAY_ID_SIZE];
    uint32_t sequence;
    const uint8_t *payload;
    size_t payload_length;
} GhcClientPacket;

typedef struct {
    uint8_t sender_slot;
    uint32_t sequence;
    const uint8_t *payload;
    size_t payload_length;
} GhcServerPacket;

// Encode a client-to-relay packet. Returns 0 on success and -1 on invalid input.
int ghc_encode_client_packet(
    const uint8_t relay_id[GHC_RELAY_ID_SIZE],
    const uint8_t relay_key[GHC_RELAY_KEY_SIZE],
    uint32_t sequence,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
);

// Authenticate and parse a peer/client packet using the peer's relay key.
// Payload points into packet; relay_id is copied into decoded.
int ghc_decode_client_packet(
    const uint8_t peer_relay_key[GHC_RELAY_KEY_SIZE],
    const uint8_t *packet,
    size_t packet_length,
    GhcClientPacket *decoded
);

// Authenticate and parse a relay-to-client packet. Payload points into packet.
int ghc_decode_server_packet(
    const uint8_t relay_key[GHC_RELAY_KEY_SIZE],
    const uint8_t *packet,
    size_t packet_length,
    GhcServerPacket *decoded
);
