#pragma once
#include <stddef.h>
#include <stdint.h>

#define GHC_MISSION_DEFAULT_BLEEDOUT_MS 30000u
#define GHC_MISSION_DEFAULT_REVIVE_MS 5000u
#define GHC_MISSION_DEFAULT_REVIVED_HEALTH_PERCENT 40u
#define GHC_MISSION_SNAPSHOT_VERSION 1u
#define GHC_MISSION_SNAPSHOT_SIZE 24u

typedef enum {
    GHC_PLAYER_ALIVE = 0,
    GHC_PLAYER_DOWNED = 1
} GhcPlayerLifeState;

typedef enum {
    GHC_MISSION_RUNNING = 0,
    GHC_MISSION_FAILED = 1
} GhcMissionPhase;

typedef enum {
    GHC_MISSION_FAIL_NONE = 0,
    GHC_MISSION_FAIL_BOTH_DOWN = 1,
    GHC_MISSION_FAIL_BLEEDOUT = 2
} GhcMissionFailureReason;

typedef enum {
    GHC_RULE_INVALID = -1,
    GHC_RULE_NO_CHANGE = 0,
    GHC_RULE_PLAYER_DOWNED = 1,
    GHC_RULE_PLAYER_REVIVED = 2,
    GHC_RULE_MISSION_FAILED = 3
} GhcMissionRuleResult;

typedef struct {
    uint32_t bleedout_ms;
    uint32_t revive_ms;
    uint8_t revived_health_percent;
} GhcMissionConfig;

typedef struct {
    uint8_t life_state;
    uint8_t health_percent;
    uint8_t revive_active;
    uint8_t revive_by_slot;
    uint32_t downed_at_ms;
    uint32_t revive_started_at_ms;
} GhcMissionPlayer;

typedef struct {
    GhcMissionConfig config;
    GhcMissionPlayer players[2];
    uint8_t phase;
    uint8_t failure_reason;
    uint32_t revision;
} GhcMission;

typedef struct {
    uint32_t revision;
    uint8_t phase;
    uint8_t failure_reason;
    uint8_t player_state[2];
    uint8_t health_percent[2];
    uint8_t revive_progress_percent[2];
    uint32_t bleedout_remaining_ms[2];
} GhcMissionView;

// Initializes both players as alive with full health. Zero config fields use defaults.
void ghc_mission_init(GhcMission *mission, const GhcMissionConfig *config);

// Called by the game adapter when a player's Campaign actor enters the downed state.
GhcMissionRuleResult ghc_mission_player_downed(
    GhcMission *mission,
    uint8_t player_slot,
    uint32_t now_ms
);

// Called by the authoritative host every frame/tick. A bleedout ends the mission.
GhcMissionRuleResult ghc_mission_tick(GhcMission *mission, uint32_t now_ms);

// Called by the authoritative host while the living teammate holds the revive input.
// in_range and line_of_sight must be computed by the game adapter.
GhcMissionRuleResult ghc_mission_update_revive(
    GhcMission *mission,
    uint8_t reviver_slot,
    uint8_t target_slot,
    uint8_t interact_held,
    uint8_t in_range,
    uint8_t line_of_sight,
    uint32_t now_ms
);

// Clears the failed mission state after the game has reloaded the checkpoint/mission.
void ghc_mission_reset(GhcMission *mission);

// Produces a stable, endian-independent view suitable for a GHC2 network payload.
int ghc_mission_get_view(const GhcMission *mission, uint32_t now_ms, GhcMissionView *view);
int ghc_mission_encode_snapshot(
    const GhcMissionView *view,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
);
int ghc_mission_decode_snapshot(
    const uint8_t *input,
    size_t input_length,
    GhcMissionView *view
);


// Returns 1 only when incoming_revision is newer under uint32 wraparound rules.
int ghc_mission_revision_is_newer(uint32_t incoming_revision, uint32_t current_revision);
