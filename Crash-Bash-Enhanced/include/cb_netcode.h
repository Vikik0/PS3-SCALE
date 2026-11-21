#ifndef CB_NETCODE_H
#define CB_NETCODE_H

#include <stddef.h>
#include <stdint.h>

#define CB_NET_PACKET_BYTES 24u
#define CB_NET_MAGIC 0x4342494Eu /* "CBIN" */
#define CB_NET_VERSION 1u
#define CB_NET_TYPE_INPUT 1u

typedef struct CBNetInput {
    uint32_t sequence;
    uint32_t frame;
    uint16_t buttons;
    uint16_t acknowledged_sequence;
} CBNetInput;

/* Encodes one deterministic input sample into a fixed-size, network-byte-order packet.
   Returns 0 on success, -1 for invalid arguments. */
int cb_net_encode_input(const CBNetInput *input, uint8_t *packet, size_t capacity);

/* Validates magic/version/type/length/CRC before exposing any decoded values.
   Returns 0 on success, -1 for malformed or corrupted packets. */
int cb_net_decode_input(const uint8_t *packet, size_t length, CBNetInput *input);

/* CRC-32/ISO-HDLC over bytes; exposed for packet diagnostics and tests. */
uint32_t cb_net_crc32(const uint8_t *bytes, size_t length);

#endif
