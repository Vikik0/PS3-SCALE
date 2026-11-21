#include "local_input.h"

#define GHC_PAD_INDEX_DIGITAL1 2u
#define GHC_PAD_INDEX_DIGITAL2 3u
#define GHC_PAD_INDEX_RIGHT_X 4u
#define GHC_PAD_INDEX_RIGHT_Y 5u
#define GHC_PAD_INDEX_LEFT_X 6u
#define GHC_PAD_INDEX_LEFT_Y 7u
#define GHC_PAD_INDEX_L2_PRESSURE 18u
#define GHC_PAD_INDEX_R2_PRESSURE 19u

#define GHC_PAD_D1_LEFT   (1u << 7)
#define GHC_PAD_D1_DOWN   (1u << 6)
#define GHC_PAD_D1_RIGHT  (1u << 5)
#define GHC_PAD_D1_UP     (1u << 4)
#define GHC_PAD_D1_START  (1u << 3)
#define GHC_PAD_D1_R3     (1u << 2)
#define GHC_PAD_D1_L3     (1u << 1)
#define GHC_PAD_D1_SELECT (1u << 0)

#define GHC_PAD_D2_SQUARE   (1u << 7)
#define GHC_PAD_D2_CROSS    (1u << 6)
#define GHC_PAD_D2_CIRCLE   (1u << 5)
#define GHC_PAD_D2_TRIANGLE (1u << 4)
#define GHC_PAD_D2_R1       (1u << 3)
#define GHC_PAD_D2_L1       (1u << 2)
#define GHC_PAD_D2_R2       (1u << 1)
#define GHC_PAD_D2_L2       (1u << 0)

static void ghc_map_button(uint32_t *buttons, uint16_t raw, uint16_t mask, uint32_t mapped)
{
    if ((raw & mask) != 0u) *buttons |= mapped;
}

static void ghc_local_player_clear(GhcLocalPlayerInput *player, uint8_t port)
{
    player->connected = 0u;
    player->port = port;
    player->left_x = 0u;
    player->left_y = 0u;
    player->right_x = 0u;
    player->right_y = 0u;
    player->l2_pressure = 0u;
    player->r2_pressure = 0u;
    player->digital1 = 0u;
    player->digital2 = 0u;
    player->buttons = 0u;
}

void ghc_local_input_snapshot_clear(GhcLocalInputSnapshot *snapshot)
{
    uint32_t i;
    if (!snapshot) return;
    snapshot->connected_mask = 0u;
    snapshot->connected_count = 0u;
    snapshot->read_error_mask = 0u;
    snapshot->reserved = 0u;
    for (i = 0; i < GHC_LOCAL_INPUT_PLAYER_COUNT; ++i) {
        ghc_local_player_clear(&snapshot->players[i], (uint8_t)i);
    }
}

int ghc_local_input_decode_port(
    uint8_t port,
    uint8_t connected,
    int32_t data_length,
    const uint16_t *button_data,
    GhcLocalPlayerInput *output
)
{
    if (!output || port >= GHC_LOCAL_INPUT_PLAYER_COUNT) {
        return GHC_LOCAL_INPUT_ERROR_ARGUMENT;
    }

    ghc_local_player_clear(output, port);
    if (!connected) return GHC_LOCAL_INPUT_OK;

    if (!button_data || data_length < 8 || data_length > (int32_t)GHC_PAD_BUTTON_DATA_COUNT) {
        return GHC_LOCAL_INPUT_ERROR_DATA;
    }

    output->connected = 1u;
    output->digital1 = button_data[GHC_PAD_INDEX_DIGITAL1];
    output->digital2 = button_data[GHC_PAD_INDEX_DIGITAL2];
    output->right_x = (uint8_t)button_data[GHC_PAD_INDEX_RIGHT_X];
    output->right_y = (uint8_t)button_data[GHC_PAD_INDEX_RIGHT_Y];
    output->left_x = (uint8_t)button_data[GHC_PAD_INDEX_LEFT_X];
    output->left_y = (uint8_t)button_data[GHC_PAD_INDEX_LEFT_Y];

    if (data_length > (int32_t)GHC_PAD_INDEX_R2_PRESSURE) {
        output->l2_pressure = (uint8_t)button_data[GHC_PAD_INDEX_L2_PRESSURE];
        output->r2_pressure = (uint8_t)button_data[GHC_PAD_INDEX_R2_PRESSURE];
    }

    ghc_map_button(&output->buttons, output->digital1, GHC_PAD_D1_UP, GHC_LOCAL_BUTTON_UP);
    ghc_map_button(&output->buttons, output->digital1, GHC_PAD_D1_DOWN, GHC_LOCAL_BUTTON_DOWN);
    ghc_map_button(&output->buttons, output->digital1, GHC_PAD_D1_LEFT, GHC_LOCAL_BUTTON_LEFT);
    ghc_map_button(&output->buttons, output->digital1, GHC_PAD_D1_RIGHT, GHC_LOCAL_BUTTON_RIGHT);
    ghc_map_button(&output->buttons, output->digital1, GHC_PAD_D1_START, GHC_LOCAL_BUTTON_START);
    ghc_map_button(&output->buttons, output->digital1, GHC_PAD_D1_SELECT, GHC_LOCAL_BUTTON_SELECT);
    ghc_map_button(&output->buttons, output->digital1, GHC_PAD_D1_L3, GHC_LOCAL_BUTTON_L3);
    ghc_map_button(&output->buttons, output->digital1, GHC_PAD_D1_R3, GHC_LOCAL_BUTTON_R3);

    ghc_map_button(&output->buttons, output->digital2, GHC_PAD_D2_SQUARE, GHC_LOCAL_BUTTON_SQUARE);
    ghc_map_button(&output->buttons, output->digital2, GHC_PAD_D2_CROSS, GHC_LOCAL_BUTTON_CROSS);
    ghc_map_button(&output->buttons, output->digital2, GHC_PAD_D2_CIRCLE, GHC_LOCAL_BUTTON_CIRCLE);
    ghc_map_button(&output->buttons, output->digital2, GHC_PAD_D2_TRIANGLE, GHC_LOCAL_BUTTON_TRIANGLE);
    ghc_map_button(&output->buttons, output->digital2, GHC_PAD_D2_R1, GHC_LOCAL_BUTTON_R1);
    ghc_map_button(&output->buttons, output->digital2, GHC_PAD_D2_L1, GHC_LOCAL_BUTTON_L1);
    ghc_map_button(&output->buttons, output->digital2, GHC_PAD_D2_R2, GHC_LOCAL_BUTTON_R2);
    ghc_map_button(&output->buttons, output->digital2, GHC_PAD_D2_L2, GHC_LOCAL_BUTTON_L2);

    return GHC_LOCAL_INPUT_OK;
}
