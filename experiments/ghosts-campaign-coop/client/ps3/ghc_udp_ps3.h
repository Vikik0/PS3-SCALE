#pragma once
#include <stddef.h>
#include <stdint.h>

#include "ghc_stun.h"

#define GHC_UDP_MAX_DATAGRAM_BYTES 1200u

/* Uses the PS3 libnet socket imports consistently with raw libnet descriptors.
 * Do not pass these descriptors to libc close() or mix them with tagged
 * descriptors from a separate librt socket layer. */
int ghc_udp_open(uint16_t local_port, int *socket_fd, uint16_t *bound_port);
int ghc_udp_close(int socket_fd);
int ghc_udp_set_nonblocking(int socket_fd, int enabled);

/* Resolve the route-selected local IPv4 address using a temporary UDP socket. */
int ghc_udp_get_route_ipv4(
    const char *remote_ipv4,
    uint16_t remote_port,
    char local_ipv4[16]
);

int ghc_udp_sendto_ipv4(
    int socket_fd,
    const char *remote_ipv4,
    uint16_t remote_port,
    const void *data,
    size_t data_length
);

int ghc_udp_recvfrom_ipv4(
    int socket_fd,
    void *buffer,
    size_t buffer_capacity,
    size_t *received,
    char source_ipv4[16],
    uint16_t *source_port
);

/* Send a STUN Binding Request and parse its matching response. */
int ghc_udp_stun_send_request(
    int socket_fd,
    const char *stun_ipv4,
    uint16_t stun_port,
    const uint8_t transaction_id[GHC_STUN_TRANSACTION_ID_SIZE]
);

int ghc_udp_stun_receive_response(
    int socket_fd,
    const char *stun_ipv4,
    uint16_t stun_port,
    const uint8_t transaction_id[GHC_STUN_TRANSACTION_ID_SIZE],
    GhcStunMappedAddress *mapped
);
