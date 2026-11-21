#include "../client/ps3/local_input.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void decode_cross(GhcLocalInputSnapshot *snapshot, uint8_t port)
{
    uint16_t raw[GHC_PAD_BUTTON_DATA_COUNT] = {0};
    raw[3] = (uint16_t)(1u << 6);
    raw[4] = 128u; raw[5] = 128u; raw[6] = 128u; raw[7] = 128u;
    assert(ghc_local_input_decode_port(port, 1u, 24, raw, &snapshot->players[port]) == GHC_LOCAL_INPUT_OK);
    snapshot->connected_mask |= (uint8_t)(1u << port);
    snapshot->connected_count++;
}

static void test_disconnected_port_is_zeroed(void)
{
    GhcLocalPlayerInput player;
    memset(&player, 0xff, sizeof(player));
    assert(ghc_local_input_decode_port(1u, 0u, 0, 0, &player) == GHC_LOCAL_INPUT_OK);
    assert(player.port == 1u && player.connected == 0u && player.buttons == 0u);
    assert(player.left_x == 0u && player.right_y == 0u);
}

static void test_button_and_stick_mapping(void)
{
    uint16_t raw[GHC_PAD_BUTTON_DATA_COUNT] = {0};
    GhcLocalPlayerInput player;
    raw[2] = (uint16_t)((1u << 7) | (1u << 3) | (1u << 0));
    raw[3] = (uint16_t)((1u << 6) | (1u << 3) | (1u << 1));
    raw[4] = 220u; raw[5] = 30u; raw[6] = 12u; raw[7] = 245u;
    raw[18] = 170u; raw[19] = 190u;

    assert(ghc_local_input_decode_port(0u, 1u, 24, raw, &player) == GHC_LOCAL_INPUT_OK);
    assert(player.connected == 1u && player.port == 0u);
    assert((player.buttons & GHC_LOCAL_BUTTON_LEFT) != 0u);
    assert((player.buttons & GHC_LOCAL_BUTTON_START) != 0u);
    assert((player.buttons & GHC_LOCAL_BUTTON_SELECT) != 0u);
    assert((player.buttons & GHC_LOCAL_BUTTON_CROSS) != 0u);
    assert((player.buttons & GHC_LOCAL_BUTTON_R1) != 0u);
    assert((player.buttons & GHC_LOCAL_BUTTON_R2) != 0u);
    assert((player.buttons & GHC_LOCAL_BUTTON_UP) == 0u);
    assert(player.right_x == 220u && player.right_y == 30u);
    assert(player.left_x == 12u && player.left_y == 245u);
    assert(player.l2_pressure == 170u && player.r2_pressure == 190u);
}

static void test_second_controller_is_independent(void)
{
    uint16_t first[GHC_PAD_BUTTON_DATA_COUNT] = {0};
    uint16_t second[GHC_PAD_BUTTON_DATA_COUNT] = {0};
    GhcLocalPlayerInput player1, player2;
    first[3] = (uint16_t)(1u << 6);
    second[3] = (uint16_t)(1u << 5);
    first[6] = 64u; second[6] = 192u;

    assert(ghc_local_input_decode_port(0u, 1u, 24, first, &player1) == GHC_LOCAL_INPUT_OK);
    assert(ghc_local_input_decode_port(1u, 1u, 24, second, &player2) == GHC_LOCAL_INPUT_OK);
    assert((player1.buttons & GHC_LOCAL_BUTTON_CROSS) != 0u);
    assert((player1.buttons & GHC_LOCAL_BUTTON_CIRCLE) == 0u);
    assert((player2.buttons & GHC_LOCAL_BUTTON_CIRCLE) != 0u);
    assert((player2.buttons & GHC_LOCAL_BUTTON_CROSS) == 0u);
    assert(player1.left_x == 64u && player2.left_x == 192u);
    assert(player1.port == 0u && player2.port == 1u);
}

