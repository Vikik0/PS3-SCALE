#include "cb_peer.h"

#include <string.h>

#ifdef __PSL1GHT__
#include <arpa/inet.h>
#include <net/net.h>
#include <netinet/in.h>
typedef net_socklen_t cb_socklen_t;
#define cb_socket netSocket
#define cb_bind netBind
#define cb_sendto netSendTo
#define cb_recvfrom netRecvFrom
#define cb_getsockname netGetSockName
#define cb_close_socket netClose
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef socklen_t cb_socklen_t;
#define cb_socket socket
#define cb_bind bind
#define cb_sendto sendto
#define cb_recvfrom recvfrom
#define cb_getsockname getsockname
#define cb_close_socket close
#endif

int cb_peer_open(CBPeer *peer, uint16_t local_port)
{
    struct sockaddr_in local;

    if (peer == NULL) return -1;
    memset(peer, 0, sizeof(*peer));
    peer->socket_fd = -1;

    peer->socket_fd = cb_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (peer->socket_fd < 0) return -1;

    memset(&local, 0, sizeof(local));
#ifdef __PSL1GHT__
    local.sin_len = (uint8_t)sizeof(local);
#endif
    local.sin_family = AF_INET;
    local.sin_port = htons(local_port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    if (cb_bind(peer->socket_fd, (const struct sockaddr *)&local,
                (cb_socklen_t)sizeof(local)) != 0) {
        cb_close_socket(peer->socket_fd);
        peer->socket_fd = -1;
        return -1;
    }
    return 0;
}

void cb_peer_close(CBPeer *peer)
{
    if (peer == NULL) return;
    if (peer->socket_fd >= 0) cb_close_socket(peer->socket_fd);
    memset(peer, 0, sizeof(*peer));
    peer->socket_fd = -1;
}

int cb_peer_set_remote(CBPeer *peer, const char *ipv4, uint16_t port)
{
    struct in_addr address;

    if (peer == NULL || peer->socket_fd < 0 || ipv4 == NULL || port == 0) return -1;
    if (inet_pton(AF_INET, ipv4, &address) != 1) return -1;
    peer->remote_ipv4 = address.s_addr;
    peer->remote_port = htons(port);
    peer->remote_valid = 1;
    peer->rx_valid = 0;
    peer->rx_sequence = 0;
    return 0;
}

uint16_t cb_peer_local_port(CBPeer *peer)
{
    struct sockaddr_in local;
    cb_socklen_t length = (cb_socklen_t)sizeof(local);

    if (peer == NULL || peer->socket_fd < 0) return 0;
    memset(&local, 0, sizeof(local));
    if (cb_getsockname(peer->socket_fd, (struct sockaddr *)&local, &length) != 0) return 0;
    return ntohs(local.sin_port);
}

int cb_peer_send_input(CBPeer *peer, uint32_t frame, uint16_t buttons)
{
    struct sockaddr_in remote;
    CBNetInput input;
    uint8_t packet[CB_NET_PACKET_BYTES];
    int sent;

    if (peer == NULL || peer->socket_fd < 0 || !peer->remote_valid) return -1;
    memset(&remote, 0, sizeof(remote));
#ifdef __PSL1GHT__
    remote.sin_len = (uint8_t)sizeof(remote);
#endif
    remote.sin_family = AF_INET;
    remote.sin_port = peer->remote_port;
    remote.sin_addr.s_addr = peer->remote_ipv4;

    input.sequence = ++peer->tx_sequence;
    input.frame = frame;
    input.buttons = buttons;
    input.acknowledged_sequence = (uint16_t)peer->rx_sequence;
    if (cb_net_encode_input(&input, packet, sizeof(packet)) != 0) return -1;

    sent = (int)cb_sendto(peer->socket_fd, packet, sizeof(packet), 0,
                          (const struct sockaddr *)&remote,
                          (cb_socklen_t)sizeof(remote));
    return sent == (int)sizeof(packet) ? 0 : -1;
}

int cb_peer_poll_input(CBPeer *peer, CBNetInput *input)
{
    uint8_t packet[CB_NET_PACKET_BYTES];
    struct sockaddr_in source;
    cb_socklen_t source_length = (cb_socklen_t)sizeof(source);
    CBNetInput decoded;
    int received;

    if (peer == NULL || peer->socket_fd < 0 || input == NULL) return -1;

    memset(&source, 0, sizeof(source));
    received = (int)cb_recvfrom(peer->socket_fd, packet, sizeof(packet), MSG_DONTWAIT,
                                (struct sockaddr *)&source, &source_length);
    if (received != (int)sizeof(packet)) return 0;

    if (cb_net_decode_input(packet, sizeof(packet), &decoded) != 0) return 0;
    if (peer->remote_valid) {
        if (source.sin_addr.s_addr != peer->remote_ipv4 ||
            source.sin_port != peer->remote_port) return 0;
    } else {
        peer->remote_ipv4 = source.sin_addr.s_addr;
        peer->remote_port = source.sin_port;
        peer->remote_valid = 1;
    }

    if (peer->rx_valid && (int32_t)(decoded.sequence - peer->rx_sequence) <= 0) return 0;
    peer->rx_sequence = decoded.sequence;
    peer->rx_valid = 1;
    *input = decoded;
    return 1;
}
