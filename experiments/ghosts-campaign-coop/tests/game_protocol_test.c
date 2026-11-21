#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../client/ps3/ghc_game_protocol.h"

static void test_player_state_roundtrip(void) {
    GhcGameMessage input;
    GhcGameMessage output;
    uint8_t packet[GHC_GAME_MAX_PACKET_SIZE];
    size_t length = 0;

    memset(&input, 0, sizeof(input));
    input.type = GHC_GAME_PLAYER_STATE;
    input.data.player_state.slot = 2;
    input.data.player_state.actor_id = 0x1234;
    input.data.player_state.game_tick = 0x10203040;
    input.data.player_state.position_cm[0] = -1250;
    input.data.player_state.position_cm[1] = 32000;
    input.data.player_state.position_cm[2] = 785;
    input.data.player_state.velocity_cms[0] = -120;
    input.data.player_state.velocity_cms[1] = 250;
    input.data.player_state.velocity_cms[2] = 0;
    input.data.player_state.yaw_centidegrees = 35999;
    input.data.player_state.pitch_centidegrees = -4500;
    input.data.player_state.movement_flags = GHC_MOVE_FORWARD | GHC_MOVE_SPRINT | GHC_MOVE_AIM;
    input.data.player_state.stance = GHC_STANCE_CROUCHED;
    input.data.player_state.weapon_id = 17;
    input.data.player_state.ammo_clip = 30;
    input.data.player_state.ammo_reserve = 120;
    input.data.player_state.health = 85;
    input.data.player_state.armor = 25;

    assert(ghc_game_encode_message(&input, packet, sizeof(packet), &length) == 0);
    assert(length == GHC_GAME_HEADER_SIZE + 40);
    assert(memcmp(packet, "GCP1", 4) == 0);
    assert(packet[4] == GHC_GAME_VERSION && packet[5] == GHC_GAME_PLAYER_STATE);
    assert(ghc_game_decode_message(packet, length, &output) == 0);
    assert(output.type == input.type);
    assert(output.data.player_state.slot == 2);
    assert(output.data.player_state.actor_id == 0x1234);
    assert(output.data.player_state.position_cm[0] == -1250);
    assert(output.data.player_state.position_cm[1] == 32000);
    assert(output.data.player_state.velocity_cms[0] == -120);
    assert(output.data.player_state.yaw_centidegrees == 35999);
    assert(output.data.player_state.pitch_centidegrees == -4500);
    assert(output.data.player_state.movement_flags == input.data.player_state.movement_flags);
    assert(output.data.player_state.health == 85 && output.data.player_state.armor == 25);
}

static void test_revive_event_roundtrip(void) {
    GhcGameMessage input;
    GhcGameMessage output;
    uint8_t packet[GHC_GAME_MAX_PACKET_SIZE];
    size_t length = 0;

    memset(&input, 0, sizeof(input));
    input.type = GHC_GAME_REVIVE_EVENT;
    input.data.revive_event.event_id = 9001;
    input.data.revive_event.reviver_slot = 1;
    input.data.revive_event.downed_slot = 2;
    input.data.revive_event.phase = GHC_REVIVE_PROGRESS;
    input.data.revive_event.progress_ms = 2400;
    input.data.revive_event.remaining_ms = 2600;
    input.data.revive_event.result = 0;
    input.data.revive_event.game_tick = 123456;

    assert(ghc_game_encode_message(&input, packet, sizeof(packet), &length) == 0);
    assert(length == GHC_GAME_HEADER_SIZE + 16);
    assert(ghc_game_decode_message(packet, length, &output) == 0);
    assert(output.data.revive_event.event_id == 9001);
    assert(output.data.revive_event.reviver_slot == 1);
    assert(output.data.revive_event.downed_slot == 2);
    assert(output.data.revive_event.phase == GHC_REVIVE_PROGRESS);
    assert(output.data.revive_event.progress_ms == 2400);
    assert(output.data.revive_event.remaining_ms == 2600);
}

