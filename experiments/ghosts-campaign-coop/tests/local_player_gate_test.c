#include "../client/ps3/local_player_gate.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void make_ready_pair(GhcLocalPlayerObservation players[2]) {
    memset(players, 0, sizeof(GhcLocalPlayerObservation) * 2u);

    players[0].split_screen_active = 1u;
    players[0].is_split_screen_player = 1u;
    players[0].is_primary = 1u;
    players[0].controller_port = 0u;
    players[0].input_connected = 1u;
    players[0].input_frame_valid = 1u;
    players[0].observation_frame_valid = 1u;
    players[0].client_id_valid = 1u;
    players[0].entity_id_valid = 1u;
    players[0].local_client_num_valid = 1u;
    players[0].local_client_num = 0u;
    players[0].client_id = 0u;
    players[0].entity_id = 2u;
    players[0].role = GHC_LOCAL_ROLE_LOGAN;
    players[0].observation_frame = 77u;
    players[0].gclient_address = 0x100000u;
    players[0].gentity_address = 0x200000u;
    players[0].player_state_address = 0x100100u;
    players[0].input_command_address = 0x300000u;
    players[0].view_context_address = 0x400000u;
    players[0].hud_context_address = 0x500000u;

    players[1].split_screen_active = 1u;
    players[1].is_split_screen_player = 1u;
    players[1].is_primary = 0u;
    players[1].controller_port = 1u;
    players[1].input_connected = 1u;
    players[1].input_frame_valid = 1u;
    players[1].observation_frame_valid = 1u;
    players[1].client_id_valid = 1u;
    players[1].entity_id_valid = 1u;
    players[1].local_client_num_valid = 1u;
    players[1].local_client_num = 1u;
    players[1].client_id = 1u;
    players[1].entity_id = 3u;
    players[1].role = GHC_LOCAL_ROLE_GUEST_SOLDIER;
    players[1].observation_frame = 77u;
    players[1].gclient_address = 0x103700u;
    players[1].gentity_address = 0x200280u;
    players[1].player_state_address = 0x103800u;
    players[1].input_command_address = 0x303000u;
    players[1].view_context_address = 0x403000u;
    players[1].hud_context_address = 0x503000u;
}

static void test_valid_pair_passes(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_READY);
    assert(result.ready == 1u);
    assert(result.status == GHC_LOCAL_GATE_READY);
    assert(result.failing_slot == GHC_LOCAL_PLAYER_NO_SLOT);
}

static void test_split_screen_must_be_active(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    players[1].split_screen_active = 0u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_SPLITSCREEN_INACTIVE);
    assert(result.failing_slot == 1u);
    assert(result.ready == 0u);
}

static void test_both_players_must_be_native_split_screen_players(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    players[1].is_split_screen_player = 0u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_NOT_SPLITSCREEN_PLAYER);
}

static void test_primary_identity_must_be_ordered(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    players[0].is_primary = 0u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_PRIMARY_MISMATCH);
}

static void test_controller_ports_must_be_distinct_and_ordered(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    players[1].controller_port = 0u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_CONTROLLER_MISMATCH);
}

static void test_both_controller_inputs_must_be_connected(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    players[1].input_connected = 0u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_INPUT_NOT_CONNECTED);
}

static void test_both_players_need_a_valid_fresh_input_frame(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    players[1].input_frame_valid = 0u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_INPUT_FRAME_MISSING);
    assert(result.failing_slot == 1u);
}

static void test_both_observations_need_a_valid_engine_frame_id(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    players[1].observation_frame_valid = 0u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_OBSERVATION_FRAME_MISSING);
    assert(result.failing_slot == 1u);
}

static void test_both_observations_must_belong_to_the_same_engine_frame(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    players[1].observation_frame++;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_OBSERVATION_FRAME_MISMATCH);
}

static void test_boolean_observation_flags_must_be_canonical(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;

    make_ready_pair(players);
    players[1].split_screen_active = 2u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_INVALID_BOOLEAN_FLAG);

    make_ready_pair(players);
    players[1].is_split_screen_player = 2u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_INVALID_BOOLEAN_FLAG);

    make_ready_pair(players);
    players[1].input_connected = 2u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_INVALID_BOOLEAN_FLAG);

    make_ready_pair(players);
    players[1].input_frame_valid = 2u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_INVALID_BOOLEAN_FLAG);

    make_ready_pair(players);
    players[1].observation_frame_valid = 2u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_INVALID_BOOLEAN_FLAG);

    make_ready_pair(players);
    players[1].client_id_valid = 2u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_INVALID_BOOLEAN_FLAG);

    make_ready_pair(players);
    players[1].entity_id_valid = 2u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_INVALID_BOOLEAN_FLAG);

    make_ready_pair(players);
    players[1].local_client_num_valid = 2u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_INVALID_BOOLEAN_FLAG);
    assert(result.failing_slot == 1u);
}

