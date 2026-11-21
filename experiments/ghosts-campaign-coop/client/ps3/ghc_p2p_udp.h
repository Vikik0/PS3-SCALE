#pragma once
#include <stddef.h>
#include <stdint.h>

#include "ghc_lobby.h"
#include "ghc_protocol.h"
#include "ghc_udp_ps3.h"

typedef struct {
    int socket_fd;
    uint8_t local_relay_id[GHC_RELAY_ID_SIZE];
    uint8_t local_relay_key[GHC_RELAY_KEY_SIZE];
    uint8_t peer_relay_id[GHC_RELAY_ID_SIZE];
    uint8_t peer_relay_key[GHC_RELAY_KEY_SIZE];
    char peer_public_address[16];
    uint16_t peer_public_port;
    char peer_local_address[16];
    char peer_observed_address[16];
    uint16_t peer_observed_port;
    uint16_t peer_local_port;
    uint32_t next_sequence;
    GhcReplayWindow receive_window;
    uint8_t initialized;
} GhcP2PLink;

/* Configure direct P2P authentication using the recipient's relay key. */
int ghc_p2p_link_init(
    GhcP2PLink *link,
    int socket_fd,
    const GhcGuestSession *local_session,
    const GhcPeerSnapshot *peer
);

/* Send one authenticated GHC2 payload to the peer's public and LAN candidates. */
int ghc_p2p_send(GhcP2PLink *link, const uint8_t *payload, size_t payload_length);

/* Receive and authenticate one peer datagram. Returns payload length or a negative error. */
int ghc_p2p_receive(
    GhcP2PLink *link,
    uint8_t *payload,
    size_t payload_capacity,
    uint32_t *sequence
);

void ghc_p2p_link_reset(GhcP2PLink *link);