static void test_action_damage_mission_and_heartbeat(void) {
    GhcGameMessage input;
    GhcGameMessage output;
    uint8_t packet[GHC_GAME_MAX_PACKET_SIZE];
    size_t length = 0;

    memset(&input, 0, sizeof(input));
    input.type = GHC_GAME_PLAYER_ACTION;
    input.data.player_action.slot = 1;
    input.data.player_action.action = GHC_ACTION_RELOAD;
    input.data.player_action.actor_id = 10;
    input.data.player_action.weapon_id = 22;
    input.data.player_action.game_tick = 200;
    assert(ghc_game_encode_message(&input, packet, sizeof(packet), &length) == 0);
    assert(ghc_game_decode_message(packet, length, &output) == 0);
    assert(output.data.player_action.action == GHC_ACTION_RELOAD);

    memset(&input, 0, sizeof(input));
    input.type = GHC_GAME_DAMAGE_EVENT;
    input.data.damage_event.event_id = 44;
    input.data.damage_event.attacker_slot = 0;
    input.data.damage_event.victim_slot = 2;
    input.data.damage_event.damage = 35;
    input.data.damage_event.resulting_health = 65;
    input.data.damage_event.game_tick = 201;
    assert(ghc_game_encode_message(&input, packet, sizeof(packet), &length) == 0);
    assert(ghc_game_decode_message(packet, length, &output) == 0);
    assert(output.data.damage_event.attacker_slot == 0);
    assert(output.data.damage_event.victim_slot == 2);
    assert(output.data.damage_event.damage == 35);

    memset(&input, 0, sizeof(input));
    input.type = GHC_GAME_MISSION_EVENT;
    input.data.mission_event.event_id = 55;
    input.data.mission_event.event_type = GHC_MISSION_CHECKPOINT_REACHED;
    input.data.mission_event.checkpoint_id = 3;
    input.data.mission_event.objective_id = 7;
    input.data.mission_event.game_tick = 202;
    assert(ghc_game_encode_message(&input, packet, sizeof(packet), &length) == 0);
    assert(ghc_game_decode_message(packet, length, &output) == 0);
    assert(output.data.mission_event.checkpoint_id == 3);
    assert(output.data.mission_event.objective_id == 7);

    memset(&input, 0, sizeof(input));
    input.type = GHC_GAME_HEARTBEAT;
    input.data.heartbeat.game_tick = 300;
    input.data.heartbeat.ping_ms = 42;
    input.data.heartbeat.slot = 2;
    input.data.heartbeat.status = 1;
    assert(ghc_game_encode_message(&input, packet, sizeof(packet), &length) == 0);
    assert(ghc_game_decode_message(packet, length, &output) == 0);
    assert(output.data.heartbeat.ping_ms == 42);
    assert(output.data.heartbeat.slot == 2);
}

static void test_reject_invalid_packet_and_fields(void) {
    GhcGameMessage input;
    GhcGameMessage output;
    uint8_t packet[GHC_GAME_MAX_PACKET_SIZE];
    size_t length = 0;

    memset(&input, 0, sizeof(input));
    input.type = GHC_GAME_PLAYER_STATE;
    input.data.player_state.slot = 3;
    assert(ghc_game_encode_message(&input, packet, sizeof(packet), &length) == -1);

    input.data.player_state.slot = 1;
    input.data.player_state.health = 101;
    assert(ghc_game_encode_message(&input, packet, sizeof(packet), &length) == -1);

    memset(&input, 0, sizeof(input));
    input.type = GHC_GAME_REVIVE_EVENT;
    input.data.revive_event.reviver_slot = 1;
    input.data.revive_event.downed_slot = 1;
    input.data.revive_event.phase = GHC_REVIVE_COMPLETE;
    assert(ghc_game_encode_message(&input, packet, sizeof(packet), &length) == -1);

    memset(&input, 0, sizeof(input));
    input.type = GHC_GAME_HEARTBEAT;
    input.data.heartbeat.slot = 1;
    assert(ghc_game_encode_message(&input, packet, sizeof(packet), &length) == 0);
    packet[4] = 99;
    assert(ghc_game_decode_message(packet, length, &output) == -1);

    packet[4] = GHC_GAME_VERSION;
    packet[0] = 'X';
    assert(ghc_game_decode_message(packet, length, &output) == -1);
}

int main(void) {
    test_player_state_roundtrip();
    test_revive_event_roundtrip();
    test_action_damage_mission_and_heartbeat();
    test_reject_invalid_packet_and_fields();
    puts("Ghosts co-op game payload protocol tests: PASS");
    return 0;
}