static void test_story_identity_must_be_preserved(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    players[1].role = GHC_LOCAL_ROLE_LOGAN;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_ROLE_MISMATCH);
}

static void test_local_client_numbers_must_be_valid_and_unique_without_aliasing_controller_ports(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    players[0].local_client_num = 7u;
    players[1].local_client_num = 11u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_READY);

    players[1].local_client_num_valid = 0u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_LOCAL_CLIENT_NUM_MISSING);

    make_ready_pair(players);
    players[1].local_client_num = players[0].local_client_num;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_DUPLICATE_LOCAL_CLIENT_NUM);
}

static void test_client_and_entity_ids_must_be_valid_and_unique(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    players[1].client_id_valid = 0u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_CLIENT_ID_MISSING);

    make_ready_pair(players);
    players[1].client_id = players[0].client_id;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_DUPLICATE_CLIENT_ID);

    make_ready_pair(players);
    players[1].entity_id = players[0].entity_id;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_DUPLICATE_ENTITY_ID);
}

static void test_player_state_addresses_must_be_present_and_unique(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    players[1].player_state_address = 0u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_STATE_ADDRESS_MISSING);

    make_ready_pair(players);
    players[1].gclient_address = players[0].gclient_address;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_DUPLICATE_STATE_ADDRESS);

    make_ready_pair(players);
    players[1].player_state_address = players[0].gclient_address;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_DUPLICATE_STATE_ADDRESS);
}

static void test_command_view_and_hud_contexts_are_required(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;

    make_ready_pair(players);
    players[1].input_command_address = 0u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_INPUT_COMMAND_ADDRESS_MISSING);

    make_ready_pair(players);
    players[1].view_context_address = 0u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_VIEW_CONTEXT_ADDRESS_MISSING);

    make_ready_pair(players);
    players[1].hud_context_address = 0u;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_HUD_CONTEXT_ADDRESS_MISSING);
}

static void test_command_view_and_hud_contexts_must_be_independent(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;

    make_ready_pair(players);
    players[1].input_command_address = players[0].input_command_address;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_DUPLICATE_ENGINE_CONTEXT_ADDRESS);

    make_ready_pair(players);
    players[1].view_context_address = players[0].view_context_address;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_DUPLICATE_ENGINE_CONTEXT_ADDRESS);

    make_ready_pair(players);
    players[1].hud_context_address = players[0].hud_context_address;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_DUPLICATE_ENGINE_CONTEXT_ADDRESS);

    make_ready_pair(players);
    players[1].view_context_address = players[0].gclient_address;
    assert(ghc_local_player_gate_validate(players, &result) == GHC_LOCAL_GATE_DUPLICATE_ENGINE_CONTEXT_ADDRESS);
}

static void test_null_arguments_fail_closed(void) {
    GhcLocalPlayerObservation players[2];
    GhcLocalPlayerGateResult result;
    make_ready_pair(players);
    result.ready = 1u;
    result.failing_slot = 0u;
    result.status = GHC_LOCAL_GATE_READY;
    assert(ghc_local_player_gate_validate(0, &result) == GHC_LOCAL_GATE_INVALID_ARGUMENT);
    assert(result.ready == 0u);
    assert(result.failing_slot == GHC_LOCAL_PLAYER_NO_SLOT);
    assert(result.status == GHC_LOCAL_GATE_INVALID_ARGUMENT);
    assert(ghc_local_player_gate_validate(players, 0) == GHC_LOCAL_GATE_INVALID_ARGUMENT);
}

int main(void) {
    test_valid_pair_passes();
    test_split_screen_must_be_active();
    test_both_players_must_be_native_split_screen_players();
    test_primary_identity_must_be_ordered();
    test_controller_ports_must_be_distinct_and_ordered();
    test_both_controller_inputs_must_be_connected();
    test_both_players_need_a_valid_fresh_input_frame();
    test_both_observations_need_a_valid_engine_frame_id();
    test_both_observations_must_belong_to_the_same_engine_frame();
    test_boolean_observation_flags_must_be_canonical();
    test_story_identity_must_be_preserved();
    test_local_client_numbers_must_be_valid_and_unique_without_aliasing_controller_ports();
    test_client_and_entity_ids_must_be_valid_and_unique();
    test_player_state_addresses_must_be_present_and_unique();
    test_command_view_and_hud_contexts_are_required();
    test_command_view_and_hud_contexts_must_be_independent();
    test_null_arguments_fail_closed();
    puts("Ghosts local-player readiness gate tests: PASS");
    return 0;
}
