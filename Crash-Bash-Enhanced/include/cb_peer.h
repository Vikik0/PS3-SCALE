#ifndef CB_PEER_H
#define CB_PEER_H

#include <stdint.h>

#include "cb_netcode.h"

typedef struct CBPeer {
    int socket_fd;
    uint32_t remote_ipv4; /* network byte order */
    uint16_t remote_port; /* network byte order */
    uint32_t tx_sequence;
    uint32_t rx_sequence;
    int remote_valid;
    int rx_valid;
} CBPeer;

/* Opens a UDP socket. local_port=0 asks the OS for an available port. */
int cb_peer_open(CBPeer *peer, uint16_t local_port);
void cb_peer_close(CBPeer *peer);

/* Sets the only accepted/sent peer address. IPv4 text is used deliberately:
   the current lobby exchanges IPv4 endpoints, not DNS names. */
int cb_peer_set_remote(CBPeer *peer, const char *ipv4, uint16_t port);

/* Returns the bound local UDP port, or 0 on error. */
uint16_t cb_peer_local_port(CBPeer *peer);

/* Sends one input sample to the configured peer. */
int cb_peer_send_input(CBPeer *peer, uint32_t frame, uint16_t buttons);

/* Polls one datagram. 1=new valid input, 0=no usable datagram, -1=bad state.
   If no remote is configured, the first valid datagram pins the remote address. */
int cb_peer_poll_input(CBPeer *peer, CBNetInput *input);

#endif
