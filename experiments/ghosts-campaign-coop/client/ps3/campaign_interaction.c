#include "campaign_interaction.h"

static int valid_player(uint8_t player_slot)
{
    return player_slot < GHC_CAMPAIGN_INTERACTION_PLAYER_COUNT;
}

static int valid_spot(uint8_t spot)
{
    return spot < GHC_CAMPAIGN_INTERACTION_SPOT_COUNT;
}

void ghc_campaign_interaction_reset(GhcCampaignInteractionState *state)
{
    uint32_t i;
    if (!state) return;

    state->completed_mask = 0u;
    state->reserved = 0u;
    for (i = 0; i < GHC_CAMPAIGN_INTERACTION_SPOT_COUNT; ++i)
        state->owner_by_spot[i] = GHC_CAMPAIGN_INTERACTION_NO_OWNER;
    for (i = 0; i < GHC_CAMPAIGN_INTERACTION_PLAYER_COUNT; ++i)
        state->active_spot_by_player[i] = GHC_CAMPAIGN_INTERACTION_NO_SPOT;
}

GhcCampaignInteractionStatus ghc_campaign_interaction_begin(
    GhcCampaignInteractionState *state,
    uint8_t session_ready,
    uint8_t player_slot,
    uint8_t spot
)
{
    uint32_t i;

    if (!state) return GHC_CAMPAIGN_INTERACTION_INVALID_ARGUMENT;
    if (!session_ready) return GHC_CAMPAIGN_INTERACTION_SESSION_NOT_READY;
    if (!valid_player(player_slot)) return GHC_CAMPAIGN_INTERACTION_INVALID_PLAYER_SLOT;
    if (!valid_spot(spot)) return GHC_CAMPAIGN_INTERACTION_INVALID_SPOT;
    if ((state->completed_mask & (uint8_t)(1u << spot)) != 0u)
        return GHC_CAMPAIGN_INTERACTION_ALREADY_COMPLETE;
    if (state->active_spot_by_player[player_slot] != GHC_CAMPAIGN_INTERACTION_NO_SPOT)
        return GHC_CAMPAIGN_INTERACTION_PLAYER_BUSY;
    if (state->owner_by_spot[spot] != GHC_CAMPAIGN_INTERACTION_NO_OWNER)
        return GHC_CAMPAIGN_INTERACTION_ALREADY_OWNED;

    for (i = 0; i < GHC_CAMPAIGN_INTERACTION_PLAYER_COUNT; ++i) {
        if (state->active_spot_by_player[i] != GHC_CAMPAIGN_INTERACTION_NO_SPOT)
            return GHC_CAMPAIGN_INTERACTION_BUSY;
    }

    if (spot == 1u && (state->completed_mask & 0x01u) == 0u)
        return GHC_CAMPAIGN_INTERACTION_PREREQUISITE_NOT_COMPLETE;

    state->owner_by_spot[spot] = player_slot;
    state->active_spot_by_player[player_slot] = spot;
    return GHC_CAMPAIGN_INTERACTION_OK;
}

static GhcCampaignInteractionStatus finish_interaction(
    GhcCampaignInteractionState *state,
    uint8_t player_slot,
    uint8_t spot,
    uint8_t completed
)
{
    if (!state) return GHC_CAMPAIGN_INTERACTION_INVALID_ARGUMENT;
    if (!valid_player(player_slot)) return GHC_CAMPAIGN_INTERACTION_INVALID_PLAYER_SLOT;
    if (!valid_spot(spot)) return GHC_CAMPAIGN_INTERACTION_INVALID_SPOT;
    if (state->owner_by_spot[spot] == GHC_CAMPAIGN_INTERACTION_NO_OWNER)
        return GHC_CAMPAIGN_INTERACTION_NO_ACTIVE_INTERACTION;
    if (state->owner_by_spot[spot] != player_slot ||
        state->active_spot_by_player[player_slot] != spot)
        return GHC_CAMPAIGN_INTERACTION_NOT_OWNER;

    if (completed)
        state->completed_mask |= (uint8_t)(1u << spot);

    state->owner_by_spot[spot] = GHC_CAMPAIGN_INTERACTION_NO_OWNER;
    state->active_spot_by_player[player_slot] = GHC_CAMPAIGN_INTERACTION_NO_SPOT;
    return GHC_CAMPAIGN_INTERACTION_OK;
}

GhcCampaignInteractionStatus ghc_campaign_interaction_complete(
    GhcCampaignInteractionState *state,
    uint8_t player_slot,
    uint8_t spot
)
{
    return finish_interaction(state, player_slot, spot, 1u);
}

GhcCampaignInteractionStatus ghc_campaign_interaction_cancel(
    GhcCampaignInteractionState *state,
    uint8_t player_slot,
    uint8_t spot
)
{
    return finish_interaction(state, player_slot, spot, 0u);
}

GhcCampaignInteractionStatus ghc_campaign_interaction_abort_player(
    GhcCampaignInteractionState *state,
    uint8_t player_slot
)
{
    uint8_t spot;
    if (!state) return GHC_CAMPAIGN_INTERACTION_INVALID_ARGUMENT;
    if (!valid_player(player_slot)) return GHC_CAMPAIGN_INTERACTION_INVALID_PLAYER_SLOT;

    spot = state->active_spot_by_player[player_slot];
    if (!valid_spot(spot))
        return GHC_CAMPAIGN_INTERACTION_NO_ACTIVE_INTERACTION;
    if (state->owner_by_spot[spot] != player_slot)
        return GHC_CAMPAIGN_INTERACTION_NOT_OWNER;

    state->owner_by_spot[spot] = GHC_CAMPAIGN_INTERACTION_NO_OWNER;
    state->active_spot_by_player[player_slot] = GHC_CAMPAIGN_INTERACTION_NO_SPOT;
    return GHC_CAMPAIGN_INTERACTION_OK;
}

uint8_t ghc_campaign_interaction_owner_for_spot(
    const GhcCampaignInteractionState *state,
    uint8_t spot
)
{
    if (!state || !valid_spot(spot))
        return GHC_CAMPAIGN_INTERACTION_NO_OWNER;
    return state->owner_by_spot[spot];
}

uint8_t ghc_campaign_interaction_spot_for_player(
    const GhcCampaignInteractionState *state,
    uint8_t player_slot
)
{
    if (!state || !valid_player(player_slot))
        return GHC_CAMPAIGN_INTERACTION_NO_SPOT;
    return state->active_spot_by_player[player_slot];
}

uint8_t ghc_campaign_interaction_is_complete(
    const GhcCampaignInteractionState *state,
    uint8_t spot
)
{
    if (!state || !valid_spot(spot)) return 0u;
    return (uint8_t)((state->completed_mask >> spot) & 1u);
}
