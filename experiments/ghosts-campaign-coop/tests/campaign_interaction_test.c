#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../client/ps3/campaign_interaction.h"

static GhcCampaignInteractionState state;

static void reset(void)
{
    memset(&state, 0xa5, sizeof(state));
    ghc_campaign_interaction_reset(&state);
}

static void test_reset_initializes_all_slots(void)
{
    uint32_t i;
    reset();
    assert(state.completed_mask == 0u);
    for (i = 0; i < GHC_CAMPAIGN_INTERACTION_SPOT_COUNT; ++i)
        assert(state.owner_by_spot[i] == GHC_CAMPAIGN_INTERACTION_NO_OWNER);
    for (i = 0; i < GHC_CAMPAIGN_INTERACTION_PLAYER_COUNT; ++i)
        assert(state.active_spot_by_player[i] == GHC_CAMPAIGN_INTERACTION_NO_SPOT);
}

static void test_only_one_global_drill_owner_is_allowed(void)
{
    reset();
    assert(ghc_campaign_interaction_begin(&state, 1u, 0u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_begin(&state, 1u, 1u, 1u) == GHC_CAMPAIGN_INTERACTION_BUSY);
    assert(ghc_campaign_interaction_owner_for_spot(&state, 0u) == 0u);
    assert(ghc_campaign_interaction_owner_for_spot(&state, 1u) == GHC_CAMPAIGN_INTERACTION_NO_OWNER);
    assert(ghc_campaign_interaction_spot_for_player(&state, 0u) == 0u);
    assert(ghc_campaign_interaction_spot_for_player(&state, 1u) == GHC_CAMPAIGN_INTERACTION_NO_SPOT);
}

static void test_two_sequential_stages_may_have_different_owners(void)
{
    reset();
    assert(ghc_campaign_interaction_begin(&state, 1u, 0u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_complete(&state, 0u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_is_complete(&state, 0u) == 1u);
    assert(ghc_campaign_interaction_begin(&state, 1u, 1u, 1u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_owner_for_spot(&state, 1u) == 1u);
    assert(ghc_campaign_interaction_complete(&state, 1u, 1u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_is_complete(&state, 1u) == 1u);
}

static void test_second_stage_requires_first_stage_completion(void)
{
    reset();
    assert(ghc_campaign_interaction_begin(&state, 1u, 1u, 1u) ==
           GHC_CAMPAIGN_INTERACTION_PREREQUISITE_NOT_COMPLETE);
    assert(ghc_campaign_interaction_begin(&state, 1u, 0u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_cancel(&state, 0u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_begin(&state, 1u, 1u, 1u) ==
           GHC_CAMPAIGN_INTERACTION_PREREQUISITE_NOT_COMPLETE);
}

static void test_same_spot_has_exactly_one_owner(void)
{
    reset();
    assert(ghc_campaign_interaction_begin(&state, 1u, 0u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_begin(&state, 1u, 1u, 0u) ==
           GHC_CAMPAIGN_INTERACTION_ALREADY_OWNED);
    assert(ghc_campaign_interaction_owner_for_spot(&state, 0u) == 0u);
}

static void test_one_player_cannot_start_two_interactions(void)
{
    reset();
    assert(ghc_campaign_interaction_begin(&state, 1u, 0u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_begin(&state, 1u, 0u, 1u) ==
           GHC_CAMPAIGN_INTERACTION_PLAYER_BUSY);
}

static void test_completion_is_shared_and_cannot_be_repeated(void)
{
    reset();
    assert(ghc_campaign_interaction_begin(&state, 1u, 0u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_complete(&state, 0u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_is_complete(&state, 0u) == 1u);
    assert(ghc_campaign_interaction_owner_for_spot(&state, 0u) == GHC_CAMPAIGN_INTERACTION_NO_OWNER);
    assert(ghc_campaign_interaction_begin(&state, 1u, 1u, 0u) ==
           GHC_CAMPAIGN_INTERACTION_ALREADY_COMPLETE);
    assert(ghc_campaign_interaction_complete(&state, 0u, 0u) ==
           GHC_CAMPAIGN_INTERACTION_NO_ACTIVE_INTERACTION);
}

static void test_cancel_releases_owner_without_completing_stage(void)
{
    reset();
    assert(ghc_campaign_interaction_begin(&state, 1u, 1u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_cancel(&state, 1u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_is_complete(&state, 0u) == 0u);
    assert(ghc_campaign_interaction_owner_for_spot(&state, 0u) == GHC_CAMPAIGN_INTERACTION_NO_OWNER);
    assert(ghc_campaign_interaction_begin(&state, 1u, 0u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
}

static void test_non_owner_cannot_complete_or_cancel_another_players_stage(void)
{
    reset();
    assert(ghc_campaign_interaction_begin(&state, 1u, 0u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_complete(&state, 1u, 0u) == GHC_CAMPAIGN_INTERACTION_NOT_OWNER);
    assert(ghc_campaign_interaction_cancel(&state, 1u, 0u) == GHC_CAMPAIGN_INTERACTION_NOT_OWNER);
    assert(ghc_campaign_interaction_owner_for_spot(&state, 0u) == 0u);
    assert(ghc_campaign_interaction_is_complete(&state, 0u) == 0u);
}

static void test_abort_on_death_or_disconnect_releases_current_stage(void)
{
    reset();
    assert(ghc_campaign_interaction_begin(&state, 1u, 0u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_abort_player(&state, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_owner_for_spot(&state, 0u) == GHC_CAMPAIGN_INTERACTION_NO_OWNER);
    assert(ghc_campaign_interaction_is_complete(&state, 0u) == 0u);
    assert(ghc_campaign_interaction_begin(&state, 1u, 1u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
}

static void test_session_gate_and_invalid_inputs_fail_closed(void)
{
    reset();
    assert(ghc_campaign_interaction_begin(&state, 0u, 0u, 0u) ==
           GHC_CAMPAIGN_INTERACTION_SESSION_NOT_READY);
    assert(ghc_campaign_interaction_begin(&state, 1u, 2u, 0u) ==
           GHC_CAMPAIGN_INTERACTION_INVALID_PLAYER_SLOT);
    assert(ghc_campaign_interaction_begin(&state, 1u, 0u, 2u) ==
           GHC_CAMPAIGN_INTERACTION_INVALID_SPOT);
    assert(ghc_campaign_interaction_begin(NULL, 1u, 0u, 0u) ==
           GHC_CAMPAIGN_INTERACTION_INVALID_ARGUMENT);
    assert(ghc_campaign_interaction_owner_for_spot(&state, 2u) ==
           GHC_CAMPAIGN_INTERACTION_NO_OWNER);
}

static void test_player_abort_requires_an_active_owned_interaction(void)
{
    reset();
    assert(ghc_campaign_interaction_abort_player(&state, 0u) ==
           GHC_CAMPAIGN_INTERACTION_NO_ACTIVE_INTERACTION);
    assert(ghc_campaign_interaction_abort_player(&state, 2u) ==
           GHC_CAMPAIGN_INTERACTION_INVALID_PLAYER_SLOT);
    assert(ghc_campaign_interaction_begin(&state, 1u, 1u, 0u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_abort_player(&state, 1u) == GHC_CAMPAIGN_INTERACTION_OK);
    assert(ghc_campaign_interaction_spot_for_player(&state, 1u) ==
           GHC_CAMPAIGN_INTERACTION_NO_SPOT);
}

int main(void)
{
    test_reset_initializes_all_slots();
    test_only_one_global_drill_owner_is_allowed();
    test_two_sequential_stages_may_have_different_owners();
    test_second_stage_requires_first_stage_completion();
    test_same_spot_has_exactly_one_owner();
    test_one_player_cannot_start_two_interactions();
    test_completion_is_shared_and_cannot_be_repeated();
    test_cancel_releases_owner_without_completing_stage();
    test_non_owner_cannot_complete_or_cancel_another_players_stage();
    test_abort_on_death_or_disconnect_releases_current_stage();
    test_session_gate_and_invalid_inputs_fail_closed();
    test_player_abort_requires_an_active_owned_interaction();
    puts("Campaign interaction sequencing tests: PASS");
    return 0;
}
