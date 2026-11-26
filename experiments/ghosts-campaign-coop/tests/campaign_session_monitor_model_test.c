#include "campaign_session_monitor_model.h"

#include <assert.h>
#include <stdio.h>

static GhcSessionMonitorObservation ready_pair(void)
{
    GhcSessionMonitorObservation o = {
        1u, 1u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u
    };
    return o;
}

static void test_start_waits_for_native_pair(void)
{
    GhcSessionMonitorModel m;
    GhcSessionMonitorObservation o = {0};
    ghc_session_monitor_model_init(&m);
    assert(ghc_session_monitor_model_start(&m) == 1);
    assert(m.state == GHC_SESSION_MONITOR_WAITING_FOR_PAIR);
    assert(ghc_session_monitor_model_tick(&m, &o) == 0);
    assert(m.session_ready == 0u);
}

static void test_valid_native_pair_binds(void)
{
    GhcSessionMonitorModel m;
    GhcSessionMonitorObservation o = ready_pair();
    ghc_session_monitor_model_init(&m);
    ghc_session_monitor_model_start(&m);
    assert(ghc_session_monitor_model_tick(&m, &o) == 1);
    assert(m.session_ready == 1u);
    assert(m.rebind_count == 1u);
    assert(m.state == GHC_SESSION_MONITOR_READY);
}

static void test_requires_split_screen_and_player_array(void)
{
    GhcSessionMonitorModel m;
    GhcSessionMonitorObservation o = ready_pair();
    ghc_session_monitor_model_init(&m);
    ghc_session_monitor_model_start(&m);
    o.split_screen = 0u;
    assert(ghc_session_monitor_model_tick(&m, &o) == 0);
    o = ready_pair();
    o.player_array_present = 0u;
    assert(ghc_session_monitor_model_tick(&m, &o) == 0);
}

static void test_requires_exactly_two_players_and_one_of_each_role(void)
{
    GhcSessionMonitorObservation o = ready_pair();
    assert(ghc_session_monitor_pair_candidate_valid(&o));
    o.native_player_count = 1u;
    assert(!ghc_session_monitor_pair_candidate_valid(&o));
    o = ready_pair();
    o.native_player_count = 3u;
    assert(!ghc_session_monitor_pair_candidate_valid(&o));
    o = ready_pair();
    o.primary_count = 2u;
    assert(!ghc_session_monitor_pair_candidate_valid(&o));
    o = ready_pair();
    o.secondary_count = 2u;
    assert(!ghc_session_monitor_pair_candidate_valid(&o));
}

static void test_requires_two_valid_distinct_entities(void)
{
    GhcSessionMonitorObservation o = ready_pair();
    o.primary_entity_valid = 0u;
    assert(!ghc_session_monitor_pair_candidate_valid(&o));
    o = ready_pair();
    o.secondary_entity_valid = 0u;
    assert(!ghc_session_monitor_pair_candidate_valid(&o));
    o = ready_pair();
    o.entities_distinct = 0u;
    assert(!ghc_session_monitor_pair_candidate_valid(&o));
}

static void test_rejects_non_boolean_native_flags(void)
{
    GhcSessionMonitorObservation o = ready_pair();
    o.split_screen = 2u;
    assert(!ghc_session_monitor_pair_candidate_valid(&o));
    o = ready_pair();
    o.campaign_aliases_match = 2u;
    assert(!ghc_session_monitor_pair_current(&o));
}

static void test_current_pair_requires_both_bound_handles_and_aliases(void)
{
    GhcSessionMonitorObservation o = ready_pair();
    assert(ghc_session_monitor_pair_current(&o));
    o.bound_primary_present = 0u;
    assert(!ghc_session_monitor_pair_current(&o));
    o = ready_pair();
    o.bound_secondary_present = 0u;
    assert(!ghc_session_monitor_pair_current(&o));
    o = ready_pair();
    o.campaign_aliases_match = 0u;
    assert(!ghc_session_monitor_pair_current(&o));
}

static void test_seven_transient_failures_are_tolerated(void)
{
    GhcSessionMonitorModel m;
    GhcSessionMonitorObservation o = ready_pair();
    unsigned int i;
    ghc_session_monitor_model_init(&m);
    ghc_session_monitor_model_start(&m);
    ghc_session_monitor_model_tick(&m, &o);
    o.campaign_aliases_match = 0u;
    for (i = 0; i < 7u; ++i)
        assert(ghc_session_monitor_model_tick(&m, &o) == 0);
    assert(m.session_ready == 1u);
    assert(m.consecutive_failures == 7u);
    o = ready_pair();
    assert(ghc_session_monitor_model_tick(&m, &o) == 1);
    assert(m.consecutive_failures == 0u);
}

static void test_eighth_failure_invalidates_and_releases_active_locks(void)
{
    GhcSessionMonitorModel m;
    GhcSessionMonitorObservation o = ready_pair();
    unsigned int i;
    ghc_session_monitor_model_init(&m);
    ghc_session_monitor_model_start(&m);
    ghc_session_monitor_model_tick(&m, &o);
    m.active_lock_count = 3u;
    m.completed_event_count = 5u;
    o.bound_secondary_present = 0u;
    for (i = 0; i < 8u; ++i)
        ghc_session_monitor_model_tick(&m, &o);
    assert(m.session_ready == 0u);
    assert(m.active_lock_count == 0u);
    assert(m.invalidation_count == 1u);
    assert(m.completed_event_count == 5u);
    assert(m.state == GHC_SESSION_MONITOR_WAITING_FOR_PAIR);
}

