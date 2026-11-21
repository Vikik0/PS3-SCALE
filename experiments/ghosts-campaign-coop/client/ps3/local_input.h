#ifndef GHC_LOCAL_INPUT_H
#define GHC_LOCAL_INPUT_H

#include <stdint.h>

#define GHC_LOCAL_INPUT_PLAYER_COUNT 2u
#define GHC_PAD_BUTTON_DATA_COUNT 64u
#define GHC_LOCAL_STICK_DEADZONE_DEFAULT 20u

typedef enum {
    GHC_LOCAL_INPUT_OK = 0,
    GHC_LOCAL_INPUT_ERROR_ARGUMENT = -1,
    GHC_LOCAL_INPUT_ERROR_DATA = -2,
    GHC_LOCAL_INPUT_ERROR_PAD_INFO = -3
} GhcLocalInputResult;

enum {
    GHC_LOCAL_BUTTON_UP       = 1u << 0,
    GHC_LOCAL_BUTTON_DOWN     = 1u << 1,
    GHC_LOCAL_BUTTON_LEFT     = 1u << 2,
    GHC_LOCAL_BUTTON_RIGHT    = 1u << 3,
    GHC_LOCAL_BUTTON_START    = 1u << 4,
    GHC_LOCAL_BUTTON_SELECT   = 1u << 5,
    GHC_LOCAL_BUTTON_L3       = 1u << 6,
    GHC_LOCAL_BUTTON_R3       = 1u << 7,
    GHC_LOCAL_BUTTON_SQUARE   = 1u << 8,
    GHC_LOCAL_BUTTON_CROSS    = 1u << 9,
    GHC_LOCAL_BUTTON_CIRCLE   = 1u << 10,
    GHC_LOCAL_BUTTON_TRIANGLE = 1u << 11,
    GHC_LOCAL_BUTTON_R1       = 1u << 12,
    GHC_LOCAL_BUTTON_L1       = 1u << 13,
    GHC_LOCAL_BUTTON_R2       = 1u << 14,
    GHC_LOCAL_BUTTON_L2       = 1u << 15
};

typedef struct {
    uint8_t connected;
    uint8_t port;
    uint8_t left_x;
    uint8_t left_y;
    uint8_t right_x;
    uint8_t right_y;
    uint8_t l2_pressure;
    uint8_t r2_pressure;
    uint16_t digital1;
    uint16_t digital2;
    uint32_t buttons;
} GhcLocalPlayerInput;

typedef struct {
    uint8_t connected_mask;
    uint8_t connected_count;
    uint8_t read_error_mask;
    uint8_t reserved;
    GhcLocalPlayerInput players[GHC_LOCAL_INPUT_PLAYER_COUNT];
} GhcLocalInputSnapshot;

typedef struct {
    uint8_t connected;
    uint8_t port;
    int8_t left_x;
    int8_t left_y;
    int8_t right_x;
    int8_t right_y;
    uint8_t l2_pressure;
    uint8_t r2_pressure;
    uint32_t buttons;
    uint32_t pressed;
    uint32_t released;
} GhcLocalPlayerFrame;

typedef struct {
    uint8_t connected_mask;
    uint8_t valid_mask;
    uint8_t read_error_mask;
    uint8_t reserved;
    GhcLocalPlayerFrame players[GHC_LOCAL_INPUT_PLAYER_COUNT];
} GhcLocalInputFrame;

typedef struct {
    uint8_t previous_connected_mask;
    uint8_t reserved[3];
    GhcLocalPlayerFrame previous_players[GHC_LOCAL_INPUT_PLAYER_COUNT];
} GhcLocalInputTracker;

/* Pure decoder. It has no PS3 SDK dependency and is host-testable. */
int ghc_local_input_decode_port(
    uint8_t port,
    uint8_t connected,
    int32_t data_length,
    const uint16_t *button_data,
    GhcLocalPlayerInput *output
);

void ghc_local_input_snapshot_clear(GhcLocalInputSnapshot *snapshot);

/* Converts raw snapshots into per-frame button edges and normalized sticks. */
void ghc_local_input_tracker_reset(GhcLocalInputTracker *tracker);
int ghc_local_input_tracker_update(
    const GhcLocalInputSnapshot *snapshot,
    GhcLocalInputTracker *tracker,
    uint8_t stick_deadzone,
    GhcLocalInputFrame *frame
);

/*
 * PS3 adapter. It reads only ports 0 and 1.
 * It deliberately does not call cellPadInit/cellPadEnd: Ghosts owns the
 * controller subsystem lifecycle inside the running game process.
 */
int ghc_local_input_poll_ps3(GhcLocalInputSnapshot *snapshot);

#endif
