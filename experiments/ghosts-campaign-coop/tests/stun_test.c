#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../client/ps3/ghc_stun.h"

static void write_u16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static void write_u32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}

static void make_header(uint8_t *packet, uint16_t type, uint16_t length, const uint8_t txid[12]) {
    write_u16(packet, type);
    write_u16(packet + 2, length);
    packet[4] = 0x21; packet[5] = 0x12; packet[6] = 0xA4; packet[7] = 0x42;
    memcpy(packet + 8, txid, 12);
}

static void test_binding_request(void) {
    const uint8_t txid[12] = {0,1,2,3,4,5,6,7,8,9,10,11};
    uint8_t packet[20];
    size_t length = 0;
    assert(ghc_stun_build_binding_request(txid, packet, sizeof(packet), &length) == 0);
    assert(length == 20);
    assert(packet[0] == 0 && packet[1] == 1);
    assert(packet[2] == 0 && packet[3] == 0);
    assert(packet[4] == 0x21 && packet[5] == 0x12 && packet[6] == 0xA4 && packet[7] == 0x42);
    assert(memcmp(packet + 8, txid, 12) == 0);
}

static void test_xor_mapped_address(void) {
    const uint8_t txid[12] = {1,2,3,4,5,6,7,8,9,10,11,12};
    const uint8_t ip[4] = {203,0,113,7};
    const uint16_t port = 54321;
    uint8_t packet[32];
    uint32_t address = ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16)
        | ((uint32_t)ip[2] << 8) | ip[3];
    GhcStunMappedAddress mapped;
    char formatted[16];

    make_header(packet, GHC_STUN_BINDING_SUCCESS, 12, txid);
    write_u16(packet + 20, 0x0020);
    write_u16(packet + 22, 8);
    packet[24] = 0;
    packet[25] = 1;
    write_u16(packet + 26, (uint16_t)(port ^ 0x2112u));
    write_u32(packet + 28, address ^ 0x2112A442u);

    assert(ghc_stun_parse_binding_response(packet, sizeof(packet), txid, &mapped) == 0);
    assert(mapped.port == port);
    assert(memcmp(mapped.ipv4, ip, 4) == 0);
    assert(ghc_stun_format_ipv4(&mapped, formatted) == 0);
    assert(strcmp(formatted, "203.0.113.7") == 0);
}

static void test_mapped_address_fallback(void) {
    const uint8_t txid[12] = {9,8,7,6,5,4,3,2,1,0,1,2};
    const uint8_t ip[4] = {192,168,1,90};
    uint8_t packet[32];
    GhcStunMappedAddress mapped;

    make_header(packet, GHC_STUN_BINDING_SUCCESS, 12, txid);
    write_u16(packet + 20, 0x0001);
    write_u16(packet + 22, 8);
    packet[24] = 0;
    packet[25] = 1;
    write_u16(packet + 26, 40000);
    write_u32(packet + 28, ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16)
        | ((uint32_t)ip[2] << 8) | ip[3]);

    assert(ghc_stun_parse_binding_response(packet, sizeof(packet), txid, &mapped) == 0);
    assert(mapped.port == 40000);
    assert(memcmp(mapped.ipv4, ip, 4) == 0);
}

static void test_reject_bad_transaction_and_malformed(void) {
    const uint8_t txid[12] = {1,1,1,1,1,1,1,1,1,1,1,1};
    const uint8_t wrong[12] = {2,2,2,2,2,2,2,2,2,2,2,2};
    uint8_t packet[32];
    GhcStunMappedAddress mapped;

    make_header(packet, GHC_STUN_BINDING_SUCCESS, 12, txid);
    write_u16(packet + 20, 0x0020);
    write_u16(packet + 22, 8);
    packet[24] = 0;
    packet[25] = 1;
    write_u16(packet + 26, 1234 ^ 0x2112u);
    write_u32(packet + 28, 0x01020304u ^ 0x2112A442u);

    assert(ghc_stun_parse_binding_response(packet, sizeof(packet), wrong, &mapped) == -1);
    assert(ghc_stun_parse_binding_response(packet, sizeof(packet) - 1, txid, &mapped) == -1);

    write_u16(packet + 22, 40);
    assert(ghc_stun_parse_binding_response(packet, sizeof(packet), txid, &mapped) == -1);

    make_header(packet, GHC_STUN_BINDING_ERROR, 0, txid);
    assert(ghc_stun_parse_binding_response(packet, 20, txid, &mapped) == -2);
}

int main(void) {
    test_binding_request();
    test_xor_mapped_address();
    test_mapped_address_fallback();
    test_reject_bad_transaction_and_malformed();
    puts("GHC2 STUN parser tests: PASS");
    return 0;
}
