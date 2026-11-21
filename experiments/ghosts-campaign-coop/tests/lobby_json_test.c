#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../client/ps3/ghc_lobby_json.h"

static void test_guest_session_response(void) {
    const char json[] =
        "{"
        "\"session_id\":\"0123456789abcdef0123456789abcdef\","
        "\"token\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
        "\"mode\":\"guest\","
        "\"expires_in_seconds\":86400,"
        "\"relay\":{\"protocol\":\"GHC2\","
        "\"relay_id\":\"00112233445566778899aabbccddeeff\","
        "\"relay_key\":\"00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff\"}"
        "}";
    GhcGuestSession session;

    assert(ghc_lobby_parse_guest_session(json, &session) == 0);
    assert(strcmp(session.token, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") == 0);
    assert(strcmp(session.session_id, "0123456789abcdef0123456789abcdef") == 0);
    assert(session.expires_in_seconds == 86400u);
    assert(session.relay_id[0] == 0x00 && session.relay_id[1] == 0x11);
    assert(session.relay_id[14] == 0xee && session.relay_id[15] == 0xff);
    assert(session.relay_key[0] == 0x00 && session.relay_key[31] == 0xff);
}

static void test_nested_room_code(void) {
    const char json[] = "{\"status\":\"waiting\",\"code\":\"A1B2C3\",\"players\":[]}";
    char code[7];
    assert(ghc_lobby_parse_room_code(json, code) == 0);
    assert(strcmp(code, "A1B2C3") == 0);
}

static void test_json_escaped_string(void) {
    const char json[] = "{\"message\":\"line one\\nline two\",\"other\":\"ok\"}";
    char value[32];
    assert(ghc_json_get_string(json, "message", value, sizeof(value)) == 0);
    assert(strcmp(value, "line one\nline two") == 0);
}

static void test_reject_bad_or_missing_fields(void) {
    const char bad_session[] =
        "{\"session_id\":\"bad\",\"token\":\"bad\",\"expires_in_seconds\":30,"
        "\"relay\":{\"relay_id\":\"zz\",\"relay_key\":\"00\"}}";
    const char bad_room[] = "{\"code\":\"abc123\"}";
    GhcGuestSession session;
    char code[7];

    assert(ghc_lobby_parse_guest_session(bad_session, &session) == -1);
    assert(session.token[0] == '\0');
    assert(ghc_lobby_parse_room_code(bad_room, code) == -1);
    assert(ghc_json_get_string("{\"x\":\"y\"}", "missing", code, sizeof(code)) == -1);
}

static void test_integer_bounds(void) {
    uint32_t value = 0;
    assert(ghc_json_get_u32("{\"ttl\":86400}", "ttl", &value) == 0);
    assert(value == 86400u);
    assert(ghc_json_get_u32("{\"ttl\":4294967296}", "ttl", &value) == -1);
    assert(ghc_json_get_u32("{\"ttl\":-1}", "ttl", &value) == -1);
}


static void test_peer_snapshot_with_candidate(void) {
    const char json[] =
        "{\"room_status\":\"running\",\"peer_ready\":true,"
        "\"peer\":{\"session_id\":\"0123456789abcdef0123456789abcdef\","
        "\"slot\":2,\"relay_id\":\"00112233445566778899aabbccddeeff\","
        "\"relay_key\":\"00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff\"},"
        "\"peer_candidate\":{\"public_address\":\"8.8.8.8\",\"public_port\":3478,"
        "\"local_address\":\"192.168.1.20\",\"local_port\":50000,\"nat_type\":\"moderate\"}}";
    GhcPeerSnapshot snapshot;

    assert(ghc_lobby_parse_peer_snapshot(json, &snapshot) == 0);
    assert(snapshot.peer_present == 1);
    assert(snapshot.peer_ready == 1);
    assert(snapshot.room_running == 1);
    assert(snapshot.peer_slot == 2);
    assert(snapshot.candidate_present == 1);
    assert(strcmp(snapshot.candidate.public_address, "8.8.8.8") == 0);
    assert(snapshot.candidate.public_port == 3478);
    assert(strcmp(snapshot.candidate.local_address, "192.168.1.20") == 0);
    assert(snapshot.candidate.local_port == 50000);
    assert(strcmp(snapshot.candidate.nat_type, "moderate") == 0);
}

static void test_peer_waiting_without_second_player(void) {
    const char json[] = "{\"room_status\":\"waiting\",\"peer_ready\":false,\"peer\":null,\"peer_candidate\":null}";
    GhcPeerSnapshot snapshot;
    assert(ghc_lobby_parse_peer_snapshot(json, &snapshot) == 0);
    assert(snapshot.peer_present == 0);
    assert(snapshot.peer_ready == 0);
    assert(snapshot.room_running == 0);
    assert(snapshot.candidate_present == 0);
}

static void test_reject_invalid_peer_candidate(void) {
    const char json[] =
        "{\"room_status\":\"running\",\"peer\":{\"session_id\":\"0123456789abcdef0123456789abcdef\","
        "\"slot\":2,\"relay_id\":\"00112233445566778899aabbccddeeff\","
        "\"relay_key\":\"00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff\"},"
        "\"peer_candidate\":{\"public_address\":\"999.1.1.1\",\"public_port\":70000,"
        "\"local_address\":\"192.168.1.20\",\"local_port\":50000,\"nat_type\":\"bad\"}}";
    GhcPeerSnapshot snapshot;
    assert(ghc_lobby_parse_peer_snapshot(json, &snapshot) == -1);
}

int main(void) {
    test_guest_session_response();
    test_nested_room_code();
    test_json_escaped_string();
    test_reject_bad_or_missing_fields();
    test_integer_bounds();
    test_peer_snapshot_with_candidate();
    test_peer_waiting_without_second_player();
    test_reject_invalid_peer_candidate();
    puts("Ghosts co-op lobby JSON tests: PASS");
    return 0;
}
