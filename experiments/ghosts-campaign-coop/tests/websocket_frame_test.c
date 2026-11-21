#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../client/ps3/ghc_websocket.h"

static void test_client_binary_frame_masks_payload(void) {
    const uint8_t payload[] = { 'G', 'H', 'C', '2', 0x10, 0x20, 0x30 };
    const uint8_t mask[4] = { 0x37, 0xFA, 0x21, 0x3D };
    uint8_t frame[GHC_WS_MAX_CLIENT_FRAME];
    size_t frame_length = 0;
    size_t i;

    assert(ghc_ws_encode_client_binary(payload, sizeof(payload), mask, frame, sizeof(frame), &frame_length) == 0);
    assert(frame_length == sizeof(payload) + 6);
    assert(frame[0] == 0x82);
    assert(frame[1] == (uint8_t)(0x80u | sizeof(payload)));
    for (i = 0; i < 4; ++i) assert(frame[2 + i] == mask[i]);
    for (i = 0; i < sizeof(payload); ++i) assert(frame[6 + i] == (uint8_t)(payload[i] ^ mask[i & 3u]));
}

static void test_server_binary_frame_decode(void) {
    const uint8_t frame[] = { 0x82, 0x04, 'G', 'H', 'C', '2' };
    uint8_t payload[16];
    size_t payload_length = 0;
    size_t consumed = 0;
    uint8_t opcode = 0;

    assert(ghc_ws_decode_server_frame(frame, sizeof(frame), payload, sizeof(payload),
                                      &payload_length, &consumed, &opcode) == 0);
    assert(opcode == GHC_WS_OPCODE_BINARY);
    assert(payload_length == 4);
    assert(consumed == sizeof(frame));
    assert(payload[0] == 'G' && payload[1] == 'H' && payload[2] == 'C' && payload[3] == '2');
}

static void test_incomplete_frame_requests_more_bytes(void) {
    const uint8_t partial[] = { 0x82, 0x05, 'a', 'b' };
    uint8_t payload[16];
    size_t payload_length = 99;
    size_t consumed = 99;
    uint8_t opcode = 99;

    assert(ghc_ws_decode_server_frame(partial, sizeof(partial), payload, sizeof(payload),
                                      &payload_length, &consumed, &opcode) == 1);
    assert(payload_length == 0);
    assert(consumed == 0);
    assert(opcode == 0);
}

static void test_ping_control_frame(void) {
    const uint8_t frame[] = { 0x89, 0x01, 'p' };
    uint8_t payload[8];
    size_t payload_length = 0;
    size_t consumed = 0;
    uint8_t opcode = 0;

    assert(ghc_ws_decode_server_frame(frame, sizeof(frame), payload, sizeof(payload),
                                      &payload_length, &consumed, &opcode) == 0);
    assert(opcode == GHC_WS_OPCODE_PING);
    assert(payload_length == 1 && payload[0] == 'p');
    assert(consumed == sizeof(frame));
}

static void test_reject_masked_fragmented_and_reserved_frames(void) {
    const uint8_t masked[] = { 0x82, 0x81, 0, 0, 0, 0, 'x' };
    const uint8_t fragmented[] = { 0x02, 0x01, 'x' };
    const uint8_t reserved[] = { 0xC2, 0x01, 'x' };
    uint8_t payload[16];
    size_t payload_length, consumed;
    uint8_t opcode;

    assert(ghc_ws_decode_server_frame(masked, sizeof(masked), payload, sizeof(payload),
                                      &payload_length, &consumed, &opcode) == -1);
    assert(ghc_ws_decode_server_frame(fragmented, sizeof(fragmented), payload, sizeof(payload),
                                      &payload_length, &consumed, &opcode) == -1);
    assert(ghc_ws_decode_server_frame(reserved, sizeof(reserved), payload, sizeof(payload),
                                      &payload_length, &consumed, &opcode) == -1);
}

static void test_extended_length_and_bounds(void) {
    uint8_t source[126];
    uint8_t frame[GHC_WS_MAX_CLIENT_FRAME];
    uint8_t mask[4] = { 1, 2, 3, 4 };
    uint8_t decoded[126];
    size_t frame_length = 0;
    size_t decoded_length = 0;
    size_t consumed = 0;
    uint8_t opcode = 0;
    size_t i;

    for (i = 0; i < sizeof(source); ++i) source[i] = (uint8_t)i;
    assert(ghc_ws_encode_client_binary(source, sizeof(source), mask, frame, sizeof(frame), &frame_length) == 0);
    assert(frame[0] == 0x82 && frame[1] == 0xFE);
    assert(frame[2] == 0 && frame[3] == 126);
    assert(frame_length == sizeof(source) + 8);

    {
        uint8_t server_frame[sizeof(source) + 4];
        server_frame[0] = 0x82;
        server_frame[1] = 126;
        server_frame[2] = 0;
        server_frame[3] = 126;
        for (i = 0; i < sizeof(source); ++i) server_frame[4 + i] = source[i];
        assert(ghc_ws_decode_server_frame(server_frame, sizeof(server_frame), decoded, sizeof(decoded),
                                          &decoded_length, &consumed, &opcode) == 0);
        assert(opcode == GHC_WS_OPCODE_BINARY);
        assert(decoded_length == sizeof(source));
        assert(consumed == sizeof(server_frame));
        for (i = 0; i < sizeof(source); ++i) assert(decoded[i] == source[i]);
    }

    assert(ghc_ws_encode_client_binary(source, GHC_WS_MAX_PAYLOAD + 1u, mask,
                                      frame, sizeof(frame), &frame_length) == -1);
}

int main(void) {
    test_client_binary_frame_masks_payload();
    test_server_binary_frame_decode();
    test_incomplete_frame_requests_more_bytes();
    test_ping_control_frame();
    test_reject_masked_fragmented_and_reserved_frames();
    test_extended_length_and_bounds();
    puts("GHC2 WebSocket frame tests: PASS");
    return 0;
}
