#include "../client/ps3/local_input.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

int main(void)
{
    test_disconnected_port_is_zeroed();
    test_button_and_stick_mapping();
    test_second_controller_is_independent();
    test_invalid_input_is_rejected();
    test_snapshot_clear_sets_port_numbers();
    puts("Ghosts local controller input decoder: PASS");
    return 0;
}
