#pragma once
#include <stddef.h>
#include <stdint.h>

#define GHC_GAME_MAGIC "GCP1"
#define GHC_GAME_VERSION 1u
#define GHC_GAME_HEADER_SIZE 8u
#define GHC_GAME_MAX_PACKET_SIZE 64u

typedef enum {
    GHC_GAME_PLAYER_STATE = 1,
    GHC_GAME_PLAYER_ACTION = 2,
    GHC_GAME_DAMAGE_EVENT = 3,
    GHC_GAME_REVIVE_EVENT = 4,
    GHC_GAME_MISSION_EVENT = 5,
    GHC_GAME_HEARTBEAT = 6
} GhcGameMessageType;

enum {
    GHC_MOVE_FORWARD = 1u << 0,
    GHC_MOVE_BACKWARD = 1u << 1,
    GHC_MOVE_LEFT = 1u << 2,
    GHC_MOVE_RIGHT = 1u << 3,
    GHC_MOVE_JUMP = 1u << 4,
    GHC_MOVE_CROUCH = 1u << 5,
    GHC_MOVE_SPRINT = 1u << 6,
    GHC_MOVE_AIM = 1u << 7,
    GHC_MOVE_FIRE = 1u << 8
};

typedef enum {
    GHC_STANCE_STANDING = 0,
    GHC_STANCE_CROUCHED = 1,
    GHC_STANCE_PRONE = 2,
    GHC_STANCE_DOWNED = 3
} GhcGameStance;

typedef enum {
    GHC_ACTION_FIRE = 1,
    GHC_ACTION_RELOAD = 2,
    GHC_ACTION_SWITCH_WEAPON = 3,
    GHC_ACTION_AIM_START = 4,
    GHC_ACTION_AIM_STOP = 5,
    GHC_ACTION_MELEE = 6,
    GHC_ACTION_JUMP = 7,
    GHC_ACTION_INTERACT = 8,
    GHC_ACTION_GRENADE = 9
} GhcGameAction;

typedef enum {
    GHC_REVIVE_START = 1,
    GHC_REVIVE_PROGRESS = 2,
    GHC_REVIVE_COMPLETE = 3,
    GHC_REVIVE_INTERRUPTED = 4,
    GHC_REVIVE_FAILED = 5
} GhcGameRevivePhase;

typedef enum {
    GHC_MISSION_CHECKPOINT_REACHED = 1,
    GHC_MISSION_OBJECTIVE_UPDATE = 2,
    GHC_MISSION_FAILED = 3,
    GHC_MISSION_COMPLETE = 4,
    GHC_MISSION_RESTART = 5
} GhcGameMissionEventType;

typedef struct {
    uint8_t slot;
    uint16_t actor_id;
    uint32_t game_tick;
    int32_t position_cm[3];
    int16_t velocity_cms[3];
    uint16_t yaw_centidegrees;
    int16_t pitch_centidegrees;
    uint16_t movement_flags;
    uint8_t stance;
    uint16_t weapon_id;
    uint16_t ammo_clip;
    uint16_t ammo_reserve;
    uint8_t health;
    uint8_t armor;
} GhcGamePlayerState;

typedef struct {
    uint8_t slot;
    uint8_t action;
    uint16_t actor_id;
    uint16_t target_actor_id;
    uint16_t weapon_id;
    uint16_t value;
    uint32_t game_tick;
} GhcGamePlayerAction;

typedef struct {
    uint32_t event_id;
    uint8_t attacker_slot; /* 0 = AI/world, 1/2 = player */
    uint8_t victim_slot;
    uint16_t damage;
    uint8_t damage_type;
    uint8_t hit_location;
    uint8_t resulting_health;
    uint8_t flags;
    uint32_t game_tick;
} GhcGameDamageEvent;

typedef struct {
    uint32_t event_id;
    uint8_t reviver_slot;
    uint8_t downed_slot;
    uint8_t phase;
    uint16_t progress_ms;
    uint16_t remaining_ms;
    uint8_t result;
    uint32_t game_tick;
} GhcGameReviveEvent;

typedef struct {
    uint32_t event_id;
    uint8_t event_type;
    uint8_t state;
    uint16_t checkpoint_id;
    uint16_t objective_id;
    uint32_t game_tick;
} GhcGameMissionEvent;

typedef struct {
    uint32_t game_tick;
    uint16_t ping_ms;
    uint8_t slot;
    uint8_t status;
} GhcGameHeartbeat;

typedef struct {
    uint8_t type;
    union {
        GhcGamePlayerState player_state;
        GhcGamePlayerAction player_action;
        GhcGameDamageEvent damage_event;
        GhcGameReviveEvent revive_event;
        GhcGameMissionEvent mission_event;
        GhcGameHeartbeat heartbeat;
    } data;
} GhcGameMessage;

/* Fixed-size, versioned, big-endian payload codec carried inside GHC2. */
int ghc_game_encode_message(
    const GhcGameMessage *message,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
);

int ghc_game_decode_message(
    const uint8_t *packet,
    size_t packet_length,
    GhcGameMessage *message
);
