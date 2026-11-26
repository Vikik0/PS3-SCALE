#include "campaign_session_monitor_model.h"

static int valid_bool(uint8_t value)
{
    return value == 0u || value == 1u;
}

void ghc_session_monitor_model_init(GhcSessionMonitorModel *model)
{
    if (!model)
        return;
    model->active = 0u;
    model->stop_requested = 0u;
    model->session_ready = 0u;
    model->consecutive_failures = 0u;
    model->invalidation_count = 0u;
    model->rebind_count = 0u;
    model->active_lock_count = 0u;
    model->completed_event_count = 0u;
    model->state = GHC_SESSION_MONITOR_STOPPED;
}

int ghc_session_monitor_model_start(GhcSessionMonitorModel *model)
{
    if (!model)
        return 0;
    if (model->active && !model->stop_requested)
        return 1;
    model->active = 1u;
    model->stop_requested = 0u;
    model->state = model->session_ready
        ? GHC_SESSION_MONITOR_READY
        : GHC_SESSION_MONITOR_WAITING_FOR_PAIR;
    return 1;
}

int ghc_session_monitor_model_stop(GhcSessionMonitorModel *model)
{
    if (!model || !model->active)
        return 0;
    model->stop_requested = 1u;
    return 1;
}

int ghc_session_monitor_pair_candidate_valid(const GhcSessionMonitorObservation *o)
{
    if (!o)
        return 0;
    if (!valid_bool(o->split_screen)
        || !valid_bool(o->player_array_present)
        || !valid_bool(o->primary_entity_valid)
        || !valid_bool(o->secondary_entity_valid)
        || !valid_bool(o->entities_distinct))
        return 0;
    return o->split_screen == 1u
        && o->player_array_present == 1u
        && o->native_player_count == 2u
        && o->primary_count == 1u
        && o->secondary_count == 1u
        && o->primary_entity_valid == 1u
        && o->secondary_entity_valid == 1u
        && o->entities_distinct == 1u;
}

int ghc_session_monitor_pair_current(const GhcSessionMonitorObservation *o)
{
    if (!ghc_session_monitor_pair_candidate_valid(o))
        return 0;
    if (!valid_bool(o->bound_primary_present)
        || !valid_bool(o->bound_secondary_present)
        || !valid_bool(o->campaign_aliases_match))
        return 0;
    return o->bound_primary_present == 1u
        && o->bound_secondary_present == 1u
        && o->campaign_aliases_match == 1u;
}

int ghc_session_monitor_model_tick(
    GhcSessionMonitorModel *model,
    const GhcSessionMonitorObservation *observation
)
{
    if (!model || !observation || !model->active)
        return 0;

    if (model->stop_requested) {
        model->active = 0u;
        model->session_ready = 0u;
        model->consecutive_failures = 0u;
        model->active_lock_count = 0u;
        model->state = GHC_SESSION_MONITOR_STOPPED;
        return 0;
    }

    if (model->session_ready) {
        if (ghc_session_monitor_pair_current(observation)) {
            model->consecutive_failures = 0u;
            model->state = GHC_SESSION_MONITOR_READY;
            return 1;
        }

        if (model->consecutive_failures < 8u)
            model->consecutive_failures++;

        if (model->consecutive_failures >= 8u) {
            model->session_ready = 0u;
            model->consecutive_failures = 0u;
            model->active_lock_count = 0u;
            model->invalidation_count++;
            model->state = GHC_SESSION_MONITOR_WAITING_FOR_PAIR;
        }
        return 0;
    }

    if (ghc_session_monitor_pair_candidate_valid(observation)) {
        model->session_ready = 1u;
        model->consecutive_failures = 0u;
        model->rebind_count++;
        model->state = GHC_SESSION_MONITOR_READY;
        return 1;
    }

    model->consecutive_failures = 0u;
    model->state = GHC_SESSION_MONITOR_WAITING_FOR_PAIR;
    return 0;
}
