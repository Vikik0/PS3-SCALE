#ifndef GHC_CAMPAIGN_SESSION_MONITOR_MODEL_H
#define GHC_CAMPAIGN_SESSION_MONITOR_MODEL_H

#include <stdint.h>

typedef enum {
    GHC_SESSION_MONITOR_STOPPED = 0,
    GHC_SESSION_MONITOR_WAITING_FOR_PAIR = 1,
    GHC_SESSION_MONITOR_READY = 2
} GhcSessionMonitorState;

typedef struct {
    uint8_t active;
    uint8_t stop_requested;
    uint8_t session_ready;
    uint8_t consecutive_failures;
    uint32_t invalidation_count;
    uint32_t rebind_count;
    uint32_t active_lock_count;
    uint32_t completed_event_count;
    GhcSessionMonitorState state;
} GhcSessionMonitorModel;

typedef struct {
    uint8_t split_screen;
    uint8_t player_array_present;
    uint8_t native_player_count;
    uint8_t primary_count;
    uint8_t secondary_count;
    uint8_t primary_entity_valid;
    uint8_t secondary_entity_valid;
    uint8_t entities_distinct;
    uint8_t bound_primary_present;
    uint8_t bound_secondary_present;
    uint8_t campaign_aliases_match;
} GhcSessionMonitorObservation;

void ghc_session_monitor_model_init(GhcSessionMonitorModel *model);
int ghc_session_monitor_model_start(GhcSessionMonitorModel *model);
int ghc_session_monitor_model_stop(GhcSessionMonitorModel *model);
int ghc_session_monitor_pair_candidate_valid(const GhcSessionMonitorObservation *observation);
int ghc_session_monitor_pair_current(const GhcSessionMonitorObservation *observation);
int ghc_session_monitor_model_tick(
    GhcSessionMonitorModel *model,
    const GhcSessionMonitorObservation *observation
);

#endif
