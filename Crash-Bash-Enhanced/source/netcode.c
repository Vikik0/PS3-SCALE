#include "cb_netcode.h"

#include <string.h>

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t get32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

uint32_t cb_net_crc32(const uint8_t *bytes, size_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    unsigned bit;

    if (bytes == NULL && length != 0) return 0;
    for (i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (bit = 0; bit < 8; bit++) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

int cb_net_encode_input(const CBNetInput *input, uint8_t *packet, size_t capacity)
{
    if (input == NULL || packet == NULL || capacity < CB_NET_PACKET_BYTES) return -1;
    memset(packet, 0, CB_NET_PACKET_BYTES);
    put32(packet, CB_NET_MAGIC);
    packet[4] = CB_NET_VERSION;
    packet[5] = CB_NET_TYPE_INPUT;
    put16(packet + 6, 8u);
    put32(packet + 8, input->sequence);
    put32(packet + 12, input->frame);
    put16(packet + 16, input->buttons);
    put16(packet + 18, input->acknowledged_sequence);
    put32(packet + 20, cb_net_crc32(packet, 20u));
    return 0;
}

int cb_net_decode_input(const uint8_t *packet, size_t length, CBNetInput *input)
{
    CBNetInput decoded;

    if (packet == NULL || input == NULL || length != CB_NET_PACKET_BYTES) return -1;
    if (get32(packet) != CB_NET_MAGIC || packet[4] != CB_NET_VERSION ||
        packet[5] != CB_NET_TYPE_INPUT || get16(packet + 6) != 8u) return -1;
    if (get32(packet + 20) != cb_net_crc32(packet, 20u)) return -1;

    decoded.sequence = get32(packet + 8);
    decoded.frame = get32(packet + 12);
    decoded.buttons = get16(packet + 16);
    decoded.acknowledged_sequence = get16(packet + 18);
    *input = decoded;
    return 0;
}
