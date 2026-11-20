#pragma once
#include <stdint.h>

// Shared wire constants. This header is a protocol contract, not a finished SPRX.
#define GHC_MAGIC "GHC1"
#define GHC_TOKEN_HEX_LENGTH 32
#define GHC_CLIENT_HEADER_SIZE 38
#define GHC_SERVER_HEADER_SIZE 7
#define GHC_MAX_DATAGRAM 1200
#define GHC_DEFAULT_UDP_PORT 28991

typedef enum {
    GHC_PLAYER_ONE = 1,
    GHC_PLAYER_TWO = 2
} GhcPlayerSlot;

// Client -> server:
// [4 bytes magic][32 ASCII hex token][2-byte big-endian sequence][opaque payload]
// Server -> peer:
// [4 bytes magic][1-byte sender slot][2-byte big-endian sequence][opaque payload]
