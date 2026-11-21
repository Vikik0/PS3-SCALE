#pragma once
#include <stddef.h>
#include <stdint.h>

#define GHC_WS_MAX_PAYLOAD 1200
#define GHC_WS_MAX_CLIENT_FRAME (GHC_WS_MAX_PAYLOAD + 8)

#define GHC_WS_OPCODE_BINARY 0x2
#define GHC_WS_OPCODE_CLOSE  0x8
#define GHC_WS_OPCODE_PING   0x9
#define GHC_WS_OPCODE_PONG   0xA

/*
 * Encode one final, masked client-to-server binary WebSocket frame.
 * The caller supplies four cryptographically random mask bytes.
 * Returns 0 on success, -1 on invalid input or insufficient output capacity.
 */
int ghc_ws_encode_client_binary(
    const uint8_t *payload,
    size_t payload_length,
    const uint8_t mask[4],
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
);

/*
 * Decode one complete, unmasked server WebSocket frame.
 * Returns:
 *   0  one complete supported frame was decoded
 *   1  more input bytes are required
 *  -1  malformed, oversized, fragmented, masked or unsupported frame
 *
 * Supported server opcodes: binary, close, ping and pong.
 * Payload bytes are copied into the caller's output buffer.
 */
int ghc_ws_decode_server_frame(
    const uint8_t *input,
    size_t input_length,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_length,
    size_t *consumed,
    uint8_t *opcode
);