static void test_invalid_input_is_rejected(void)
{
    uint16_t raw[GHC_PAD_BUTTON_DATA_COUNT] = {0};
    GhcLocalPlayerInput player;
    assert(ghc_local_input_decode_port(0u, 1u, 7, raw, &player) == GHC_LOCAL_INPUT_ERROR_DATA);
    assert(ghc_local_input_decode_port(0u, 1u, 24, 0, &player) == GHC_LOCAL_INPUT_ERROR_DATA);
    assert(ghc_local_input_decode_port(2u, 1u, 24, raw, &player) == GHC_LOCAL_INPUT_ERROR_ARGUMENT);
    assert(ghc_local_input_decode_port(0u, 1u, 65, raw, &player) == GHC_LOCAL_INPUT_ERROR_DATA);
    assert(ghc_local_input_decode_port(0u, 1u, 24, raw, 0) == GHC_LOCAL_INPUT_ERROR_ARGUMENT);
}

static void test_snapshot_clear_sets_port_numbers(void)
{
    GhcLocalInputSnapshot snapshot;
    memset(&snapshot, 0xff, sizeof(snapshot));
    ghc_local_input_snapshot_clear(&snapshot);
    assert(snapshot.connected_mask == 0u && snapshot.connected_count == 0u);
    assert(snapshot.read_error_mask == 0u);
    assert(snapshot.players[0].port == 0u && snapshot.players[1].port == 1u);
    assert(snapshot.players[0].connected == 0u && snapshot.players[1].connected == 0u);
}

static void test_frame_button_edges_and_stick_deadzone(void)
{
    GhcLocalInputSnapshot snapshot;
    GhcLocalInputTracker tracker;
    GhcLocalInputFrame frame;
    uint16_t raw[GHC_PAD_BUTTON_DATA_COUNT] = {0};

    ghc_local_input_snapshot_clear(&snapshot);
    ghc_local_input_tracker_reset(&tracker);
    raw[3] = (uint16_t)(1u << 6);
    raw[4] = 255u; raw[5] = 128u; raw[6] = 148u; raw[7] = 107u;
    raw[18] = 201u; raw[19] = 222u;
    assert(ghc_local_input_decode_port(0u, 1u, 24, raw, &snapshot.players[0]) == GHC_LOCAL_INPUT_OK);
    snapshot.connected_mask = 1u;
    snapshot.connected_count = 1u;

    assert(ghc_local_input_tracker_update(&snapshot, &tracker, GHC_LOCAL_STICK_DEADZONE_DEFAULT, &frame) == GHC_LOCAL_INPUT_OK);
    assert(frame.valid_mask == 1u && frame.connected_mask == 1u);
    assert(frame.players[0].buttons == GHC_LOCAL_BUTTON_CROSS);
    assert(frame.players[0].pressed == GHC_LOCAL_BUTTON_CROSS && frame.players[0].released == 0u);
    assert(frame.players[0].left_x == 0 && frame.players[0].left_y == -1);
    assert(frame.players[0].right_x == 127 && frame.players[0].right_y == 0);
    assert(frame.players[0].l2_pressure == 201u && frame.players[0].r2_pressure == 222u);

    assert(ghc_local_input_tracker_update(&snapshot, &tracker, GHC_LOCAL_STICK_DEADZONE_DEFAULT, &frame) == GHC_LOCAL_INPUT_OK);
    assert(frame.players[0].pressed == 0u && frame.players[0].released == 0u);
}

static void test_two_player_frame_edges_are_independent(void)
{
    GhcLocalInputSnapshot snapshot;
    GhcLocalInputTracker tracker;
    GhcLocalInputFrame frame;
    uint16_t raw0[GHC_PAD_BUTTON_DATA_COUNT] = {0};
    uint16_t raw1[GHC_PAD_BUTTON_DATA_COUNT] = {0};

    ghc_local_input_snapshot_clear(&snapshot);
    ghc_local_input_tracker_reset(&tracker);
    raw0[3] = (uint16_t)(1u << 6);
    raw1[3] = (uint16_t)(1u << 5);
    raw0[6] = 255u;
    raw1[6] = 0u;
    assert(ghc_local_input_decode_port(0u, 1u, 24, raw0, &snapshot.players[0]) == GHC_LOCAL_INPUT_OK);
    assert(ghc_local_input_decode_port(1u, 1u, 24, raw1, &snapshot.players[1]) == GHC_LOCAL_INPUT_OK);
    snapshot.connected_mask = 3u;
    snapshot.connected_count = 2u;

    assert(ghc_local_input_tracker_update(&snapshot, &tracker, 20u, &frame) == GHC_LOCAL_INPUT_OK);
    assert(frame.connected_mask == 3u && frame.valid_mask == 3u);
    assert(frame.players[0].pressed == GHC_LOCAL_BUTTON_CROSS);
    assert(frame.players[1].pressed == GHC_LOCAL_BUTTON_CIRCLE);
    assert(frame.players[0].left_x == 127 && frame.players[1].left_x == -127);
    assert((frame.players[0].buttons & GHC_LOCAL_BUTTON_CIRCLE) == 0u);
    assert((frame.players[1].buttons & GHC_LOCAL_BUTTON_CROSS) == 0u);
}

