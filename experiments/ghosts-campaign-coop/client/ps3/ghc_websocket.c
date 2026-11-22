#include "ghc_websocket.h"

static void ghc_ws_copy(uint8_t *destination, const uint8_t *source, size_t length) {
    size_t i;
    for (i = 0; i < length; ++i) destination[i] = source[i];
}

int ghc_ws_encode_client_binary(
    const uint8_t *payload,
    size_t payload_length,
    const uint8_t mask[4],
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
) {
    size_t header_length;
    size_t i;
    size_t cursor;

    if (!mask || !output || !output_length) return -1;
    *output_length = 0;
    if (payload_length > GHC_WS_MAX_PAYLOAD) return -1;
    if (payload_length > 0 && !payload) return -1;

    header_length = payload_length <= 125 ? 6 : 8;
    if (output_capacity < header_length + payload_length) return -1;

    output[0] = 0x82; /* FIN + binary opcode */
    if (payload_length <= 125) {
        output[1] = (uint8_t)(0x80u | (uint8_t)payload_length);
        cursor = 2;
    } else {
        output[1] = 0xFE; /* masked + 16-bit extended length */
        output[2] = (uint8_t)((payload_length >> 8) & 0xFFu);
        output[3] = (uint8_t)(payload_length & 0xFFu);
        cursor = 4;
    }

    ghc_ws_copy(output + cursor, mask, 4);
    cursor += 4;
    for (i = 0; i < payload_length; ++i) {
        output[cursor + i] = (uint8_t)(payload[i] ^ mask[i & 3u]);
    }

    *output_length = cursor + payload_length;
    return 0;
}

int ghc_ws_decode_server_frame(
    const uint8_t *input,
    size_t input_length,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_length,
    size_t *consumed,
    uint8_t *opcode
) {
    uint8_t first;
    uint8_t second;
    uint8_t fin;
    uint8_t mask;
    uint8_t op;
    uint64_t frame_length;
    size_t header_length;
    size_t i;

    if (!input || !payload_length || !consumed || !opcode) return -1;
    *payload_length = 0;
    *consumed = 0;
    *opcode = 0;
    if (input_length < 2) return 1;

    first = input[0];
    second = input[1];
    fin = (uint8_t)(first >> 7);
    op = (uint8_t)(first & 0x0Fu);
    mask = (uint8_t)(second >> 7);

    /* RSV bits must be zero; server-to-client frames must not be masked. */
    if ((first & 0x70u) != 0 || mask != 0 || fin != 1) return -1;
    if (op != GHC_WS_OPCODE_BINARY &&
        op != GHC_WS_OPCODE_CLOSE &&
        op != GHC_WS_OPCODE_PING &&
        op != GHC_WS_OPCODE_PONG) return -1;

    frame_length = (uint64_t)(second & 0x7Fu);
    header_length = 2;
    if (frame_length == 126) {
        if (input_length < 4) return 1;
        frame_length = ((uint64_t)input[2] << 8) | input[3];
        if (frame_length < 126) return -1; /* non-canonical length */
        header_length = 4;
    } else if (frame_length == 127) {
        if (input_length < 10) return 1;
        if ((input[2] & 0x80u) != 0) return -1;
        frame_length = 0;
        for (i = 0; i < 8; ++i) frame_length = (frame_length << 8) | input[2 + i];
        if (frame_length < 65536u) return -1; /* non-canonical length */
        header_length = 10;
    }

    if (op != GHC_WS_OPCODE_BINARY && frame_length > 125u) return -1;
    if (frame_length > GHC_WS_MAX_PAYLOAD) return -1;
    if (frame_length > (uint64_t)payload_capacity) return -1;
    if (frame_length > 0 && !payload) return -1;
    if (input_length < header_length + (size_t)frame_length) return 1;

    for (i = 0; i < (size_t)frame_length; ++i) payload[i] = input[header_length + i];

    *payload_length = (size_t)frame_length;
    *consumed = header_length + (size_t)frame_length;
    *opcode = op;
    return 0;
}
