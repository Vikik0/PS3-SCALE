#include "../client/ps3/mission_rules.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static GhcMission make_mission(void) {
    GhcMission mission;
    GhcMissionConfig config;
    config.bleedout_ms = 30000u;
    config.revive_ms = 5000u;
    config.revived_health_percent = 40u;
    ghc_mission_init(&mission, &config);
    return mission;
}

static void test_single_down_and_successful_revive(void) {
    GhcMission mission = make_mission();
    GhcMissionView view;

    assert(ghc_mission_player_downed(&mission, 2u, 1000u) == GHC_RULE_PLAYER_DOWNED);
    assert(mission.phase == GHC_MISSION_RUNNING);
    assert(mission.players[0].life_state == GHC_PLAYER_ALIVE);
    assert(mission.players[1].life_state == GHC_PLAYER_DOWNED);

    assert(ghc_mission_update_revive(&mission, 1u, 2u, 1u, 1u, 1u, 2000u) == GHC_RULE_NO_CHANGE);
    assert(ghc_mission_get_view(&mission, 4500u, &view) == 0);
    assert(view.revive_progress_percent[1] == 50u);

    // Releasing the revive input cancels the current attempt.
    assert(ghc_mission_update_revive(&mission, 1u, 2u, 0u, 1u, 1u, 4600u) == GHC_RULE_NO_CHANGE);
    assert(mission.players[1].revive_active == 0u);

    // A fresh, uninterrupted revive completes after five seconds.
    assert(ghc_mission_update_revive(&mission, 1u, 2u, 1u, 1u, 1u, 5000u) == GHC_RULE_NO_CHANGE);
    assert(ghc_mission_update_revive(&mission, 1u, 2u, 1u, 1u, 1u, 9999u) == GHC_RULE_NO_CHANGE);
    assert(ghc_mission_update_revive(&mission, 1u, 2u, 1u, 1u, 1u, 10000u) == GHC_RULE_PLAYER_REVIVED);

    assert(mission.players[1].life_state == GHC_PLAYER_ALIVE);
    assert(mission.players[1].health_percent == 40u);
    assert(mission.phase == GHC_MISSION_RUNNING);
}

static void test_revive_requires_range_and_line_of_sight(void) {
    GhcMission mission = make_mission();

    assert(ghc_mission_player_downed(&mission, 2u, 100u) == GHC_RULE_PLAYER_DOWNED);
    assert(ghc_mission_update_revive(&mission, 1u, 2u, 1u, 0u, 1u, 200u) == GHC_RULE_NO_CHANGE);
    assert(mission.players[1].revive_active == 0u);
    assert(ghc_mission_update_revive(&mission, 1u, 2u, 1u, 1u, 0u, 300u) == GHC_RULE_NO_CHANGE);
    assert(mission.players[1].revive_active == 0u);
    assert(ghc_mission_update_revive(&mission, 1u, 2u, 1u, 1u, 1u, 400u) == GHC_RULE_NO_CHANGE);
    assert(mission.players[1].revive_active == 1u);
}

static void test_bleedout_fails_mission(void) {
    GhcMission mission = make_mission();

    assert(ghc_mission_player_downed(&mission, 2u, 1000u) == GHC_RULE_PLAYER_DOWNED);
    assert(ghc_mission_tick(&mission, 30999u) == GHC_RULE_NO_CHANGE);
    assert(ghc_mission_tick(&mission, 31000u) == GHC_RULE_MISSION_FAILED);
    assert(mission.phase == GHC_MISSION_FAILED);
    assert(mission.failure_reason == GHC_MISSION_FAIL_BLEEDOUT);
}

static void test_bleedout_fails_even_if_revive_is_in_progress(void) {
    GhcMission mission = make_mission();

    assert(ghc_mission_player_downed(&mission, 2u, 1000u) == GHC_RULE_PLAYER_DOWNED);
    assert(ghc_mission_update_revive(&mission, 1u, 2u, 1u, 1u, 1u, 2000u) == GHC_RULE_NO_CHANGE);
    assert(ghc_mission_tick(&mission, 30999u) == GHC_RULE_NO_CHANGE);
    assert(ghc_mission_tick(&mission, 31000u) == GHC_RULE_MISSION_FAILED);
    assert(mission.failure_reason == GHC_MISSION_FAIL_BLEEDOUT);
}

static void test_both_players_downed_fails_immediately(void) {
    GhcMission mission = make_mission();

    assert(ghc_mission_player_downed(&mission, 1u, 1000u) == GHC_RULE_PLAYER_DOWNED);
    assert(ghc_mission_player_downed(&mission, 2u, 2000u) == GHC_RULE_MISSION_FAILED);
    assert(mission.phase == GHC_MISSION_FAILED);
    assert(mission.failure_reason == GHC_MISSION_FAIL_BOTH_DOWN);
    assert(ghc_mission_update_revive(&mission, 1u, 2u, 1u, 1u, 1u, 3000u) == GHC_RULE_NO_CHANGE);
}

static void test_revive_is_cancelled_when_reviver_is_down(void) {
    GhcMission mission = make_mission();

    assert(ghc_mission_player_downed(&mission, 2u, 100u) == GHC_RULE_PLAYER_DOWNED);
    assert(ghc_mission_update_revive(&mission, 1u, 2u, 1u, 1u, 1u, 200u) == GHC_RULE_NO_CHANGE);
    assert(ghc_mission_player_downed(&mission, 1u, 300u) == GHC_RULE_MISSION_FAILED);
    assert(mission.failure_reason == GHC_MISSION_FAIL_BOTH_DOWN);
}

