#include "local_player_gate.h"

static GhcLocalPlayerGateStatus gh_fail(
    GhcLocalPlayerGateResult *result,
    uint8_t slot,
    GhcLocalPlayerGateStatus status
) {
    if (result) {
        result->ready = 0u;
        result->failing_slot = slot;
        result->status = (uint8_t)status;
        result->reserved = 0u;
    }
    return status;
}

GhcLocalPlayerGateStatus ghc_local_player_gate_validate(
    const GhcLocalPlayerObservation players[GHC_LOCAL_PLAYER_SLOT_COUNT],
    GhcLocalPlayerGateResult *result
) {
    uint8_t slot;
    uint8_t i;
    uint8_t j;
    const GhcLocalPlayerObservation *p1;
    const GhcLocalPlayerObservation *p2;
    uint64_t p1_addresses[6];
    uint64_t p2_addresses[6];

    if (!result) {
        return GHC_LOCAL_GATE_INVALID_ARGUMENT;
    }
    if (!players) {
        return gh_fail(result, GHC_LOCAL_PLAYER_NO_SLOT, GHC_LOCAL_GATE_INVALID_ARGUMENT);
    }

    result->ready = 0u;
    result->failing_slot = GHC_LOCAL_PLAYER_NO_SLOT;
    result->status = GHC_LOCAL_GATE_INVALID_ARGUMENT;
    result->reserved = 0u;

    for (slot = 0u; slot < GHC_LOCAL_PLAYER_SLOT_COUNT; ++slot) {
        const GhcLocalPlayerObservation *player = &players[slot];
        uint8_t expected_primary = (uint8_t)(slot == 0u);
        uint8_t expected_port = slot;
        uint8_t expected_role = slot == 0u
            ? GHC_LOCAL_ROLE_LOGAN : GHC_LOCAL_ROLE_GUEST_SOLDIER;

        if (!player->split_screen_active) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_SPLITSCREEN_INACTIVE);
        }
        if (!player->is_split_screen_player) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_NOT_SPLITSCREEN_PLAYER);
        }
        if (player->is_primary != expected_primary) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_PRIMARY_MISMATCH);
        }
        if (player->controller_port != expected_port) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_CONTROLLER_MISMATCH);
        }
        if (!player->input_connected) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_INPUT_NOT_CONNECTED);
        }
        if (!player->input_frame_valid) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_INPUT_FRAME_MISSING);
        }
        if (player->role != expected_role) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_ROLE_MISMATCH);
        }
        if (!player->client_id_valid) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_CLIENT_ID_MISSING);
        }
        if (!player->entity_id_valid) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_ENTITY_ID_MISSING);
        }
        if (!player->local_client_num_valid) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_LOCAL_CLIENT_NUM_MISSING);
        }
        if (!player->gclient_address || !player->gentity_address || !player->player_state_address) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_STATE_ADDRESS_MISSING);
        }
        if (!player->input_command_address) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_INPUT_COMMAND_ADDRESS_MISSING);
        }
        if (!player->view_context_address) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_VIEW_CONTEXT_ADDRESS_MISSING);
        }
        if (!player->hud_context_address) {
            return gh_fail(result, slot, GHC_LOCAL_GATE_HUD_CONTEXT_ADDRESS_MISSING);
        }
    }

    p1 = &players[0];
    p2 = &players[1];
    if (p1->observation_frame != p2->observation_frame) {
        return gh_fail(result, 1u, GHC_LOCAL_GATE_OBSERVATION_FRAME_MISMATCH);
    }
    if (p1->local_client_num == p2->local_client_num) {
        return gh_fail(result, 1u, GHC_LOCAL_GATE_DUPLICATE_LOCAL_CLIENT_NUM);
    }
    if (p1->client_id == p2->client_id) {
        return gh_fail(result, 1u, GHC_LOCAL_GATE_DUPLICATE_CLIENT_ID);
    }
    if (p1->entity_id == p2->entity_id) {
        return gh_fail(result, 1u, GHC_LOCAL_GATE_DUPLICATE_ENTITY_ID);
    }

    p1_addresses[0] = p1->gclient_address;
    p1_addresses[1] = p1->gentity_address;
    p1_addresses[2] = p1->player_state_address;
    p1_addresses[3] = p1->input_command_address;
    p1_addresses[4] = p1->view_context_address;
    p1_addresses[5] = p1->hud_context_address;
    p2_addresses[0] = p2->gclient_address;
    p2_addresses[1] = p2->gentity_address;
    p2_addresses[2] = p2->player_state_address;
    p2_addresses[3] = p2->input_command_address;
    p2_addresses[4] = p2->view_context_address;
    p2_addresses[5] = p2->hud_context_address;

    for (i = 0u; i < 6u; ++i) {
        for (j = 0u; j < 6u; ++j) {
            if (p1_addresses[i] == p2_addresses[j]) {
                if (i < 3u && j < 3u) {
                    return gh_fail(result, 1u, GHC_LOCAL_GATE_DUPLICATE_STATE_ADDRESS);
                }
                return gh_fail(result, 1u, GHC_LOCAL_GATE_DUPLICATE_ENGINE_CONTEXT_ADDRESS);
            }
        }
    }

    result->ready = 1u;
    result->failing_slot = GHC_LOCAL_PLAYER_NO_SLOT;
    result->status = GHC_LOCAL_GATE_READY;
    result->reserved = 0u;
    return GHC_LOCAL_GATE_READY;
}
