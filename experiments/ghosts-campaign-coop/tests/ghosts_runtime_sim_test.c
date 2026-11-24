/*
 * Host-only Ghosts runtime contract simulator.
 *
 * This is NOT a Ghosts engine emulator and does not claim to prove Campaign
 * hooks work. It composes the real portable input, local-player gate and
 * mission-rule modules with a deterministic fake engine boundary. The goal is
 * to catch routing/state-machine regressions before any PRX is loaded.
 */
#include "../client/ps3/local_input.h"
#include "../client/ps3/local_player_gate.h"
#include "../client/ps3/mission_rules.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    FAULT_NONE = 0,
    FAULT_CROSS_INPUT,
    FAULT_SHARED_CAMERA,
    FAULT_SHARED_HUD,
    FAULT_AI_PLAYER_ONE_ONLY,
    FAULT_DROP_GUEST_ON_CHECKPOINT,
    FAULT_DUPLICATE_GUEST_ON_TRANSITION,
    FAULT_ALIAS_PLAYER_STATE,
    FAULT_SHARED_COMMAND_STREAM,
    FAULT_STALE_INPUT_FRAME,
    FAULT_MISMATCHED_OBSERVATION_FRAME,
    FAULT_MISSING_OBSERVATION_FRAME
} SimFault;

typedef struct {
    uint8_t active;
    uint8_t role;
    uint8_t health;
    uint8_t view_slot;
    uint16_t entity_id;
    int32_t x;
    uint64_t camera_context;
    uint64_t hud_context;
    uint64_t player_state_address;
    uint32_t cross_actions;
    uint32_t circle_actions;
} SimPlayer;

typedef struct {
    SimPlayer players[2];
    GhcLocalPlayerObservation observations[2];
    GhcLocalInputSnapshot snapshot;
    GhcLocalInputTracker tracker;
    GhcLocalInputFrame frame;
    GhcMission mission;
    SimFault fault;
    uint32_t mission_id;
    uint32_t actor_count;
    int32_t npc_x;
    uint8_t npc_target;
} SimRuntime;

typedef struct {
    SimPlayer players[2];
    uint32_t mission_id;
} SimCheckpoint;

static unsigned g_checks;
static unsigned g_failures;

#define CHECK(label, condition) do { \
    ++g_checks; \
    if (!(condition)) { \
        ++g_failures; \
        printf("FAIL: %s (line %d)\n", label, __LINE__); \
    } \
} while (0)

static void sim_init(SimRuntime *rt, SimFault fault)
{
    uint32_t i;
    memset(rt, 0, sizeof(*rt));
    rt->fault = fault;
    rt->mission_id = 1u;
    rt->actor_count = 2u;
    rt->npc_x = 90;

    ghc_local_input_snapshot_clear(&rt->snapshot);
    ghc_local_input_tracker_reset(&rt->tracker);
    ghc_mission_init(&rt->mission, 0);

    for (i = 0; i < 2u; ++i) {
        SimPlayer *p = &rt->players[i];
        GhcLocalPlayerObservation *o = &rt->observations[i];

        p->active = 1u;
        p->role = (uint8_t)(i == 0u ? GHC_LOCAL_ROLE_LOGAN : GHC_LOCAL_ROLE_GUEST_SOLDIER);
        p->health = 100u;
        p->view_slot = (uint8_t)i;
        p->entity_id = (uint16_t)(20u + i);
        p->x = (int32_t)(i * 100u);
        p->camera_context = 0x1000u + (uint64_t)i * 0x100u;
        p->hud_context = 0x2000u + (uint64_t)i * 0x100u;
        p->player_state_address = 0x5000u + (uint64_t)i * 0x100u;

        o->split_screen_active = 1u;
        o->is_split_screen_player = 1u;
        o->is_primary = (uint8_t)(i == 0u);
        o->controller_port = (uint8_t)i;
        o->input_connected = 1u;
        o->input_frame_valid = 1u;
        o->observation_frame_valid = 1u;
        o->client_id_valid = 1u;
        o->entity_id_valid = 1u;
        o->local_client_num_valid = 1u;
        o->client_id = (uint16_t)(10u + i);
        o->entity_id = p->entity_id;
        o->local_client_num = (uint8_t)i;
        o->role = p->role;
        o->observation_frame = 100u;
        o->gclient_address = 0x3000u + (uint64_t)i * 0x100u;
        o->gentity_address = 0x4000u + (uint64_t)i * 0x100u;
        o->player_state_address = p->player_state_address;
        o->input_command_address = 0x6000u + (uint64_t)i * 0x100u;
        o->view_context_address = p->camera_context;
        o->hud_context_address = p->hud_context;
    }

    if (fault == FAULT_SHARED_CAMERA) {
        rt->players[1].camera_context = rt->players[0].camera_context;
        rt->observations[1].view_context_address = rt->observations[0].view_context_address;
    }
    if (fault == FAULT_SHARED_HUD) {
        rt->players[1].hud_context = rt->players[0].hud_context;
        rt->observations[1].hud_context_address = rt->observations[0].hud_context_address;
    }
    if (fault == FAULT_ALIAS_PLAYER_STATE) {
        rt->observations[1].player_state_address = rt->observations[0].player_state_address;
    }
    if (fault == FAULT_SHARED_COMMAND_STREAM) {
        rt->observations[1].input_command_address = rt->observations[0].input_command_address;
    }
    if (fault == FAULT_STALE_INPUT_FRAME) {
        rt->observations[1].input_frame_valid = 0u;
    }
    if (fault == FAULT_MISMATCHED_OBSERVATION_FRAME) {
        rt->observations[1].observation_frame++;
    }
    if (fault == FAULT_MISSING_OBSERVATION_FRAME) {
        rt->observations[1].observation_frame_valid = 0u;
    }
}