static void test_self_revive_is_not_allowed(void) {
    GhcMission mission = make_mission();

    assert(ghc_mission_player_downed(&mission, 1u, 100u) == GHC_RULE_PLAYER_DOWNED);
    assert(ghc_mission_update_revive(&mission, 1u, 1u, 1u, 1u, 1u, 200u) == GHC_RULE_INVALID);
}

static void test_timer_wraparound(void) {
    GhcMission mission = make_mission();
    uint32_t downed_at = 0xfffffff0u;

    assert(ghc_mission_player_downed(&mission, 2u, downed_at) == GHC_RULE_PLAYER_DOWNED);
    assert(ghc_mission_tick(&mission, (uint32_t)(downed_at + 29999u)) == GHC_RULE_NO_CHANGE);
    assert(ghc_mission_tick(&mission, (uint32_t)(downed_at + 30000u)) == GHC_RULE_MISSION_FAILED);
}

static void test_snapshot_roundtrip(void) {
    GhcMission mission = make_mission();
    GhcMissionView before;
    GhcMissionView after;
    uint8_t packet[GHC_MISSION_SNAPSHOT_SIZE];
    size_t packet_length = 0u;

    assert(ghc_mission_player_downed(&mission, 2u, 1000u) == GHC_RULE_PLAYER_DOWNED);
    assert(ghc_mission_update_revive(&mission, 1u, 2u, 1u, 1u, 1u, 2000u) == GHC_RULE_NO_CHANGE);
    assert(ghc_mission_get_view(&mission, 4000u, &before) == 0);
    assert(ghc_mission_encode_snapshot(&before, packet, sizeof(packet), &packet_length) == 0);
    assert(packet_length == GHC_MISSION_SNAPSHOT_SIZE);
    assert(memcmp(packet, "GMS", 3u) == 0);
    assert(packet[3] == GHC_MISSION_SNAPSHOT_VERSION);
    assert(ghc_mission_decode_snapshot(packet, packet_length, &after) == 0);

    assert(after.revision == before.revision);
    assert(after.phase == before.phase);
    assert(after.failure_reason == before.failure_reason);
    assert(after.player_state[1] == GHC_PLAYER_DOWNED);
    assert(after.revive_progress_percent[1] == 40u);
    assert(after.bleedout_remaining_ms[1] == 27000u);

    assert(ghc_mission_revision_is_newer(2u, 1u) == 1);
    assert(ghc_mission_revision_is_newer(1u, 2u) == 0);
    assert(ghc_mission_revision_is_newer(0u, 0xffffffffu) == 1);
    assert(ghc_mission_revision_is_newer(7u, 7u) == 0);

    packet[0] = 99u;
    assert(ghc_mission_decode_snapshot(packet, packet_length, &after) != 0);

    // A malformed snapshot must not partially overwrite the caller's view.
    packet[0] = (uint8_t)'G';
    packet[4] = GHC_MISSION_RUNNING;
    packet[5] = GHC_MISSION_FAIL_BLEEDOUT;
    {
        GhcMissionView preserved = after;
        assert(ghc_mission_decode_snapshot(packet, packet_length, &after) != 0);
        assert(memcmp(&after, &preserved, sizeof(after)) == 0);
    }

    // A downed player cannot have positive health in a wire snapshot.
    packet[5] = GHC_MISSION_FAIL_NONE;
    packet[14] = 1u;
    {
        GhcMissionView preserved = after;
        assert(ghc_mission_decode_snapshot(packet, packet_length, &after) != 0);
        assert(memcmp(&after, &preserved, sizeof(after)) == 0);
    }

    // Encoding rejects inconsistent mission phase/failure combinations.
    before.phase = GHC_MISSION_FAILED;
    before.failure_reason = GHC_MISSION_FAIL_NONE;
    packet_length = 123u;
    assert(ghc_mission_encode_snapshot(&before, packet, sizeof(packet), &packet_length) != 0);
    assert(packet_length == 0u);
}

static void test_reset_restores_both_players(void) {
    GhcMission mission = make_mission();

    uint32_t failed_revision;

    assert(ghc_mission_player_downed(&mission, 1u, 100u) == GHC_RULE_PLAYER_DOWNED);
    assert(ghc_mission_player_downed(&mission, 2u, 200u) == GHC_RULE_MISSION_FAILED);
    failed_revision = mission.revision;
    ghc_mission_reset(&mission);

    assert(mission.revision > failed_revision);
    assert(mission.phase == GHC_MISSION_RUNNING);
    assert(mission.failure_reason == GHC_MISSION_FAIL_NONE);
    assert(mission.players[0].life_state == GHC_PLAYER_ALIVE);
    assert(mission.players[1].life_state == GHC_PLAYER_ALIVE);
    assert(mission.players[0].health_percent == 100u);
    assert(mission.players[1].health_percent == 100u);
}

int main(void) {
    test_single_down_and_successful_revive();
    test_revive_requires_range_and_line_of_sight();
    test_bleedout_fails_mission();
    test_bleedout_fails_even_if_revive_is_in_progress();
    test_both_players_downed_fails_immediately();
    test_revive_is_cancelled_when_reviver_is_down();
    test_self_revive_is_not_allowed();
    test_timer_wraparound();
    test_snapshot_roundtrip();
    test_reset_restores_both_players();
    puts("Ghosts co-op down/revive mission rules: PASS");
    return 0;
}
