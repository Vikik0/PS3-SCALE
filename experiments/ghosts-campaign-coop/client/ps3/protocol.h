#pragma once
#include <stdint.h>

// GHC2 authenticated relay protocol. Session bearer tokens are HTTP-only;
// the relay key is obtained from the HTTPS guest-session response.
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

// Client -> relay:
// [4 magic][16-byte relay ID][uint32 sequence, big endian][payload][16-byte HMAC-SHA256]
// Relay -> client:
// [4 magic][1-byte sender slot][uint32 sequence, big endian][payload][16-byte HMAC-SHA256]
//
// Both directions use the recipient's 32-byte relay key for HMAC-SHA256.
// HMAC input is the complete packet without the final 16-byte MAC tag.
// The server issues the relay ID/key through POST /v1/sessions over HTTPS.
// Sequence numbers use a 64-packet replay window and wrap at uint32.
