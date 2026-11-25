#include "campaign_event_lock_model.h"

static int valid_player(int player_slot)
{
    return player_slot >= 0 && player_slot < (int)GHC_CAMPAIGN_EVENT_LOCK_PLAYER_COUNT;
}

static int valid_event(int event_id)
{
    return event_id >= 0 && event_id < (int)GHC_CAMPAIGN_EVENT_LOCK_COUNT;
}

static int valid_flag(uint8_t value)
{
    return value <= 1u;
}

GhcCampaignEventLockStatus ghc_campaign_event_lock_model_init(
    GhcCampaignEventLockModel *state
)
{
    uint32_t i;
    if (!state) return GHC_CAMPAIGN_EVENT_LOCK_INVALID_ARGUMENT;
    if (state->initialized == 1u) return GHC_CAMPAIGN_EVENT_LOCK_OK;
    if (state->initialized != 0u) return GHC_CAMPAIGN_EVENT_LOCK_STATE_CORRUPT;

    for (i = 0; i < GHC_CAMPAIGN_EVENT_LOCK_COUNT; ++i) {
        state->owner_by_event[i] = GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER;
        state->completed_by_event[i] = 0u;
    }
    for (i = 0; i < GHC_CAMPAIGN_EVENT_LOCK_PLAYER_COUNT; ++i)
        state->active_event_by_player[i] = GHC_CAMPAIGN_EVENT_LOCK_NO_EVENT;

    state->initialized = 1u;
    return GHC_CAMPAIGN_EVENT_LOCK_OK;
}

GhcCampaignEventLockStatus ghc_campaign_event_lock_model_acquire(
    GhcCampaignEventLockModel *state,
    uint8_t session_ready,
    uint8_t native_pair_current,
    int player_slot,
    int event_id
)
{
    GhcCampaignEventLockStatus status;

    if (!state || !valid_flag(session_ready) || !valid_flag(native_pair_current))
        return GHC_CAMPAIGN_EVENT_LOCK_INVALID_ARGUMENT;
    if (!session_ready) return GHC_CAMPAIGN_EVENT_LOCK_SESSION_NOT_READY;
    if (!native_pair_current) {
        if (state->initialized == 1u)
            (void)ghc_campaign_event_lock_model_clear_active(state);
        return GHC_CAMPAIGN_EVENT_LOCK_SESSION_STALE;
    }
    status = ghc_campaign_event_lock_model_init(state);
    if (status != GHC_CAMPAIGN_EVENT_LOCK_OK) return status;
    if (!valid_player(player_slot)) return GHC_CAMPAIGN_EVENT_LOCK_INVALID_PLAYER;
    if (!valid_event(event_id)) return GHC_CAMPAIGN_EVENT_LOCK_INVALID_EVENT;

    if (state->owner_by_event[event_id] != GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER)
        return GHC_CAMPAIGN_EVENT_LOCK_EVENT_BUSY;
    if (state->active_event_by_player[player_slot] != GHC_CAMPAIGN_EVENT_LOCK_NO_EVENT)
        return GHC_CAMPAIGN_EVENT_LOCK_PLAYER_BUSY;
    if (state->completed_by_event[event_id])
        return GHC_CAMPAIGN_EVENT_LOCK_ALREADY_COMPLETE;

    /* No yield is permitted between the checks and these two assignments. */
    state->owner_by_event[event_id] = (int8_t)player_slot;
    state->active_event_by_player[player_slot] = (int8_t)event_id;
    return GHC_CAMPAIGN_EVENT_LOCK_OK;
}

GhcCampaignEventLockStatus ghc_campaign_event_lock_model_release(
    GhcCampaignEventLockModel *state,
    uint8_t session_ready,
    uint8_t native_pair_current,
    int player_slot,
    int event_id,
    uint8_t completed
)
{
    if (!state || !valid_flag(session_ready) || !valid_flag(native_pair_current) || !valid_flag(completed))
        return GHC_CAMPAIGN_EVENT_LOCK_INVALID_ARGUMENT;
    if (state->initialized != 1u) return GHC_CAMPAIGN_EVENT_LOCK_NOT_INITIALIZED;
    if (!session_ready) return GHC_CAMPAIGN_EVENT_LOCK_SESSION_NOT_READY;
    if (!native_pair_current) {
        (void)ghc_campaign_event_lock_model_clear_active(state);
        return GHC_CAMPAIGN_EVENT_LOCK_SESSION_STALE;
    }
    if (!valid_player(player_slot)) return GHC_CAMPAIGN_EVENT_LOCK_INVALID_PLAYER;
    if (!valid_event(event_id)) return GHC_CAMPAIGN_EVENT_LOCK_INVALID_EVENT;

    if (state->owner_by_event[event_id] != player_slot ||
        state->active_event_by_player[player_slot] != event_id)
        return GHC_CAMPAIGN_EVENT_LOCK_NOT_OWNER;

    if (completed) state->completed_by_event[event_id] = 1u;
    state->owner_by_event[event_id] = GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER;
    state->active_event_by_player[player_slot] = GHC_CAMPAIGN_EVENT_LOCK_NO_EVENT;
    return GHC_CAMPAIGN_EVENT_LOCK_OK;
}

