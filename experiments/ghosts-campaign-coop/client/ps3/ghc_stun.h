#pragma once
#include <stddef.h>
#include <stdint.h>

#define GHC_STUN_HEADER_SIZE 20u
#define GHC_STUN_TRANSACTION_ID_SIZE 12u
#define GHC_STUN_BINDING_REQUEST 0x0001u
#define GHC_STUN_BINDING_SUCCESS 0x0101u
#define GHC_STUN_BINDING_ERROR 0x0111u

typedef struct {
    uint8_t ipv4[4];
    uint16_t port;
} GhcStunMappedAddress;

/* Build an RFC 5389 Binding Request using the caller-provided transaction ID. */
int ghc_stun_build_binding_request(
    const uint8_t transaction_id[GHC_STUN_TRANSACTION_ID_SIZE],
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
);

/* Parse an IPv4 XOR-MAPPED-ADDRESS (preferred) or MAPPED-ADDRESS response. */
int ghc_stun_parse_binding_response(
    const uint8_t *packet,
    size_t packet_length,
    const uint8_t expected_transaction_id[GHC_STUN_TRANSACTION_ID_SIZE],
    GhcStunMappedAddress *mapped
);

/* Format the mapped IPv4 address as dotted decimal. */
int ghc_stun_format_ipv4(const GhcStunMappedAddress *mapped, char output[16]);
