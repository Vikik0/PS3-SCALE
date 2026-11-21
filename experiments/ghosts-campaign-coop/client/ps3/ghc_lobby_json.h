#pragma once
#include <stddef.h>
#include <stdint.h>

#define GHC_LOBBY_TOKEN_HEX_LENGTH 64
#define GHC_LOBBY_SESSION_ID_HEX_LENGTH 32
#define GHC_LOBBY_ROOM_CODE_LENGTH 6

typedef struct {
    char token[GHC_LOBBY_TOKEN_HEX_LENGTH + 1];
    char session_id[GHC_LOBBY_SESSION_ID_HEX_LENGTH + 1];
    uint8_t relay_id[16];
    uint8_t relay_key[32];
    uint32_t expires_in_seconds;
} GhcGuestSession;

/* Extract an ASCII JSON string property, including nested object properties. */
int ghc_json_get_string(
    const char *json,
    const char *key,
    char *output,
    size_t output_capacity
);

/* Extract an unsigned 32-bit integer property. */
int ghc_json_get_u32(const char *json, const char *key, uint32_t *value);

/* Decode exactly output_length bytes from a hex string. */
int ghc_hex_decode_exact(const char *hex, uint8_t *output, size_t output_length);

/* Parse and validate the guest-session response returned by /v1/sessions. */
int ghc_lobby_parse_guest_session(const char *json, GhcGuestSession *session);

/* Parse a six-character uppercase alphanumeric room code from a response. */
int ghc_lobby_parse_room_code(const char *json, char room_code[7]);


typedef struct {
    char public_address[16];
    uint16_t public_port;
    char local_address[16];
    uint16_t local_port;
    char nat_type[16];
} GhcNetworkCandidate;

typedef struct {
    uint8_t peer_present;
    uint8_t peer_ready;
    uint8_t room_running;
    uint8_t candidate_present;
    uint8_t peer_slot;
    char peer_session_id[GHC_LOBBY_SESSION_ID_HEX_LENGTH + 1];
    uint8_t relay_id[16];
    uint8_t relay_key[32];
    GhcNetworkCandidate candidate;
} GhcPeerSnapshot;

/* Parse /peer or /candidate response data. A missing peer is a valid state. */
int ghc_lobby_parse_peer_snapshot(const char *json, GhcPeerSnapshot *snapshot);
