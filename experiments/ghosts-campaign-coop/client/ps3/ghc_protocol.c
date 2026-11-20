#include "ghc_protocol.h"

#include <string.h>

typedef struct {
    uint32_t state[8];
    uint64_t bit_length;
    uint8_t block[64];
    size_t block_length;
} GhcSha256;

static const uint32_t GH_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t gh_rotr(uint32_t value, uint32_t count) {
    return (value >> count) | (value << (32u - count));
}

static void gh_sha256_transform(GhcSha256 *ctx, const uint8_t block[64]) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    size_t i;

    for (i = 0; i < 16; ++i) {
        size_t j = i * 4;
        w[i] = ((uint32_t)block[j] << 24)
             | ((uint32_t)block[j + 1] << 16)
             | ((uint32_t)block[j + 2] << 8)
             | (uint32_t)block[j + 3];
    }
    for (i = 16; i < 64; ++i) {
        uint32_t s0 = gh_rotr(w[i - 15], 7) ^ gh_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = gh_rotr(w[i - 2], 17) ^ gh_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; ++i) {
        uint32_t s1 = gh_rotr(e, 6) ^ gh_rotr(e, 11) ^ gh_rotr(e, 25);
        uint32_t choice = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + s1 + choice + GH_K[i] + w[i];
        uint32_t s0 = gh_rotr(a, 2) ^ gh_rotr(a, 13) ^ gh_rotr(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = s0 + majority;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void gh_sha256_init(GhcSha256 *ctx) {
    static const uint32_t initial[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };
    memcpy(ctx->state, initial, sizeof(initial));
    ctx->bit_length = 0;
    ctx->block_length = 0;
}

static void gh_sha256_update(GhcSha256 *ctx, const uint8_t *data, size_t length) {
    size_t i;
    for (i = 0; i < length; ++i) {
        ctx->block[ctx->block_length++] = data[i];
        if (ctx->block_length == 64) {
            gh_sha256_transform(ctx, ctx->block);
            ctx->bit_length += 512;
            ctx->block_length = 0;
        }
    }
}

static void gh_sha256_final(GhcSha256 *ctx, uint8_t output[32]) {
    size_t i;
    ctx->bit_length += (uint64_t)ctx->block_length * 8u;
    ctx->block[ctx->block_length++] = 0x80u;

    if (ctx->block_length > 56) {
        while (ctx->block_length < 64) {
            ctx->block[ctx->block_length++] = 0;
        }
        gh_sha256_transform(ctx, ctx->block);
        ctx->block_length = 0;
    }
    while (ctx->block_length < 56) {
        ctx->block[ctx->block_length++] = 0;
    }
    for (i = 0; i < 8; ++i) {
        ctx->block[63 - i] = (uint8_t)(ctx->bit_length >> (i * 8));
    }
    gh_sha256_transform(ctx, ctx->block);

    for (i = 0; i < 8; ++i) {
        output[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        output[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        output[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        output[i * 4 + 3] = (uint8_t)ctx->state[i];
    }
}

static void gh_hmac_sha256(
    const uint8_t *key,
    size_t key_length,
    const uint8_t *data,
    size_t data_length,
    uint8_t output[32]
) {
    uint8_t normalized_key[64] = {0};
    uint8_t inner_pad[64];
    uint8_t outer_pad[64];
    uint8_t inner_hash[32];
    GhcSha256 ctx;
    size_t i;

    if (key_length > 64) {
        gh_sha256_init(&ctx);
        gh_sha256_update(&ctx, key, key_length);
        gh_sha256_final(&ctx, normalized_key);
    } else if (key_length > 0) {
        memcpy(normalized_key, key, key_length);
    }

    for (i = 0; i < 64; ++i) {
        inner_pad[i] = normalized_key[i] ^ 0x36u;
        outer_pad[i] = normalized_key[i] ^ 0x5cu;
    }

    gh_sha256_init(&ctx);
    gh_sha256_update(&ctx, inner_pad, sizeof(inner_pad));
    if (data_length > 0) {
        gh_sha256_update(&ctx, data, data_length);
    }
    gh_sha256_final(&ctx, inner_hash);

    gh_sha256_init(&ctx);
    gh_sha256_update(&ctx, outer_pad, sizeof(outer_pad));
    gh_sha256_update(&ctx, inner_hash, sizeof(inner_hash));
    gh_sha256_final(&ctx, output);
}

static int gh_constant_time_equal(const uint8_t *left, const uint8_t *right, size_t length) {
    uint8_t difference = 0;
    size_t i;
    for (i = 0; i < length; ++i) {
        difference |= left[i] ^ right[i];
    }
    return difference == 0;
}

static uint32_t gh_read_u32_be(const uint8_t *value) {
    return ((uint32_t)value[0] << 24)
         | ((uint32_t)value[1] << 16)
         | ((uint32_t)value[2] << 8)
         | (uint32_t)value[3];
}

static void gh_write_u32_be(uint8_t *output, uint32_t value) {
    output[0] = (uint8_t)(value >> 24);
    output[1] = (uint8_t)(value >> 16);
    output[2] = (uint8_t)(value >> 8);
    output[3] = (uint8_t)value;
}

int ghc_encode_client_packet(
    const uint8_t relay_id[GHC_RELAY_ID_SIZE],
    const uint8_t relay_key[GHC_RELAY_KEY_SIZE],
    uint32_t sequence,
    const uint8_t *payload,
    size_t payload_length,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
) {
    static const uint8_t magic[4] = {'G', 'H', 'C', '2'};
    uint8_t digest[32];
    size_t signed_length;

    if (!relay_id || !relay_key || !output || !output_length) return -1;
    if (payload_length > 0 && !payload) return -1;
    if (payload_length > GHC_MAX_DATAGRAM - GHC_CLIENT_OVERHEAD) return -1;
    signed_length = GHC_CLIENT_HEADER_SIZE + payload_length;
    if (output_capacity < signed_length + GHC_MAC_SIZE) return -1;

    memcpy(output, magic, sizeof(magic));
    memcpy(output + 4, relay_id, GHC_RELAY_ID_SIZE);
    gh_write_u32_be(output + 20, sequence);
    if (payload_length > 0) {
        memcpy(output + GHC_CLIENT_HEADER_SIZE, payload, payload_length);
    }

    gh_hmac_sha256(relay_key, GHC_RELAY_KEY_SIZE, output, signed_length, digest);
    memcpy(output + signed_length, digest, GHC_MAC_SIZE);
    *output_length = signed_length + GHC_MAC_SIZE;
    return 0;
}

int ghc_decode_server_packet(
    const uint8_t relay_key[GHC_RELAY_KEY_SIZE],
    const uint8_t *packet,
    size_t packet_length,
    GhcServerPacket *decoded
) {
    static const uint8_t magic[4] = {'G', 'H', 'C', '2'};
    uint8_t digest[32];
    size_t signed_length;
    uint8_t slot;

    if (!relay_key || !packet || !decoded) return -1;
    if (packet_length < GHC_SERVER_OVERHEAD || packet_length > GHC_MAX_DATAGRAM) return -1;
    if (memcmp(packet, magic, sizeof(magic)) != 0) return -1;

    slot = packet[4];
    if (slot != GHC_PLAYER_ONE && slot != GHC_PLAYER_TWO) return -1;

    signed_length = packet_length - GHC_MAC_SIZE;
    gh_hmac_sha256(relay_key, GHC_RELAY_KEY_SIZE, packet, signed_length, digest);
    if (!gh_constant_time_equal(digest, packet + signed_length, GHC_MAC_SIZE)) return -1;

    decoded->sender_slot = slot;
    decoded->sequence = gh_read_u32_be(packet + 5);
    decoded->payload = packet + GHC_SERVER_HEADER_SIZE;
    decoded->payload_length = packet_length - GHC_SERVER_OVERHEAD;
    return 0;
}
