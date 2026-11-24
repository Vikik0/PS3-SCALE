#include "../client/ps3/local_input.h"
#include <cell/pad.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t g_connected_mask;
static int32_t g_info_result;
static int32_t g_data_result[GHC_LOCAL_INPUT_PLAYER_COUNT];
static int32_t g_data_length[GHC_LOCAL_INPUT_PLAYER_COUNT];
static uint16_t g_raw[GHC_LOCAL_INPUT_PLAYER_COUNT][GHC_PAD_BUTTON_DATA_COUNT];
static uint32_t g_info_calls;
static uint32_t g_data_calls[CELL_PAD_MAX_PORT_NUM];

int32_t cellPadGetInfo2(CellPadInfo2 *info)
{
    uint32_t port;
    if (!info) return -1;
    g_info_calls++;
    if (g_info_result != CELL_PAD_OK) return g_info_result;
    memset(info, 0, sizeof(*info));
    info->max_connect = GHC_LOCAL_INPUT_PLAYER_COUNT;
    for (port = 0; port < GHC_LOCAL_INPUT_PLAYER_COUNT; ++port) {
        if ((g_connected_mask & (1u << port)) != 0u) {
            info->port_status[port] = CELL_PAD_STATUS_CONNECTED;
            info->now_connect++;
        }
    }
    return CELL_PAD_OK;
}

int32_t cellPadGetData(uint32_t port_no, CellPadData *data)
{
    if (port_no >= GHC_LOCAL_INPUT_PLAYER_COUNT || !data) return -1;
    g_data_calls[port_no]++;
    if (g_data_result[port_no] != CELL_PAD_OK) return g_data_result[port_no];
    memset(data, 0, sizeof(*data));
    data->len = g_data_length[port_no];
    memcpy(data->button, g_raw[port_no], sizeof(g_raw[port_no]));
    return CELL_PAD_OK;
}

static void reset_mocks(void)
{
    memset(g_data_result, 0, sizeof(g_data_result));
    memset(g_data_length, 0, sizeof(g_data_length));
    memset(g_raw, 0, sizeof(g_raw));
    memset(g_data_calls, 0, sizeof(g_data_calls));
    g_connected_mask = 0u;
    g_info_result = CELL_PAD_OK;
    g_info_calls = 0u;
}

static void test_both_ports_return_independent_button_and_stick_data(void)
{
    GhcLocalInputSnapshot snapshot;
    reset_mocks();
    g_connected_mask = 0x03u;
    g_data_length[0] = 24;
    g_data_length[1] = 24;
    g_raw[0][CELL_PAD_BTN_OFFSET_DIGITAL2] = (uint16_t)(1u << 6);
    g_raw[1][CELL_PAD_BTN_OFFSET_DIGITAL2] = (uint16_t)(1u << 5);
    g_raw[0][CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X] = 64u;
    g_raw[1][CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X] = 192u;

    assert(ghc_local_input_poll_ps3(&snapshot) == GHC_LOCAL_INPUT_OK);
    assert(g_info_calls == 1u);
    assert(g_data_calls[0] == 1u && g_data_calls[1] == 1u);
    assert(snapshot.connected_mask == 0x03u);
    assert(snapshot.connected_count == 2u);
    assert(snapshot.read_error_mask == 0u);
    assert(snapshot.players[0].connected == 1u);
    assert(snapshot.players[1].connected == 1u);
    assert((snapshot.players[0].buttons & GHC_LOCAL_BUTTON_CROSS) != 0u);
    assert((snapshot.players[0].buttons & GHC_LOCAL_BUTTON_CIRCLE) == 0u);
    assert((snapshot.players[1].buttons & GHC_LOCAL_BUTTON_CIRCLE) != 0u);
    assert((snapshot.players[1].buttons & GHC_LOCAL_BUTTON_CROSS) == 0u);
    assert(snapshot.players[0].left_x == 64u);
    assert(snapshot.players[1].left_x == 192u);
}

static void test_disconnected_port_is_not_polled(void)
{
    GhcLocalInputSnapshot snapshot;
    reset_mocks();
    g_connected_mask = 0x01u;
    g_data_length[0] = 24;

    assert(ghc_local_input_poll_ps3(&snapshot) == GHC_LOCAL_INPUT_OK);
    assert(snapshot.connected_mask == 0x01u);
    assert(snapshot.connected_count == 1u);
    assert(g_data_calls[0] == 1u);
    assert(g_data_calls[1] == 0u);
}

static void test_port_read_error_is_isolated_to_that_port(void)
{
    GhcLocalInputSnapshot snapshot;
    reset_mocks();
    g_connected_mask = 0x03u;
    g_data_length[0] = 24;
    g_data_result[1] = CELL_PAD_ERROR_NO_DEVICE;

    assert(ghc_local_input_poll_ps3(&snapshot) == GHC_LOCAL_INPUT_OK);
    assert(snapshot.connected_mask == 0x01u);
    assert(snapshot.connected_count == 1u);
    assert(snapshot.read_error_mask == 0x02u);
    assert(snapshot.players[0].connected == 1u);
    assert(snapshot.players[1].connected == 0u);
}

static void test_short_or_invalid_data_is_marked_as_read_error(void)
{
    GhcLocalInputSnapshot snapshot;
    reset_mocks();
    g_connected_mask = 0x01u;
    g_data_length[0] = 7;
    assert(ghc_local_input_poll_ps3(&snapshot) == GHC_LOCAL_INPUT_OK);
    assert(snapshot.connected_mask == 0u);
    assert(snapshot.connected_count == 0u);
    assert(snapshot.read_error_mask == 0x01u);
}

static void test_info_error_clears_snapshot_and_skips_data_calls(void)
{
    GhcLocalInputSnapshot snapshot;
    memset(&snapshot, 0xff, sizeof(snapshot));
    reset_mocks();
    g_connected_mask = 0x03u;
    g_info_result = CELL_PAD_ERROR_NO_DEVICE;

    assert(ghc_local_input_poll_ps3(&snapshot) == GHC_LOCAL_INPUT_ERROR_PAD_INFO);
    assert(g_info_calls == 1u);
    assert(g_data_calls[0] == 0u && g_data_calls[1] == 0u);
    assert(snapshot.connected_mask == 0u);
    assert(snapshot.connected_count == 0u);
    assert(snapshot.read_error_mask == 0u);
    assert(snapshot.players[0].connected == 0u);
    assert(snapshot.players[1].connected == 0u);
}

static void test_null_snapshot_is_rejected(void)
{
    reset_mocks();
    assert(ghc_local_input_poll_ps3(0) == GHC_LOCAL_INPUT_ERROR_ARGUMENT);
    assert(g_info_calls == 0u);
}

int main(void)
{
    test_both_ports_return_independent_button_and_stick_data();
    test_disconnected_port_is_not_polled();
    test_port_read_error_is_isolated_to_that_port();
    test_short_or_invalid_data_is_marked_as_read_error();
    test_info_error_clears_snapshot_and_skips_data_calls();
    test_null_snapshot_is_rejected();
    puts("Ghosts PS3 local input adapter mock tests: PASS");
    return 0;
}
