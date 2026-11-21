#include "mission_rules.h"

#include <string.h>

static uint32_t gh_elapsed(uint32_t now_ms, uint32_t start_ms) {
    return (uint32_t)(now_ms - start_ms);
}

static void gh_write_u32_be(uint8_t *out, uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

static uint32_t gh_read_u32_be(const uint8_t *in) {
    return ((uint32_t)in[0] << 24)
         | ((uint32_t)in[1] << 16)
         | ((uint32_t)in[2] << 8)
         | (uint32_t)in[3];
}

static int gh_valid_slot(uint8_t slot) {
    return slot == 1u || slot == 2u;
}

static GhcMissionPlayer *gh_player(GhcMission *mission, uint8_t slot) {
    return &mission->players[(size_t)(slot - 1u)];
}

static const GhcMissionPlayer *gh_const_player(const GhcMission *mission, uint8_t slot) {
    return &mission->players[(size_t)(slot - 1u)];
}

static void gh_fail(GhcMission *mission, uint8_t reason) {
    if (mission->phase == GHC_MISSION_FAILED) return;
    mission->phase = GHC_MISSION_FAILED;
    mission->failure_reason = reason;
    mission->revision++;
}

static int gh_both_downed(const GhcMission *mission) {
    return mission->players[0].life_state == GHC_PLAYER_DOWNED
        && mission->players[1].life_state == GHC_PLAYER_DOWNED;
}

void ghc_mission_init(GhcMission *mission, const GhcMissionConfig *config) {
    size_t i;
    if (!mission) return;

    mission->config.bleedout_ms =
        (config && config->bleedout_ms <= 0x7fffffffu && config->bleedout_ms)
        ? config->bleedout_ms : GHC_MISSION_DEFAULT_BLEEDOUT_MS;
    mission->config.revive_ms =
        (config && config->revive_ms <= 42949672u && config->revive_ms)
        ? config->revive_ms : GHC_MISSION_DEFAULT_REVIVE_MS;
    mission->config.revived_health_percent =
        (config && config->revived_health_percent > 0u && config->revived_health_percent <= 100u)
        ? config->revived_health_percent : GHC_MISSION_DEFAULT_REVIVED_HEALTH_PERCENT;

    mission->phase = GHC_MISSION_RUNNING;
    mission->failure_reason = GHC_MISSION_FAIL_NONE;
    mission->revision = 1u;

    for (i = 0; i < 2; ++i) {
        mission->players[i].life_state = GHC_PLAYER_ALIVE;
        mission->players[i].health_percent = 100u;
        mission->players[i].revive_active = 0u;
        mission->players[i].revive_by_slot = 0u;
        mission->players[i].downed_at_ms = 0u;
        mission->players[i].revive_started_at_ms = 0u;
    }
}

GhcMissionRuleResult ghc_mission_player_downed(
    GhcMission *mission,
    uint8_t player_slot,
    uint32_t now_ms
) {
    GhcMissionPlayer *player;
    if (!mission || !gh_valid_slot(player_slot)) return GHC_RULE_INVALID;
    if (mission->phase != GHC_MISSION_RUNNING) return GHC_RULE_NO_CHANGE;

    player = gh_player(mission, player_slot);
    if (player->life_state == GHC_PLAYER_DOWNED) return GHC_RULE_NO_CHANGE;

    player->life_state = GHC_PLAYER_DOWNED;
    player->health_percent = 0u;
    player->downed_at_ms = now_ms;
    player->revive_active = 0u;
    player->revive_by_slot = 0u;
    player->revive_started_at_ms = 0u;
    mission->revision++;

    if (gh_both_downed(mission)) {
        gh_fail(mission, GHC_MISSION_FAIL_BOTH_DOWN);
        return GHC_RULE_MISSION_FAILED;
    }
    return GHC_RULE_PLAYER_DOWNED;
}

GhcMissionRuleResult ghc_mission_tick(GhcMission *mission, uint32_t now_ms) {
    size_t i;
    if (!mission) return GHC_RULE_INVALID;
    if (mission->phase != GHC_MISSION_RUNNING) return GHC_RULE_NO_CHANGE;

    if (gh_both_downed(mission)) {
        gh_fail(mission, GHC_MISSION_FAIL_BOTH_DOWN);
        return GHC_RULE_MISSION_FAILED;
    }

    for (i = 0; i < 2; ++i) {
        const GhcMissionPlayer *player = &mission->players[i];
        if (player->life_state == GHC_PLAYER_DOWNED
            && gh_elapsed(now_ms, player->downed_at_ms) >= mission->config.bleedout_ms) {
            gh_fail(mission, GHC_MISSION_FAIL_BLEEDOUT);
            return GHC_RULE_MISSION_FAILED;
        }
    }
    return GHC_RULE_NO_CHANGE;
}

GhcMissionRuleResult ghc_mission_update_revive(
    GhcMission *mission,
    uint8_t reviver_slot,
    uint8_t target_slot,
    uint8_t interact_held,
    uint8_t in_range,
    uint8_t line_of_sight,
    uint32_t now_ms
) {
    GhcMissionPlayer *target;
    const GhcMissionPlayer *reviver;
    uint32_t elapsed;

    if (!mission || !gh_valid_slot(reviver_slot) || !gh_valid_slot(target_slot)
        || reviver_slot == target_slot) {
        return GHC_RULE_INVALID;
    }

    if (ghc_mission_tick(mission, now_ms) == GHC_RULE_MISSION_FAILED) {
        return GHC_RULE_MISSION_FAILED;
    }
    if (mission->phase != GHC_MISSION_RUNNING) return GHC_RULE_NO_CHANGE;

    reviver = gh_const_player(mission, reviver_slot);
    target = gh_player(mission, target_slot);

    if (reviver->life_state != GHC_PLAYER_ALIVE
        || target->life_state != GHC_PLAYER_DOWNED
        || !interact_held || !in_range || !line_of_sight) {
        if (target->revive_active) {
            target->revive_active = 0u;
            target->revive_by_slot = 0u;
            target->revive_started_at_ms = 0u;
            mission->revision++;
        }
        return GHC_RULE_NO_CHANGE;
    }

    if (!target->revive_active || target->revive_by_slot != reviver_slot) {
        target->revive_active = 1u;
        target->revive_by_slot = reviver_slot;
        target->revive_started_at_ms = now_ms;
        mission->revision++;
        return GHC_RULE_NO_CHANGE;
    }

    elapsed = gh_elapsed(now_ms, target->revive_started_at_ms);
    if (elapsed < mission->config.revive_ms) return GHC_RULE_NO_CHANGE;

    target->life_state = GHC_PLAYER_ALIVE;
    target->health_percent = mission->config.revived_health_percent;
    target->revive_active = 0u;
    target->revive_by_slot = 0u;
    target->downed_at_ms = 0u;
    target->revive_started_at_ms = 0u;
    mission->revision++;
    return GHC_RULE_PLAYER_REVIVED;
}

void ghc_mission_reset(GhcMission *mission) {
    GhcMissionConfig config;
    uint32_t next_revision;
    if (!mission) return;
    config = mission->config;
    next_revision = mission->revision + 1u;
    if (next_revision == 0u) next_revision = 1u;
    ghc_mission_init(mission, &config);
    mission->revision = next_revision;
}

int ghc_mission_get_view(const GhcMission *mission, uint32_t now_ms, GhcMissionView *view) {
    size_t i;
    if (!mission || !view) return -1;

    view->revision = mission->revision;
    view->phase = mission->phase;
    view->failure_reason = mission->failure_reason;

    for (i = 0; i < 2; ++i) {
        const GhcMissionPlayer *player = &mission->players[i];
        view->player_state[i] = player->life_state;
        view->health_percent[i] = player->health_percent;
        view->revive_progress_percent[i] = 0u;
        view->bleedout_remaining_ms[i] = 0u;

        if (player->life_state == GHC_PLAYER_DOWNED) {
            uint32_t elapsed = gh_elapsed(now_ms, player->downed_at_ms);
            view->bleedout_remaining_ms[i] =
                elapsed >= mission->config.bleedout_ms
                ? 0u : mission->config.bleedout_ms - elapsed;

            if (player->revive_active) {
                uint32_t revive_elapsed = gh_elapsed(now_ms, player->revive_started_at_ms);
                if (revive_elapsed >= mission->config.revive_ms) {
                    view->revive_progress_percent[i] = 100u;
                } else {
                    // revive_ms is capped at UINT32_MAX / 100 during init.
                    view->revive_progress_percent[i] = (uint8_t)(
                        (revive_elapsed * 100u) / mission->config.revive_ms
                    );
                }
            }
        }
    }
    return 0;
}

static int gh_valid_snapshot_view(const GhcMissionView *view) {
    size_t i;
    if (!view) return 0;
    if (view->phase > GHC_MISSION_FAILED
        || view->failure_reason > GHC_MISSION_FAIL_BLEEDOUT) return 0;
    if ((view->phase == GHC_MISSION_RUNNING
            && view->failure_reason != GHC_MISSION_FAIL_NONE)
        || (view->phase == GHC_MISSION_FAILED
            && view->failure_reason == GHC_MISSION_FAIL_NONE)) return 0;

    for (i = 0; i < 2; ++i) {
        if (view->player_state[i] > GHC_PLAYER_DOWNED
            || view->health_percent[i] > 100u
            || view->revive_progress_percent[i] > 100u) return 0;
        if (view->player_state[i] == GHC_PLAYER_DOWNED
            && view->health_percent[i] != 0u) return 0;
        if (view->player_state[i] == GHC_PLAYER_ALIVE
            && view->revive_progress_percent[i] != 0u) return 0;
    }
    return 1;
}

int ghc_mission_encode_snapshot(
    const GhcMissionView *view,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
) {
    if (output_length) *output_length = 0u;
    if (!view || !output || !output_length
        || output_capacity < GHC_MISSION_SNAPSHOT_SIZE
        || !gh_valid_snapshot_view(view)) return -1;

    output[0] = (uint8_t)'G';
    output[1] = (uint8_t)'M';
    output[2] = (uint8_t)'S';
    output[3] = GHC_MISSION_SNAPSHOT_VERSION;
    output[4] = view->phase;
    output[5] = view->failure_reason;
    gh_write_u32_be(output + 6, view->revision);
    output[10] = view->player_state[0];
    output[11] = view->health_percent[0];
    output[12] = view->revive_progress_percent[0];
    output[13] = view->player_state[1];
    output[14] = view->health_percent[1];
    output[15] = view->revive_progress_percent[1];
    gh_write_u32_be(output + 16, view->bleedout_remaining_ms[0]);
    gh_write_u32_be(output + 20, view->bleedout_remaining_ms[1]);
    *output_length = GHC_MISSION_SNAPSHOT_SIZE;
    return 0;
}

int ghc_mission_decode_snapshot(
    const uint8_t *input,
    size_t input_length,
    GhcMissionView *view
) {
    GhcMissionView decoded;
    if (!input || !view || input_length != GHC_MISSION_SNAPSHOT_SIZE) return -1;
    if (input[0] != (uint8_t)'G' || input[1] != (uint8_t)'M'
        || input[2] != (uint8_t)'S' || input[3] != GHC_MISSION_SNAPSHOT_VERSION) return -1;

    memset(&decoded, 0, sizeof(decoded));
    decoded.phase = input[4];
    decoded.failure_reason = input[5];
    decoded.revision = gh_read_u32_be(input + 6);
    decoded.player_state[0] = input[10];
    decoded.health_percent[0] = input[11];
    decoded.revive_progress_percent[0] = input[12];
    decoded.player_state[1] = input[13];
    decoded.health_percent[1] = input[14];
    decoded.revive_progress_percent[1] = input[15];
    decoded.bleedout_remaining_ms[0] = gh_read_u32_be(input + 16);
    decoded.bleedout_remaining_ms[1] = gh_read_u32_be(input + 20);

    if (!gh_valid_snapshot_view(&decoded)) return -1;
    *view = decoded;
    return 0;
}

int ghc_mission_revision_is_newer(uint32_t incoming_revision, uint32_t current_revision) {
    uint32_t difference = incoming_revision - current_revision;
    return difference != 0u && difference < 0x80000000u;
}