static int sim_gate_ready(const SimRuntime *rt)
{
    GhcLocalPlayerGateResult result;
    return ghc_local_player_gate_validate(rt->observations, &result) == GHC_LOCAL_GATE_READY;
}

static void make_raw_pad(uint16_t raw[GHC_PAD_BUTTON_DATA_COUNT],
                         uint16_t digital2, uint8_t left_x)
{
    memset(raw, 0, sizeof(uint16_t) * GHC_PAD_BUTTON_DATA_COUNT);
    raw[3] = digital2;
    raw[4] = 128u;
    raw[5] = 128u;
    raw[6] = left_x;
    raw[7] = 128u;
    raw[18] = 0u;
    raw[19] = 0u;
}

static int sim_input_frame(SimRuntime *rt,
                           uint16_t p1_digital2, uint8_t p1_left_x,
                           uint16_t p2_digital2, uint8_t p2_left_x)
{
    uint16_t raw0[GHC_PAD_BUTTON_DATA_COUNT];
    uint16_t raw1[GHC_PAD_BUTTON_DATA_COUNT];
    uint32_t i;

    ghc_local_input_snapshot_clear(&rt->snapshot);
    make_raw_pad(raw0, p1_digital2, p1_left_x);
    make_raw_pad(raw1, p2_digital2, p2_left_x);

    if (ghc_local_input_decode_port(0u, 1u, 24, raw0, &rt->snapshot.players[0]) != GHC_LOCAL_INPUT_OK) {
        return 0;
    }
    if (ghc_local_input_decode_port(1u, 1u, 24, raw1, &rt->snapshot.players[1]) != GHC_LOCAL_INPUT_OK) {
        return 0;
    }
    rt->snapshot.connected_mask = 3u;
    rt->snapshot.connected_count = 2u;

    if (ghc_local_input_tracker_update(
            &rt->snapshot, &rt->tracker, GHC_LOCAL_STICK_DEADZONE_DEFAULT, &rt->frame
        ) != GHC_LOCAL_INPUT_OK) {
        return 0;
    }

    for (i = 0; i < 2u; ++i) {
        const GhcLocalPlayerFrame *input = &rt->frame.players[i];
        uint32_t target = i;
        if (!input->connected) continue;

        /* Deliberate mutant: P2's commands are routed into P1's state. */
        if (rt->fault == FAULT_CROSS_INPUT && i == 1u) target = 0u;

        rt->players[target].x += input->left_x;
        if ((input->pressed & GHC_LOCAL_BUTTON_CROSS) != 0u) {
            rt->players[target].cross_actions++;
        }
        if ((input->pressed & GHC_LOCAL_BUTTON_CIRCLE) != 0u) {
            rt->players[target].circle_actions++;
        }
    }
    return 1;
}

static int sim_input_oracle(SimFault fault)
{
    SimRuntime rt;
    sim_init(&rt, fault);
    if (!sim_gate_ready(&rt)) return 0;
    if (!sim_input_frame(&rt,
                         (uint16_t)(1u << 6), 255u,
                         (uint16_t)(1u << 5), 0u)) return 0;

    return rt.players[0].x == 127
        && rt.players[1].x == -27
        && rt.players[0].cross_actions == 1u
        && rt.players[0].circle_actions == 0u
        && rt.players[1].cross_actions == 0u
        && rt.players[1].circle_actions == 1u;
}

static int sim_camera_hud_oracle(SimFault fault)
{
    SimRuntime rt;
    sim_init(&rt, fault);
    return sim_gate_ready(&rt)
        && rt.players[0].camera_context != rt.players[1].camera_context
        && rt.players[0].hud_context != rt.players[1].hud_context
        && rt.players[0].view_slot != rt.players[1].view_slot;
}