static void test_pair_can_rebind_after_invalidation(void)
{
    GhcSessionMonitorModel m;
    GhcSessionMonitorObservation o = ready_pair();
    unsigned int i;
    ghc_session_monitor_model_init(&m);
    ghc_session_monitor_model_start(&m);
    ghc_session_monitor_model_tick(&m, &o);
    o.campaign_aliases_match = 0u;
    for (i = 0; i < 8u; ++i)
        ghc_session_monitor_model_tick(&m, &o);
    o = ready_pair();
    assert(ghc_session_monitor_model_tick(&m, &o) == 1);
    assert(m.session_ready == 1u);
    assert(m.rebind_count == 2u);
}

static void test_completed_history_survives_disconnect_and_rebind(void)
{
    GhcSessionMonitorModel m;
    GhcSessionMonitorObservation o = ready_pair();
    unsigned int i;
    ghc_session_monitor_model_init(&m);
    ghc_session_monitor_model_start(&m);
    ghc_session_monitor_model_tick(&m, &o);
    m.completed_event_count = 9u;
    o.campaign_aliases_match = 0u;
    for (i = 0; i < 8u; ++i)
        ghc_session_monitor_model_tick(&m, &o);
    o = ready_pair();
    ghc_session_monitor_model_tick(&m, &o);
    assert(m.completed_event_count == 9u);
}

static void test_start_is_idempotent_while_monitor_active(void)
{
    GhcSessionMonitorModel m;
    GhcSessionMonitorObservation o = ready_pair();
    ghc_session_monitor_model_init(&m);
    ghc_session_monitor_model_start(&m);
    ghc_session_monitor_model_tick(&m, &o);
    assert(ghc_session_monitor_model_start(&m) == 1);
    assert(m.session_ready == 1u);
    assert(m.rebind_count == 1u);
}

static void test_explicit_stop_transitions_monitor_to_stopped(void)
{
    GhcSessionMonitorModel m;
    GhcSessionMonitorObservation o = ready_pair();
    ghc_session_monitor_model_init(&m);
    ghc_session_monitor_model_start(&m);
    ghc_session_monitor_model_tick(&m, &o);
    assert(ghc_session_monitor_model_stop(&m) == 1);
    assert(ghc_session_monitor_model_tick(&m, &o) == 0);
    assert(m.active == 0u);
    assert(m.session_ready == 0u);
    assert(m.state == GHC_SESSION_MONITOR_STOPPED);
}

static void test_start_cancels_a_pending_stop_on_the_active_monitor(void)
{
    GhcSessionMonitorModel m;
    GhcSessionMonitorObservation o = ready_pair();
    ghc_session_monitor_model_init(&m);
    assert(ghc_session_monitor_model_start(&m) == 1);
    assert(ghc_session_monitor_model_tick(&m, &o) == 1);
    assert(ghc_session_monitor_model_stop(&m) == 1);
    assert(m.stop_requested == 1u);
    assert(ghc_session_monitor_model_start(&m) == 1);
    assert(m.active == 1u);
    assert(m.stop_requested == 0u);
    assert(m.session_ready == 1u);
    assert(m.rebind_count == 1u);
    assert(ghc_session_monitor_model_tick(&m, &o) == 1);
}

static void test_explicit_stop_releases_active_locks_and_preserves_history(void)
{
    GhcSessionMonitorModel m;
    GhcSessionMonitorObservation o = ready_pair();
    ghc_session_monitor_model_init(&m);
    ghc_session_monitor_model_start(&m);
    ghc_session_monitor_model_tick(&m, &o);
    m.active_lock_count = 2u;
    m.completed_event_count = 7u;
    assert(ghc_session_monitor_model_stop(&m) == 1);
    assert(ghc_session_monitor_model_tick(&m, &o) == 0);
    assert(m.active == 0u);
    assert(m.session_ready == 0u);
    assert(m.active_lock_count == 0u);
    assert(m.completed_event_count == 7u);
    assert(m.state == GHC_SESSION_MONITOR_STOPPED);
}

static void test_inactive_monitor_ignores_ticks(void)
{
    GhcSessionMonitorModel m;
    GhcSessionMonitorObservation o = ready_pair();
    ghc_session_monitor_model_init(&m);
    assert(ghc_session_monitor_model_tick(&m, &o) == 0);
    assert(m.rebind_count == 0u);
}

int main(void)
{
    test_start_waits_for_native_pair();
    test_valid_native_pair_binds();
    test_requires_split_screen_and_player_array();
    test_requires_exactly_two_players_and_one_of_each_role();
    test_requires_two_valid_distinct_entities();
    test_rejects_non_boolean_native_flags();
    test_current_pair_requires_both_bound_handles_and_aliases();
    test_seven_transient_failures_are_tolerated();
    test_eighth_failure_invalidates_and_releases_active_locks();
    test_pair_can_rebind_after_invalidation();
    test_completed_history_survives_disconnect_and_rebind();
    test_start_is_idempotent_while_monitor_active();
    test_explicit_stop_transitions_monitor_to_stopped();
    test_start_cancels_a_pending_stop_on_the_active_monitor();
    test_explicit_stop_releases_active_locks_and_preserves_history();
    test_inactive_monitor_ignores_ticks();
    puts("Campaign session-monitor model tests: 16/16 PASS");
    return 0;
}
