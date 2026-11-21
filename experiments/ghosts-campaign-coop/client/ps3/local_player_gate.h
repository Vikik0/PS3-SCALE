#ifndef GHC_LOCAL_PLAYER_GATE_H
#define GHC_LOCAL_PLAYER_GATE_H

#include <stdint.h>

#define GHC_LOCAL_PLAYER_SLOT_COUNT 2u
#define GHC_LOCAL_PLAYER_NO_SLOT 0xffu

typedef enum {
    GHC_LOCAL_ROLE_UNKNOWN = 0,
    GHC_LOCAL_ROLE_LOGAN = 1,
    GHC_LOCAL_ROLE_GUEST_SOLDIER = 2
} GhcLocalPlayerRole;

typedef enum {
    GHC_LOCAL_GATE_READY = 0,
    GHC_LOCAL_GATE_INVALID_ARGUMENT = 1,
    GHC_LOCAL_GATE_SPLITSCREEN_INACTIVE = 2,
    GHC_LOCAL_GATE_NOT_SPLITSCREEN_PLAYER = 3,
    GHC_LOCAL_GATE_PRIMARY_MISMATCH = 4,
    GHC_LOCAL_GATE_CONTROLLER_MISMATCH = 5,
    GHC_LOCAL_GATE_INPUT_NOT_CONNECTED = 6,
    GHC_LOCAL_GATE_ROLE_MISMATCH = 7,
    GHC_LOCAL_GATE_CLIENT_ID_MISSING = 8,
    GHC_LOCAL_GATE_ENTITY_ID_MISSING = 9,
    GHC_LOCAL_GATE_STATE_ADDRESS_MISSING = 10,
    GHC_LOCAL_GATE_LOCAL_CLIENT_NUM_MISSING = 11,
    GHC_LOCAL_GATE_DUPLICATE_LOCAL_CLIENT_NUM = 12,
    GHC_LOCAL_GATE_DUPLICATE_CLIENT_ID = 13,
    GHC_LOCAL_GATE_DUPLICATE_ENTITY_ID = 14,
    GHC_LOCAL_GATE_DUPLICATE_STATE_ADDRESS = 15
} GhcLocalPlayerGateStatus;

typedef struct {
    uint8_t split_screen_active;
    uint8_t is_split_screen_player;
    uint8_t is_primary;
    uint8_t controller_port;
    uint8_t input_connected;
    uint8_t client_id_valid;
    uint8_t entity_id_valid;
    uint8_t local_client_num_valid;
    uint16_t client_id;
    uint16_t entity_id;
    uint8_t local_client_num;
    uint8_t role;
    uint8_t reserved[6];
    uint64_t gclient_address;
    uint64_t gentity_address;
    uint64_t player_state_address;
} GhcLocalPlayerObservation;

typedef struct {
    uint8_t ready;
    uint8_t failing_slot;
    uint8_t status;
    uint8_t reserved;
} GhcLocalPlayerGateResult;

/*
 * Pure safety gate. Observations must come from verified engine state; this
 * function does not read memory or create players. Slot 0 must be Logan on
 * controller port 0 and the primary local player. Slot 1 must be the separate
 * guest soldier on controller port 1 and the secondary local player.
 */
GhcLocalPlayerGateStatus ghc_local_player_gate_validate(
    const GhcLocalPlayerObservation players[GHC_LOCAL_PLAYER_SLOT_COUNT],
    GhcLocalPlayerGateResult *result
);

#endif