GhcCampaignEventLockStatus ghc_campaign_event_lock_model_abort_player(
    GhcCampaignEventLockModel *state,
    int player_slot
)
{
    int event_id;
    if (!state) return GHC_CAMPAIGN_EVENT_LOCK_INVALID_ARGUMENT;
    if (state->initialized != 1u) return GHC_CAMPAIGN_EVENT_LOCK_NOT_INITIALIZED;
    if (!valid_player(player_slot)) return GHC_CAMPAIGN_EVENT_LOCK_INVALID_PLAYER;

    event_id = state->active_event_by_player[player_slot];
    if (!valid_event(event_id)) return GHC_CAMPAIGN_EVENT_LOCK_NO_ACTIVE_EVENT;
    if (state->owner_by_event[event_id] != player_slot)
        return GHC_CAMPAIGN_EVENT_LOCK_STATE_CORRUPT;

    state->owner_by_event[event_id] = GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER;
    state->active_event_by_player[player_slot] = GHC_CAMPAIGN_EVENT_LOCK_NO_EVENT;
    return GHC_CAMPAIGN_EVENT_LOCK_OK;
}

GhcCampaignEventLockStatus ghc_campaign_event_lock_model_clear_active(
    GhcCampaignEventLockModel *state
)
{
    uint32_t player;
    if (!state) return GHC_CAMPAIGN_EVENT_LOCK_INVALID_ARGUMENT;
    if (state->initialized != 1u) return GHC_CAMPAIGN_EVENT_LOCK_NOT_INITIALIZED;
    if (ghc_campaign_event_lock_model_validate(state) != GHC_CAMPAIGN_EVENT_LOCK_OK)
        return GHC_CAMPAIGN_EVENT_LOCK_STATE_CORRUPT;

    for (player = 0; player < GHC_CAMPAIGN_EVENT_LOCK_PLAYER_COUNT; ++player) {
        GhcCampaignEventLockStatus status = ghc_campaign_event_lock_model_abort_player(state, (int)player);
        if (status != GHC_CAMPAIGN_EVENT_LOCK_OK && status != GHC_CAMPAIGN_EVENT_LOCK_NO_ACTIVE_EVENT)
            return status;
    }
    return GHC_CAMPAIGN_EVENT_LOCK_OK;
}

GhcCampaignEventLockStatus ghc_campaign_event_lock_model_reset_event(
    GhcCampaignEventLockModel *state,
    int event_id
)
{
    if (!state) return GHC_CAMPAIGN_EVENT_LOCK_INVALID_ARGUMENT;
    if (state->initialized != 1u) return GHC_CAMPAIGN_EVENT_LOCK_NOT_INITIALIZED;
    if (!valid_event(event_id)) return GHC_CAMPAIGN_EVENT_LOCK_INVALID_EVENT;
    if (state->owner_by_event[event_id] != GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER)
        return GHC_CAMPAIGN_EVENT_LOCK_RESET_BUSY;
    state->completed_by_event[event_id] = 0u;
    return GHC_CAMPAIGN_EVENT_LOCK_OK;
}

int8_t ghc_campaign_event_lock_model_owner(
    const GhcCampaignEventLockModel *state,
    int event_id
)
{
    if (!state || state->initialized != 1u || !valid_event(event_id))
        return GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER;
    return state->owner_by_event[event_id];
}

int8_t ghc_campaign_event_lock_model_active_for_player(
    const GhcCampaignEventLockModel *state,
    int player_slot
)
{
    if (!state || state->initialized != 1u || !valid_player(player_slot))
        return GHC_CAMPAIGN_EVENT_LOCK_NO_EVENT;
    return state->active_event_by_player[player_slot];
}

uint8_t ghc_campaign_event_lock_model_is_complete(
    const GhcCampaignEventLockModel *state,
    int event_id
)
{
    if (!state || state->initialized != 1u || !valid_event(event_id)) return 0u;
    return state->completed_by_event[event_id];
}

uint8_t ghc_campaign_event_lock_model_is_owner(
    const GhcCampaignEventLockModel *state,
    int player_slot,
    int event_id
)
{
    if (!state || state->initialized != 1u || !valid_player(player_slot) || !valid_event(event_id))
        return 0u;
    return (uint8_t)(state->owner_by_event[event_id] == player_slot &&
                     state->active_event_by_player[player_slot] == event_id);
}

GhcCampaignEventLockStatus ghc_campaign_event_lock_model_validate(
    const GhcCampaignEventLockModel *state
)
{
    uint32_t event_id;
    uint32_t player;

    if (!state) return GHC_CAMPAIGN_EVENT_LOCK_INVALID_ARGUMENT;
    if (state->initialized != 1u) return GHC_CAMPAIGN_EVENT_LOCK_NOT_INITIALIZED;

    for (event_id = 0; event_id < GHC_CAMPAIGN_EVENT_LOCK_COUNT; ++event_id) {
        int owner = state->owner_by_event[event_id];
        if (owner < GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER || owner >= (int)GHC_CAMPAIGN_EVENT_LOCK_PLAYER_COUNT)
            return GHC_CAMPAIGN_EVENT_LOCK_STATE_CORRUPT;
        if (!valid_flag(state->completed_by_event[event_id]))
            return GHC_CAMPAIGN_EVENT_LOCK_STATE_CORRUPT;
        if (owner >= 0 && state->active_event_by_player[owner] != (int8_t)event_id)
            return GHC_CAMPAIGN_EVENT_LOCK_STATE_CORRUPT;
    }

    for (player = 0; player < GHC_CAMPAIGN_EVENT_LOCK_PLAYER_COUNT; ++player) {
        int active = state->active_event_by_player[player];
        if (active != GHC_CAMPAIGN_EVENT_LOCK_NO_EVENT && !valid_event(active))
            return GHC_CAMPAIGN_EVENT_LOCK_STATE_CORRUPT;
        if (valid_event(active) && state->owner_by_event[active] != (int8_t)player)
            return GHC_CAMPAIGN_EVENT_LOCK_STATE_CORRUPT;
    }

    return GHC_CAMPAIGN_EVENT_LOCK_OK;
}
