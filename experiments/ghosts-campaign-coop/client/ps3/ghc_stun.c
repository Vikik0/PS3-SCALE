#include "ghc_stun.h"

static const uint8_t GHC_STUN_COOKIE[4] = { 0x21, 0x12, 0xA4, 0x42 };

static uint16_t gh_read_u16(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint32_t gh_read_u32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24)
        | ((uint32_t)bytes[1] << 16)
        | ((uint32_t)bytes[2] << 8)
        | (uint32_t)bytes[3];
}

static void gh_write_u16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static int gh_bytes_equal(const uint8_t *left, const uint8_t *right, size_t length) {
    size_t i;
    uint8_t difference = 0;
    for (i = 0; i < length; ++i) difference |= (uint8_t)(left[i] ^ right[i]);
    return difference == 0;
}

int ghc_stun_build_binding_request(
    const uint8_t transaction_id[GHC_STUN_TRANSACTION_ID_SIZE],
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
) {
    if (!transaction_id || !output || !output_length) return -1;
    *output_length = 0;
    if (output_capacity < GHC_STUN_HEADER_SIZE) return -1;

    gh_write_u16(output, GHC_STUN_BINDING_REQUEST);
    gh_write_u16(output + 2, 0);
    output[4] = GHC_STUN_COOKIE[0];
    output[5] = GHC_STUN_COOKIE[1];
    output[6] = GHC_STUN_COOKIE[2];
    output[7] = GHC_STUN_COOKIE[3];
    {
        size_t i;
        for (i = 0; i < GHC_STUN_TRANSACTION_ID_SIZE; ++i) {
            output[8 + i] = transaction_id[i];
        }
    }
    *output_length = GHC_STUN_HEADER_SIZE;
    return 0;
}

int ghc_stun_parse_binding_response(
    const uint8_t *packet,
    size_t packet_length,
    const uint8_t expected_transaction_id[GHC_STUN_TRANSACTION_ID_SIZE],
    GhcStunMappedAddress *mapped
) {
    uint16_t message_type;
    uint16_t message_length;
    size_t cursor;
    size_t message_end;
    int have_xor_mapped = 0;
    int have_mapped = 0;
    GhcStunMappedAddress xor_result;
    GhcStunMappedAddress mapped_result;

    if (!packet || !expected_transaction_id || !mapped) return -1;
    if (packet_length < GHC_STUN_HEADER_SIZE) return -1;

    message_type = gh_read_u16(packet);
    message_length = gh_read_u16(packet + 2);
    if (message_type != GHC_STUN_BINDING_SUCCESS && message_type != GHC_STUN_BINDING_ERROR) return -1;
    if ((message_length & 3u) != 0u) return -1;
    if ((size_t)message_length + GHC_STUN_HEADER_SIZE != packet_length) return -1;
    if (!gh_bytes_equal(packet + 4, GHC_STUN_COOKIE, sizeof(GHC_STUN_COOKIE))) return -1;
    if (!gh_bytes_equal(packet + 8, expected_transaction_id, GHC_STUN_TRANSACTION_ID_SIZE)) return -1;
    if (message_type == GHC_STUN_BINDING_ERROR) return -2;

    cursor = GHC_STUN_HEADER_SIZE;
    message_end = GHC_STUN_HEADER_SIZE + (size_t)message_length;
    while (cursor < message_end) {
        uint16_t attribute_type;
        uint16_t attribute_length;
        size_t padded_length;
        const uint8_t *value;

        if (message_end - cursor < 4u) return -1;
        attribute_type = gh_read_u16(packet + cursor);
        attribute_length = gh_read_u16(packet + cursor + 2);
        padded_length = ((size_t)attribute_length + 3u) & ~3u;
        if (padded_length > message_end - cursor - 4u) return -1;
        value = packet + cursor + 4u;

        if (attribute_type == 0x0020u || attribute_type == 0x0001u) {
            GhcStunMappedAddress candidate;
            uint16_t port;
            uint32_t address;

            if (attribute_length != 8u || value[0] != 0u || value[1] != 0x01u) return -1;
            port = gh_read_u16(value + 2);
            address = gh_read_u32(value + 4);
            if (attribute_type == 0x0020u) {
                port ^= 0x2112u;
                address ^= 0x2112A442u;
                candidate.ipv4[0] = (uint8_t)(address >> 24);
                candidate.ipv4[1] = (uint8_t)(address >> 16);
                candidate.ipv4[2] = (uint8_t)(address >> 8);
                candidate.ipv4[3] = (uint8_t)address;
                candidate.port = port;
                xor_result = candidate;
                have_xor_mapped = 1;
            } else {
                candidate.ipv4[0] = (uint8_t)(address >> 24);
                candidate.ipv4[1] = (uint8_t)(address >> 16);
                candidate.ipv4[2] = (uint8_t)(address >> 8);
                candidate.ipv4[3] = (uint8_t)address;
                candidate.port = port;
                mapped_result = candidate;
                have_mapped = 1;
            }
        }
        cursor += 4u + padded_length;
    }

    if (have_xor_mapped) {
        *mapped = xor_result;
        return 0;
    }
    if (have_mapped) {
        *mapped = mapped_result;
        return 0;
    }
    return -1;
}

int ghc_stun_format_ipv4(const GhcStunMappedAddress *mapped, char output[16]) {
    size_t cursor = 0;
    size_t i;
    if (!mapped || !output) return -1;
    for (i = 0; i < 4; ++i) {
        uint8_t value = mapped->ipv4[i];
        char digits[3];
        size_t count = 0;
        do {
            digits[count++] = (char)('0' + (value % 10u));
            value = (uint8_t)(value / 10u);
        } while (value && count < sizeof(digits));
        while (count) output[cursor++] = digits[--count];
        if (i != 3) output[cursor++] = '.';
    }
    output[cursor] = '\0';
    return 0;
}