static uint8_t sim_choose_npc_target(const SimRuntime *rt)
{
    int32_t d0, d1;
    if (rt->fault == FAULT_AI_PLAYER_ONE_ONLY) return 0u;
    d0 = rt->npc_x - rt->players[0].x;
    d1 = rt->npc_x - rt->players[1].x;
    if (d0 < 0) d0 = -d0;
    if (d1 < 0) d1 = -d1;
    return (uint8_t)(d0 <= d1 ? 0u : 1u);
}

static int sim_ai_oracle(SimFault fault)
{
    SimRuntime rt;
    sim_init(&rt, fault);
    rt.players[0].x = 0;
    rt.players[1].x = 100;
    rt.npc_x = 95;
    rt.npc_target = sim_choose_npc_target(&rt);
    if (rt.npc_target != 1u) return 0;

    rt.npc_x = 5;
    rt.npc_target = sim_choose_npc_target(&rt);
    return rt.npc_target == 0u;
}

static int sim_revive_oracle(void)
{
    SimRuntime rt;
    sim_init(&rt, FAULT_NONE);

    CHECK("P2 down event", ghc_mission_player_downed(&rt.mission, 2u, 1000u) == GHC_RULE_PLAYER_DOWNED);
    CHECK("mission remains running while P1 is alive", rt.mission.phase == GHC_MISSION_RUNNING);
    CHECK("P1 remains alive", rt.mission.players[0].life_state == GHC_PLAYER_ALIVE);
    CHECK("P2 becomes downed", rt.mission.players[1].life_state == GHC_PLAYER_DOWNED);

    CHECK("revive starts", ghc_mission_update_revive(&rt.mission, 1u, 2u, 1u, 1u, 1u, 2000u) == GHC_RULE_NO_CHANGE);
    CHECK("revive completes", ghc_mission_update_revive(&rt.mission, 1u, 2u, 1u, 1u, 1u, 7000u) == GHC_RULE_PLAYER_REVIVED);
    CHECK("P2 returns alive", rt.mission.players[1].life_state == GHC_PLAYER_ALIVE);
    CHECK("revived health is 40 percent", rt.mission.players[1].health_percent == 40u);

    ghc_mission_reset(&rt.mission);
    CHECK("P1 can be downed", ghc_mission_player_downed(&rt.mission, 1u, 8000u) == GHC_RULE_PLAYER_DOWNED);
    CHECK("both down fails mission", ghc_mission_player_downed(&rt.mission, 2u, 8100u) == GHC_RULE_MISSION_FAILED);
    CHECK("both-down failure reason", rt.mission.failure_reason == GHC_MISSION_FAIL_BOTH_DOWN);
    return rt.mission.phase == GHC_MISSION_FAILED;
}

static void sim_checkpoint_save(const SimRuntime *rt, SimCheckpoint *checkpoint)
{
    memcpy(checkpoint->players, rt->players, sizeof(checkpoint->players));
    checkpoint->mission_id = rt->mission_id;
}

static void sim_checkpoint_restore(SimRuntime *rt, const SimCheckpoint *checkpoint)
{
    memcpy(rt->players, checkpoint->players, sizeof(rt->players));
    rt->mission_id = checkpoint->mission_id;
    if (rt->fault == FAULT_DROP_GUEST_ON_CHECKPOINT) {
        rt->players[1].active = 0u;
    }
}

static int sim_checkpoint_oracle(SimFault fault)
{
    SimRuntime rt;
    SimCheckpoint checkpoint;
    sim_init(&rt, fault);
    rt.players[0].x = 150;
    rt.players[1].x = 350;
    rt.players[0].health = 75u;
    rt.players[1].health = 25u;
    sim_checkpoint_save(&rt, &checkpoint);

    rt.players[0].x = -999;
    rt.players[1].x = -999;
    rt.players[0].health = 1u;
    rt.players[1].health = 1u;
    sim_checkpoint_restore(&rt, &checkpoint);

    return rt.players[0].active && rt.players[1].active
        && rt.players[0].x == 150 && rt.players[1].x == 350
        && rt.players[0].health == 75u && rt.players[1].health == 25u
        && rt.players[0].entity_id != rt.players[1].entity_id
        && rt.players[0].role == GHC_LOCAL_ROLE_LOGAN
        && rt.players[1].role == GHC_LOCAL_ROLE_GUEST_SOLDIER;
}

static int sim_transition_oracle(SimFault fault)
{
    SimRuntime rt;
    sim_init(&rt, fault);
    ++rt.mission_id;
    if (fault == FAULT_DUPLICATE_GUEST_ON_TRANSITION) {
        ++rt.actor_count;
        rt.players[1].entity_id = rt.players[0].entity_id;
    }
    return rt.actor_count == 2u
        && rt.players[0].active && rt.players[1].active
        && rt.players[0].entity_id != rt.players[1].entity_id
        && rt.mission_id == 2u;
}

