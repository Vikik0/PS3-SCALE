#ifndef GHC_CAMPAIGN_EVENT_LOCK_MODEL_H
#define GHC_CAMPAIGN_EVENT_LOCK_MODEL_H

#include <stdint.h>

#define GHC_CAMPAIGN_EVENT_LOCK_COUNT 16u
#define GHC_CAMPAIGN_EVENT_LOCK_PLAYER_COUNT 2u
#define GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER ((int8_t)-1)
#define GHC_CAMPAIGN_EVENT_LOCK_NO_EVENT ((int8_t)-1)
#define GHC_CAMPAIGN_EVENT_LOCK_MODEL_INITIALIZER { 0u, { 0 }, { 0 }, { 0 } }

typedef enum {
    GHC_CAMPAIGN_EVENT_LOCK_OK = 0,
    GHC_CAMPAIGN_EVENT_LOCK_INVALID_ARGUMENT = 1,
    GHC_CAMPAIGN_EVENT_LOCK_NOT_INITIALIZED = 2,
    GHC_CAMPAIGN_EVENT_LOCK_SESSION_NOT_READY = 3,
    GHC_CAMPAIGN_EVENT_LOCK_SESSION_STALE = 4,
    GHC_CAMPAIGN_EVENT_LOCK_INVALID_PLAYER = 5,
    GHC_CAMPAIGN_EVENT_LOCK_INVALID_EVENT = 6,
    GHC_CAMPAIGN_EVENT_LOCK_EVENT_BUSY = 7,
    GHC_CAMPAIGN_EVENT_LOCK_PLAYER_BUSY = 8,
    GHC_CAMPAIGN_EVENT_LOCK_ALREADY_COMPLETE = 9,
    GHC_CAMPAIGN_EVENT_LOCK_NOT_OWNER = 10,
    GHC_CAMPAIGN_EVENT_LOCK_NO_ACTIVE_EVENT = 11,
    GHC_CAMPAIGN_EVENT_LOCK_RESET_BUSY = 12,
    GHC_CAMPAIGN_EVENT_LOCK_STATE_CORRUPT = 13
} GhcCampaignEventLockStatus;

/*
 * HOST-ONLY contract model for ghc_campaign_event_lock_* in
 * ghc_campaign_local_session.gsc. This model is not linked into the PS3 PRX.
 * Zero-initialize the state (or use GHC_CAMPAIGN_EVENT_LOCK_MODEL_INITIALIZER)
 * before the first init call. Repeated init calls preserve completed events,
 * matching the GSC bridge's idempotent initialization across native-pair rebinds.
 */
typedef struct {
    uint8_t initialized;
    int8_t owner_by_event[GHC_CAMPAIGN_EVENT_LOCK_COUNT];
    int8_t active_event_by_player[GHC_CAMPAIGN_EVENT_LOCK_PLAYER_COUNT];
    uint8_t completed_by_event[GHC_CAMPAIGN_EVENT_LOCK_COUNT];
} GhcCampaignEventLockModel;

GhcCampaignEventLockStatus ghc_campaign_event_lock_model_init(
    GhcCampaignEventLockModel *state
);

GhcCampaignEventLockStatus ghc_campaign_event_lock_model_acquire(
    GhcCampaignEventLockModel *state,
    uint8_t session_ready,
    uint8_t native_pair_current,
    int player_slot,
    int event_id
);

GhcCampaignEventLockStatus ghc_campaign_event_lock_model_release(
    GhcCampaignEventLockModel *state,
    uint8_t session_ready,
    uint8_t native_pair_current,
    int player_slot,
    int event_id,
    uint8_t completed
);

GhcCampaignEventLockStatus ghc_campaign_event_lock_model_abort_player(
    GhcCampaignEventLockModel *state,
    int player_slot
);

/* Clears active ownership only; completed-event history is preserved. */
GhcCampaignEventLockStatus ghc_campaign_event_lock_model_clear_active(
    GhcCampaignEventLockModel *state
);

/* A completed event can be made available again only while it has no owner. */
GhcCampaignEventLockStatus ghc_campaign_event_lock_model_reset_event(
    GhcCampaignEventLockModel *state,
    int event_id
);

int8_t ghc_campaign_event_lock_model_owner(
    const GhcCampaignEventLockModel *state,
    int event_id
);

int8_t ghc_campaign_event_lock_model_active_for_player(
    const GhcCampaignEventLockModel *state,
    int player_slot
);

uint8_t ghc_campaign_event_lock_model_is_complete(
    const GhcCampaignEventLockModel *state,
    int event_id
);

uint8_t ghc_campaign_event_lock_model_is_owner(
    const GhcCampaignEventLockModel *state,
    int player_slot,
    int event_id
);

GhcCampaignEventLockStatus ghc_campaign_event_lock_model_validate(
    const GhcCampaignEventLockModel *state
);

#endif
