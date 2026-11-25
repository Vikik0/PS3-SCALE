#include "campaign_event_lock_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static GhcCampaignEventLockModel state;

static void reset_state(void)
{
    memset(&state, 0, sizeof(state));
    assert(ghc_campaign_event_lock_model_init(&state) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_validate(&state) == GHC_CAMPAIGN_EVENT_LOCK_OK);
}

static void test_init_sets_all_sentinels(void)
{
    uint32_t i;
    reset_state();
    for (i = 0; i < GHC_CAMPAIGN_EVENT_LOCK_COUNT; ++i) {
        assert(ghc_campaign_event_lock_model_owner(&state, (int)i) == GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER);
        assert(ghc_campaign_event_lock_model_is_complete(&state, (int)i) == 0u);
    }
    for (i = 0; i < GHC_CAMPAIGN_EVENT_LOCK_PLAYER_COUNT; ++i)
        assert(ghc_campaign_event_lock_model_active_for_player(&state, (int)i) == GHC_CAMPAIGN_EVENT_LOCK_NO_EVENT);
}

static void test_init_is_idempotent_and_preserves_completed_history(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 0) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_release(&state, 1u, 1u, 0, 0, 1u) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_init(&state) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_is_complete(&state, 0) == 1u);
    assert(ghc_campaign_event_lock_model_owner(&state, 0) == GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER);
}

static void test_acquire_and_release_round_trip(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 1, 3) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_owner(&state, 3) == 1);
    assert(ghc_campaign_event_lock_model_active_for_player(&state, 1) == 3);
    assert(ghc_campaign_event_lock_model_is_owner(&state, 1, 3) == 1u);
    assert(ghc_campaign_event_lock_model_release(&state, 1u, 1u, 1, 3, 0u) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_owner(&state, 3) == GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER);
    assert(ghc_campaign_event_lock_model_active_for_player(&state, 1) == GHC_CAMPAIGN_EVENT_LOCK_NO_EVENT);
    assert(ghc_campaign_event_lock_model_validate(&state) == GHC_CAMPAIGN_EVENT_LOCK_OK);
}

static void test_one_event_has_one_owner(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 0) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 1, 0) == GHC_CAMPAIGN_EVENT_LOCK_EVENT_BUSY);
    assert(ghc_campaign_event_lock_model_owner(&state, 0) == 0);
}

static void test_two_players_may_own_different_events_concurrently(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 0) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 1, 1) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_owner(&state, 0) == 0);
    assert(ghc_campaign_event_lock_model_owner(&state, 1) == 1);
    assert(ghc_campaign_event_lock_model_validate(&state) == GHC_CAMPAIGN_EVENT_LOCK_OK);
}

static void test_one_player_cannot_own_two_events_at_once(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 0) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 1) == GHC_CAMPAIGN_EVENT_LOCK_PLAYER_BUSY);
    assert(ghc_campaign_event_lock_model_owner(&state, 1) == GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER);
}

static void test_only_owner_can_release_or_complete(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 2) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_release(&state, 1u, 1u, 1, 2, 1u) == GHC_CAMPAIGN_EVENT_LOCK_NOT_OWNER);
    assert(ghc_campaign_event_lock_model_is_complete(&state, 2) == 0u);
    assert(ghc_campaign_event_lock_model_owner(&state, 2) == 0);
}

static void test_completed_event_requires_explicit_reset(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 3) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_release(&state, 1u, 1u, 0, 3, 1u) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 1, 3) == GHC_CAMPAIGN_EVENT_LOCK_ALREADY_COMPLETE);
    assert(ghc_campaign_event_lock_model_reset_event(&state, 3) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 1, 3) == GHC_CAMPAIGN_EVENT_LOCK_OK);
}

static void test_reset_cannot_clear_an_active_event(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 2) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_reset_event(&state, 2) == GHC_CAMPAIGN_EVENT_LOCK_RESET_BUSY);
    assert(ghc_campaign_event_lock_model_owner(&state, 2) == 0);
}

static void test_cancel_releases_without_completing(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 1, 1) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_release(&state, 1u, 1u, 1, 1, 0u) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_is_complete(&state, 1) == 0u);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 1) == GHC_CAMPAIGN_EVENT_LOCK_OK);
}

static void test_abort_releases_owner_without_completion(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 0) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_abort_player(&state, 0) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_owner(&state, 0) == GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER);
    assert(ghc_campaign_event_lock_model_is_complete(&state, 0) == 0u);
    assert(ghc_campaign_event_lock_model_abort_player(&state, 0) == GHC_CAMPAIGN_EVENT_LOCK_NO_ACTIVE_EVENT);
}

static void test_clear_active_releases_both_players_but_preserves_completion(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 0) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_release(&state, 1u, 1u, 0, 0, 1u) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 1) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 1, 2) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_clear_active(&state) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_owner(&state, 1) == GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER);
    assert(ghc_campaign_event_lock_model_owner(&state, 2) == GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER);
    assert(ghc_campaign_event_lock_model_is_complete(&state, 0) == 1u);
    assert(ghc_campaign_event_lock_model_validate(&state) == GHC_CAMPAIGN_EVENT_LOCK_OK);
}