static void test_disconnect_emits_release_once(void)
{
    GhcLocalInputSnapshot snapshot;
    GhcLocalInputTracker tracker;
    GhcLocalInputFrame frame;

    ghc_local_input_snapshot_clear(&snapshot);
    ghc_local_input_tracker_reset(&tracker);
    decode_cross(&snapshot, 0u);
    assert(ghc_local_input_tracker_update(&snapshot, &tracker, 20u, &frame) == GHC_LOCAL_INPUT_OK);
    assert(frame.players[0].pressed == GHC_LOCAL_BUTTON_CROSS);

    ghc_local_input_snapshot_clear(&snapshot);
    assert(ghc_local_input_tracker_update(&snapshot, &tracker, 20u, &frame) == GHC_LOCAL_INPUT_OK);
    assert(frame.connected_mask == 0u);
    assert(frame.players[0].released == GHC_LOCAL_BUTTON_CROSS);
    assert(frame.players[0].buttons == 0u);

    assert(ghc_local_input_tracker_update(&snapshot, &tracker, 20u, &frame) == GHC_LOCAL_INPUT_OK);
    assert(frame.players[0].released == 0u);
}

static void test_read_error_does_not_fake_button_release(void)
{
    GhcLocalInputSnapshot snapshot;
    GhcLocalInputTracker tracker;
    GhcLocalInputFrame frame;

    ghc_local_input_snapshot_clear(&snapshot);
    ghc_local_input_tracker_reset(&tracker);
    decode_cross(&snapshot, 0u);
    assert(ghc_local_input_tracker_update(&snapshot, &tracker, 20u, &frame) == GHC_LOCAL_INPUT_OK);

    ghc_local_input_snapshot_clear(&snapshot);
    snapshot.read_error_mask = 1u;
    assert(ghc_local_input_tracker_update(&snapshot, &tracker, 20u, &frame) == GHC_LOCAL_INPUT_OK);
    assert(frame.read_error_mask == 1u && frame.valid_mask == 2u);
    assert(frame.players[0].connected == 1u);
    assert(frame.players[0].buttons == GHC_LOCAL_BUTTON_CROSS);
    assert(frame.players[0].pressed == 0u && frame.players[0].released == 0u);
}

static void test_invalid_deadzone_is_rejected(void)
{
    GhcLocalInputSnapshot snapshot;
    GhcLocalInputTracker tracker;
    GhcLocalInputFrame frame;
    ghc_local_input_snapshot_clear(&snapshot);
    ghc_local_input_tracker_reset(&tracker);
    assert(ghc_local_input_tracker_update(&snapshot, &tracker, 127u, &frame) == GHC_LOCAL_INPUT_ERROR_ARGUMENT);
}

int main(void)
{
    test_disconnected_port_is_zeroed();
    test_button_and_stick_mapping();
    test_second_controller_is_independent();
    test_invalid_input_is_rejected();
    test_snapshot_clear_sets_port_numbers();
    test_frame_button_edges_and_stick_deadzone();
    test_two_player_frame_edges_are_independent();
    test_disconnect_emits_release_once();
    test_read_error_does_not_fake_button_release();
    test_invalid_deadzone_is_rejected();
    puts("Ghosts local controller input decoder and frame tracker: PASS");
    return 0;
}