static void test_gate_and_controller_routing(void)
{
    SimRuntime rt;
    sim_init(&rt, FAULT_NONE);
    CHECK("two distinct native local-player observations pass the gate", sim_gate_ready(&rt));
    CHECK("input decoder keeps P1 and P2 isolated", sim_input_oracle(FAULT_NONE));
    CHECK("controller mutant is detected", !sim_input_oracle(FAULT_CROSS_INPUT));
    sim_init(&rt, FAULT_ALIAS_PLAYER_STATE);
    CHECK("aliased engine player-state addresses fail closed", !sim_gate_ready(&rt));
    sim_init(&rt, FAULT_SHARED_COMMAND_STREAM);
    CHECK("shared native command stream fails closed", !sim_gate_ready(&rt));
    sim_init(&rt, FAULT_STALE_INPUT_FRAME);
    CHECK("stale or missing input frame fails closed", !sim_gate_ready(&rt));
    sim_init(&rt, FAULT_MISMATCHED_OBSERVATION_FRAME);
    CHECK("player observations from different engine frames fail closed", !sim_gate_ready(&rt));
    sim_init(&rt, FAULT_MISSING_OBSERVATION_FRAME);
    CHECK("missing engine observation frame id fails closed", !sim_gate_ready(&rt));
}

static void test_camera_and_hud_isolation(void)
{
    CHECK("each local player has a distinct camera, HUD and view slot", sim_camera_hud_oracle(FAULT_NONE));
    CHECK("shared camera mutant is detected by the readiness gate", !sim_camera_hud_oracle(FAULT_SHARED_CAMERA));
    CHECK("shared HUD mutant is detected by the readiness gate", !sim_camera_hud_oracle(FAULT_SHARED_HUD));
}

static void test_ai_can_target_both_players(void)
{
    CHECK("AI target can be P2 and then P1", sim_ai_oracle(FAULT_NONE));
    CHECK("P1-only AI mutant is detected", !sim_ai_oracle(FAULT_AI_PLAYER_ONE_ONLY));
}

static void test_mission_recovery_rules(void)
{
    CHECK("down/revive and both-down restart contract", sim_revive_oracle());
}

static void test_checkpoint_and_transition_invariants(void)
{
    CHECK("checkpoint restores both independent players", sim_checkpoint_oracle(FAULT_NONE));
    CHECK("checkpoint dropping P2 mutant is detected", !sim_checkpoint_oracle(FAULT_DROP_GUEST_ON_CHECKPOINT));
    CHECK("mission transition preserves exactly two player actors", sim_transition_oracle(FAULT_NONE));
    CHECK("duplicate P2 transition mutant is detected", !sim_transition_oracle(FAULT_DUPLICATE_GUEST_ON_TRANSITION));
}

static void test_deterministic_input_soak(void)
{
    SimRuntime rt;
    uint32_t seed = 0x47535453u;
    uint32_t i;
    sim_init(&rt, FAULT_NONE);
    for (i = 0; i < 5000u; ++i) {
        uint16_t p1_buttons, p2_buttons;
        uint8_t p1_x, p2_x;
        seed = seed * 1664525u + 1013904223u;
        p1_buttons = (uint16_t)((seed >> 16) & 0x00f0u);
        seed = seed * 1664525u + 1013904223u;
        p2_buttons = (uint16_t)((seed >> 16) & 0x00f0u);
        seed = seed * 1664525u + 1013904223u;
        p1_x = (uint8_t)(seed >> 24);
        seed = seed * 1664525u + 1013904223u;
        p2_x = (uint8_t)(seed >> 24);
        CHECK("seeded controller frame decodes", sim_input_frame(&rt, p1_buttons, p1_x, p2_buttons, p2_x));
        CHECK("player identities stay distinct through the soak",
              rt.players[0].entity_id != rt.players[1].entity_id
              && rt.players[0].camera_context != rt.players[1].camera_context
              && rt.players[0].hud_context != rt.players[1].hud_context
              && sim_gate_ready(&rt));
    }
}

int main(void)
{
    test_gate_and_controller_routing();
    test_camera_and_hud_isolation();
    test_ai_can_target_both_players();
    test_mission_recovery_rules();
    test_checkpoint_and_transition_invariants();
    test_deterministic_input_soak();

    printf("Ghosts host runtime simulator: %u checks, %u failures\n", g_checks, g_failures);
    if (g_failures != 0u) {
        printf("RESULT: FAIL host-runtime-sim\n");
        return 1;
    }
    printf("RESULT: PASS host-runtime-sim\n");
    return 0;
}