static void test_stale_native_pair_fails_closed_and_clears_active_locks(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 0) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 1, 1) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 0u, 0, 2) == GHC_CAMPAIGN_EVENT_LOCK_SESSION_STALE);
    assert(ghc_campaign_event_lock_model_owner(&state, 0) == GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER);
    assert(ghc_campaign_event_lock_model_owner(&state, 1) == GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER);
    assert(ghc_campaign_event_lock_model_validate(&state) == GHC_CAMPAIGN_EVENT_LOCK_OK);
}

static void test_session_not_ready_does_not_mutate_existing_state(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 0) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_acquire(&state, 0u, 1u, 1, 1) == GHC_CAMPAIGN_EVENT_LOCK_SESSION_NOT_READY);
    assert(ghc_campaign_event_lock_model_owner(&state, 0) == 0);
    assert(ghc_campaign_event_lock_model_active_for_player(&state, 0) == 0);
}

static void test_acquire_does_not_initialize_before_session_is_ready(void)
{
    memset(&state, 0, sizeof(state));
    assert(ghc_campaign_event_lock_model_acquire(&state, 0u, 1u, 0, 0) ==
           GHC_CAMPAIGN_EVENT_LOCK_SESSION_NOT_READY);
    assert(state.initialized == 0u);
}

static void test_stale_pair_does_not_initialize_an_uninitialized_lock_table(void)
{
    memset(&state, 0, sizeof(state));
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 0u, 0, 0) ==
           GHC_CAMPAIGN_EVENT_LOCK_SESSION_STALE);
    assert(state.initialized == 0u);
}

static void test_release_with_stale_pair_clears_active_without_completion(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 1, 3) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    assert(ghc_campaign_event_lock_model_release(&state, 1u, 0u, 1, 3, 1u) == GHC_CAMPAIGN_EVENT_LOCK_SESSION_STALE);
    assert(ghc_campaign_event_lock_model_owner(&state, 3) == GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER);
    assert(ghc_campaign_event_lock_model_is_complete(&state, 3) == 0u);
}

static void test_invalid_arguments_fail_closed(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(NULL, 1u, 1u, 0, 0) == GHC_CAMPAIGN_EVENT_LOCK_INVALID_ARGUMENT);
    assert(ghc_campaign_event_lock_model_acquire(&state, 2u, 1u, 0, 0) == GHC_CAMPAIGN_EVENT_LOCK_INVALID_ARGUMENT);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 2u, 0, 0) == GHC_CAMPAIGN_EVENT_LOCK_INVALID_ARGUMENT);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, -1, 0) == GHC_CAMPAIGN_EVENT_LOCK_INVALID_PLAYER);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 2, 0) == GHC_CAMPAIGN_EVENT_LOCK_INVALID_PLAYER);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, -1) == GHC_CAMPAIGN_EVENT_LOCK_INVALID_EVENT);
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 16) == GHC_CAMPAIGN_EVENT_LOCK_INVALID_EVENT);
    assert(ghc_campaign_event_lock_model_release(&state, 1u, 1u, 0, 0, 2u) == GHC_CAMPAIGN_EVENT_LOCK_INVALID_ARGUMENT);
}

static void test_invariant_checker_rejects_owner_active_mismatch(void)
{
    reset_state();
    assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, 0) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    state.active_event_by_player[0] = GHC_CAMPAIGN_EVENT_LOCK_NO_EVENT;
    assert(ghc_campaign_event_lock_model_validate(&state) == GHC_CAMPAIGN_EVENT_LOCK_STATE_CORRUPT);
}

static void test_invariant_checker_rejects_invalid_completion_flag(void)
{
    reset_state();
    state.completed_by_event[4] = 2u;
    assert(ghc_campaign_event_lock_model_validate(&state) == GHC_CAMPAIGN_EVENT_LOCK_STATE_CORRUPT);
}

static void test_phase_90_event_ids_zero_through_three_are_supported(void)
{
    int event_id;
    reset_state();
    for (event_id = 0; event_id < 4; ++event_id) {
        assert(ghc_campaign_event_lock_model_acquire(&state, 1u, 1u, 0, event_id) == GHC_CAMPAIGN_EVENT_LOCK_OK);
        assert(ghc_campaign_event_lock_model_release(&state, 1u, 1u, 0, event_id, 0u) == GHC_CAMPAIGN_EVENT_LOCK_OK);
    }
}

int main(void)
{
    test_init_sets_all_sentinels();
    test_init_is_idempotent_and_preserves_completed_history();
    test_acquire_and_release_round_trip();
    test_one_event_has_one_owner();
    test_two_players_may_own_different_events_concurrently();
    test_one_player_cannot_own_two_events_at_once();
    test_only_owner_can_release_or_complete();
    test_completed_event_requires_explicit_reset();
    test_reset_cannot_clear_an_active_event();
    test_cancel_releases_without_completing();
    test_abort_releases_owner_without_completion();
    test_clear_active_releases_both_players_but_preserves_completion();
    test_stale_native_pair_fails_closed_and_clears_active_locks();
    test_session_not_ready_does_not_mutate_existing_state();
    test_acquire_does_not_initialize_before_session_is_ready();
    test_stale_pair_does_not_initialize_an_uninitialized_lock_table();
    test_release_with_stale_pair_clears_active_without_completion();
    test_invalid_arguments_fail_closed();
    test_invariant_checker_rejects_owner_active_mismatch();
    test_invariant_checker_rejects_invalid_completion_flag();
    test_phase_90_event_ids_zero_through_three_are_supported();
    puts("Campaign event-lock model tests: 21/21 PASS");
    return 0;
}
