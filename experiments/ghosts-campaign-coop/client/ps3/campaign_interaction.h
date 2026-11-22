#ifndef GHC_CAMPAIGN_INTERACTION_H
#define GHC_CAMPAIGN_INTERACTION_H

#include <stdint.h>

#define GHC_CAMPAIGN_INTERACTION_PLAYER_COUNT 2u
#define GHC_CAMPAIGN_INTERACTION_SPOT_COUNT 2u
#define GHC_CAMPAIGN_INTERACTION_NO_OWNER 0xffu
#define GHC_CAMPAIGN_INTERACTION_NO_SPOT 0xffu

typedef enum {
    GHC_CAMPAIGN_INTERACTION_OK = 0,
    GHC_CAMPAIGN_INTERACTION_INVALID_ARGUMENT = 1,
    GHC_CAMPAIGN_INTERACTION_SESSION_NOT_READY = 2,
    GHC_CAMPAIGN_INTERACTION_INVALID_PLAYER_SLOT = 3,
    GHC_CAMPAIGN_INTERACTION_INVALID_SPOT = 4,
    GHC_CAMPAIGN_INTERACTION_ALREADY_OWNED = 5,
    GHC_CAMPAIGN_INTERACTION_ALREADY_COMPLETE = 6,
    GHC_CAMPAIGN_INTERACTION_NOT_OWNER = 7,
    GHC_CAMPAIGN_INTERACTION_PLAYER_BUSY = 8,
    GHC_CAMPAIGN_INTERACTION_NO_ACTIVE_INTERACTION = 9,
    GHC_CAMPAIGN_INTERACTION_BUSY = 10,
    GHC_CAMPAIGN_INTERACTION_PREREQUISITE_NOT_COMPLETE = 11
} GhcCampaignInteractionStatus;

typedef struct {
    uint8_t owner_by_spot[GHC_CAMPAIGN_INTERACTION_SPOT_COUNT];
    uint8_t active_spot_by_player[GHC_CAMPAIGN_INTERACTION_PLAYER_COUNT];
    uint8_t completed_mask;
    uint8_t reserved;
} GhcCampaignInteractionState;

/*
 * Clockwork has two sequential drill stages on one vault door, not two
 * independent simultaneous stations. Only one player may own the active
 * drill stage globally. Stage 1 (spot 0) must complete before Stage 2
 * (spot 1) can begin. The owner may change between stages.
 */
void ghc_campaign_interaction_reset(GhcCampaignInteractionState *state);

GhcCampaignInteractionStatus ghc_campaign_interaction_begin(
    GhcCampaignInteractionState *state,
    uint8_t session_ready,
    uint8_t player_slot,
    uint8_t spot
);

GhcCampaignInteractionStatus ghc_campaign_interaction_complete(
    GhcCampaignInteractionState *state,
    uint8_t player_slot,
    uint8_t spot
);

GhcCampaignInteractionStatus ghc_campaign_interaction_cancel(
    GhcCampaignInteractionState *state,
    uint8_t player_slot,
    uint8_t spot
);

/* Release the current owner on death/disconnect/cutscene exit, without completion. */
GhcCampaignInteractionStatus ghc_campaign_interaction_abort_player(
    GhcCampaignInteractionState *state,
    uint8_t player_slot
);

uint8_t ghc_campaign_interaction_owner_for_spot(
    const GhcCampaignInteractionState *state,
    uint8_t spot
);

uint8_t ghc_campaign_interaction_spot_for_player(
    const GhcCampaignInteractionState *state,
    uint8_t player_slot
);

uint8_t ghc_campaign_interaction_is_complete(
    const GhcCampaignInteractionState *state,
    uint8_t spot
);

#endif
