/* Ghosts Campaign Co-op PRX bootstrap. */
#include <stddef.h>
#include <stdint.h>
#include "../mission_rules.h"
#include "../local_input.h"

#define SYS_PRX_RESIDENT 0

static volatile uint32_t g_module_state = 0;
static GhcMission g_mission;
static GhcLocalInputSnapshot g_local_input;
static GhcLocalInputTracker g_local_tracker;
static GhcLocalInputFrame g_local_frame;
static volatile uint32_t g_start_count = 0;
static volatile int32_t g_local_input_last_result = GHC_LOCAL_INPUT_ERROR_DATA;
static volatile uint32_t g_local_input_poll_count = 0;

int module_start(size_t args, void *argp)
{
    (void)args;
    (void)argp;
    ghc_mission_init(&g_mission, 0);
    ghc_local_input_snapshot_clear(&g_local_input);
    ghc_local_input_tracker_reset(&g_local_tracker);
    (void)ghc_local_input_tracker_update(
        &g_local_input, &g_local_tracker, GHC_LOCAL_STICK_DEADZONE_DEFAULT, &g_local_frame
    );
    g_start_count++;
    g_module_state = 1;
    return SYS_PRX_RESIDENT;
}

int module_stop(size_t args, void *argp)
{
    (void)args;
    (void)argp;
    g_module_state = 0;
    return SYS_PRX_RESIDENT;
}

uint32_t ghc_module_state(void) { return g_module_state; }
uint32_t ghc_module_start_count(void) { return g_start_count; }
uint32_t ghc_mission_revision(void) { return g_mission.revision; }
uint32_t ghc_mission_phase(void) { return g_mission.phase; }

/* Diagnostic poll entry point for a future verified Campaign frame adapter. */
int32_t ghc_local_input_refresh(void)
{
    int result = ghc_local_input_poll_ps3(&g_local_input);
    if (result == GHC_LOCAL_INPUT_OK) {
        result = ghc_local_input_tracker_update(
            &g_local_input, &g_local_tracker, GHC_LOCAL_STICK_DEADZONE_DEFAULT, &g_local_frame
        );
    }
    g_local_input_last_result = (int32_t)result;
    g_local_input_poll_count++;
    return (int32_t)result;
}

uint32_t ghc_local_input_last_error(void) { return (uint32_t)g_local_input_last_result; }
uint32_t ghc_local_input_poll_total(void) { return g_local_input_poll_count; }
uint32_t ghc_local_input_connected_mask(void) { return g_local_frame.connected_mask; }
uint32_t ghc_local_input_valid_mask(void) { return g_local_frame.valid_mask; }
uint32_t ghc_local_input_read_error_mask(void) { return g_local_input.read_error_mask; }

uint32_t ghc_local_input_player_connected(uint32_t player_slot)
{
    if (player_slot < 1u || player_slot > GHC_LOCAL_INPUT_PLAYER_COUNT) return 0u;
    return g_local_frame.players[player_slot - 1u].connected;
}

uint32_t ghc_local_input_player_buttons(uint32_t player_slot)
{
    if (player_slot < 1u || player_slot > GHC_LOCAL_INPUT_PLAYER_COUNT) return 0u;
    return g_local_frame.players[player_slot - 1u].buttons;
}

uint32_t ghc_local_input_player_pressed_buttons(uint32_t player_slot)
{
    if (player_slot < 1u || player_slot > GHC_LOCAL_INPUT_PLAYER_COUNT) return 0u;
    return g_local_frame.players[player_slot - 1u].pressed;
}

uint32_t ghc_local_input_player_released_buttons(uint32_t player_slot)
{
    if (player_slot < 1u || player_slot > GHC_LOCAL_INPUT_PLAYER_COUNT) return 0u;
    return g_local_frame.players[player_slot - 1u].released;
}

uint32_t ghc_local_input_player_left_stick(uint32_t player_slot)
{
    const GhcLocalPlayerInput *player;
    if (player_slot < 1u || player_slot > GHC_LOCAL_INPUT_PLAYER_COUNT) return 0u;
    player = &g_local_input.players[player_slot - 1u];
    return ((uint32_t)player->left_x << 8) | (uint32_t)player->left_y;
}

uint32_t ghc_local_input_player_right_stick(uint32_t player_slot)
{
    const GhcLocalPlayerInput *player;
    if (player_slot < 1u || player_slot > GHC_LOCAL_INPUT_PLAYER_COUNT) return 0u;
    player = &g_local_input.players[player_slot - 1u];
    return ((uint32_t)player->right_x << 8) | (uint32_t)player->right_y;
}

uint32_t ghc_local_input_player_left_stick_normalized(uint32_t player_slot)
{
    const GhcLocalPlayerFrame *player;
    if (player_slot < 1u || player_slot > GHC_LOCAL_INPUT_PLAYER_COUNT) return 0u;
    player = &g_local_frame.players[player_slot - 1u];
    return ((uint32_t)(uint8_t)player->left_x << 8) | (uint32_t)(uint8_t)player->left_y;
}

uint32_t ghc_local_input_player_right_stick_normalized(uint32_t player_slot)
{
    const GhcLocalPlayerFrame *player;
    if (player_slot < 1u || player_slot > GHC_LOCAL_INPUT_PLAYER_COUNT) return 0u;
    player = &g_local_frame.players[player_slot - 1u];
    return ((uint32_t)(uint8_t)player->right_x << 8) | (uint32_t)(uint8_t)player->right_y;
}

uint32_t ghc_local_input_player_l2_pressure(uint32_t player_slot)
{
    if (player_slot < 1u || player_slot > GHC_LOCAL_INPUT_PLAYER_COUNT) return 0u;
    return g_local_frame.players[player_slot - 1u].l2_pressure;
}

uint32_t ghc_local_input_player_r2_pressure(uint32_t player_slot)
{
    if (player_slot < 1u || player_slot > GHC_LOCAL_INPUT_PLAYER_COUNT) return 0u;
    return g_local_frame.players[player_slot - 1u].r2_pressure;
}
