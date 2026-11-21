#include "ghc_udp_ps3.h"

#include <sys/socket.h>
#include <netinet/in.h>

static void gh_zero(void *destination, size_t length) {
    uint8_t *bytes = (uint8_t *)destination;
    size_t i;
    for (i = 0; i < length; ++i) bytes[i] = 0;
}

static int gh_parse_ipv4(const char *text, uint32_t *address) {
    uint32_t octets[4] = {0, 0, 0, 0};
    uint32_t index = 0;
    uint32_t digits = 0;
    const char *cursor = text;

    if (!text || !address) return -1;
    while (*cursor) {
        if (*cursor >= '0' && *cursor <= '9') {
            octets[index] = octets[index] * 10u + (uint32_t)(*cursor - '0');
            if (octets[index] > 255u || ++digits > 3u) return -1;
        } else if (*cursor == '.') {
            if (digits == 0 || index >= 3u) return -1;
            ++index;
            digits = 0;
        } else {
            return -1;
        }
        ++cursor;
    }
    if (index != 3u || digits == 0) return -1;
    *address = (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3];
    return 0;
}

static void gh_format_ipv4(uint32_t address, char output[16]) {
    GhcStunMappedAddress mapped;
    mapped.ipv4[0] = (uint8_t)(address >> 24);
    mapped.ipv4[1] = (uint8_t)(address >> 16);
    mapped.ipv4[2] = (uint8_t)(address >> 8);
    mapped.ipv4[3] = (uint8_t)address;
    mapped.port = 0;
    (void)ghc_stun_format_ipv4(&mapped, output);
}

static int gh_make_sockaddr(const char *ipv4, uint16_t port, struct sockaddr_in *address) {
    uint32_t parsed;
    if (!address || port == 0 || gh_parse_ipv4(ipv4, &parsed) != 0) return -1;
    gh_zero(address, sizeof(*address));
    address->sin_len = (uint8_t)sizeof(*address);
    address->sin_family = AF_INET;
    address->sin_port = htons(port);
    address->sin_addr.s_addr = parsed;
    return 0;
}

int ghc_udp_open(uint16_t local_port, int *socket_fd, uint16_t *bound_port) {
    int descriptor;
    struct sockaddr_in local_address;
    socklen_t address_length = (socklen_t)sizeof(local_address);

    if (!socket_fd || !bound_port) return -1;
    *socket_fd = -1;
    *bound_port = 0;

    descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (descriptor < 0) return -2;

    gh_zero(&local_address, sizeof(local_address));
    local_address.sin_len = (uint8_t)sizeof(local_address);
    local_address.sin_family = AF_INET;
    local_address.sin_port = htons(local_port);
    local_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(descriptor, (const struct sockaddr *)&local_address, sizeof(local_address)) != 0) {
        (void)closesocket(descriptor);
        return -3;
    }
    if (getsockname(descriptor, (struct sockaddr *)&local_address, &address_length) != 0) {
        (void)closesocket(descriptor);
        return -4;
    }

    *socket_fd = descriptor;
    *bound_port = ntohs(local_address.sin_port);
    return 0;
}

int ghc_udp_close(int socket_fd) {
    if (socket_fd < 0) return -1;
    return closesocket(socket_fd);
}

int ghc_udp_set_nonblocking(int socket_fd, int enabled) {
    int value = enabled ? 1 : 0;
    if (socket_fd < 0) return -1;
    return setsockopt(socket_fd, SOL_SOCKET, SO_NBIO, &value, sizeof(value));
}

int ghc_udp_get_route_ipv4(
    const char *remote_ipv4,
    uint16_t remote_port,
    char local_ipv4[16]
) {
    int descriptor;
    struct sockaddr_in remote_address;
    struct sockaddr_in local_address;
    socklen_t local_length = (socklen_t)sizeof(local_address);

    if (!local_ipv4 || gh_make_sockaddr(remote_ipv4, remote_port, &remote_address) != 0) return -1;
    descriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (descriptor < 0) return -2;

    if (connect(descriptor, (const struct sockaddr *)&remote_address, sizeof(remote_address)) != 0) {
        (void)closesocket(descriptor);
        return -3;
    }
    gh_zero(&local_address, sizeof(local_address));
    if (getsockname(descriptor, (struct sockaddr *)&local_address, &local_length) != 0) {
        (void)closesocket(descriptor);
        return -4;
    }
    (void)closesocket(descriptor);

    if (local_address.sin_addr.s_addr == INADDR_ANY) return -5;
    gh_format_ipv4(local_address.sin_addr.s_addr, local_ipv4);
    return 0;
}

int ghc_udp_sendto_ipv4(
    int socket_fd,
    const char *remote_ipv4,
    uint16_t remote_port,
    const void *data,
    size_t data_length
) {
    struct sockaddr_in remote_address;
    ssize_t sent;
    if (socket_fd < 0 || !data || data_length == 0 || data_length > GHC_UDP_MAX_DATAGRAM_BYTES) return -1;
    if (gh_make_sockaddr(remote_ipv4, remote_port, &remote_address) != 0) return -1;
    sent = sendto(
        socket_fd,
        data,
        data_length,
        0,
        (const struct sockaddr *)&remote_address,
        sizeof(remote_address)
    );
    if (sent < 0) return -2;
    if ((size_t)sent != data_length) return -3;
    return (int)sent;
}

int ghc_udp_recvfrom_ipv4(
    int socket_fd,
    void *buffer,
    size_t buffer_capacity,
    size_t *received,
    char source_ipv4[16],
    uint16_t *source_port
) {
    struct sockaddr_in source;
    socklen_t source_length = (socklen_t)sizeof(source);
    ssize_t result;

    if (socket_fd < 0 || !buffer || buffer_capacity == 0
        || buffer_capacity > GHC_UDP_MAX_DATAGRAM_BYTES
        || !received || !source_ipv4 || !source_port) return -1;

    gh_zero(&source, sizeof(source));
    result = recvfrom(
        socket_fd,
        buffer,
        buffer_capacity,
        0,
        (struct sockaddr *)&source,
        &source_length
    );
    if (result < 0) return -2;

    gh_format_ipv4(source.sin_addr.s_addr, source_ipv4);
    *source_port = ntohs(source.sin_port);
    *received = (size_t)result;
    return 0;
}

int ghc_udp_stun_send_request(
    int socket_fd,
    const char *stun_ipv4,
    uint16_t stun_port,
    const uint8_t transaction_id[GHC_STUN_TRANSACTION_ID_SIZE]
) {
    uint8_t packet[GHC_STUN_HEADER_SIZE];
    size_t packet_length = 0;
    int result;
    if (ghc_stun_build_binding_request(transaction_id, packet, sizeof(packet), &packet_length) != 0) return -1;
    result = ghc_udp_sendto_ipv4(socket_fd, stun_ipv4, stun_port, packet, packet_length);
    return result == (int)packet_length ? 0 : -2;
}

int ghc_udp_stun_receive_response(
    int socket_fd,
    const char *stun_ipv4,
    uint16_t stun_port,
    const uint8_t transaction_id[GHC_STUN_TRANSACTION_ID_SIZE],
    GhcStunMappedAddress *mapped
) {
    uint8_t packet[GHC_UDP_MAX_DATAGRAM_BYTES];
    size_t received = 0;
    char source_ipv4[16];
    uint16_t source_port = 0;
    int result;

    if (!mapped || !transaction_id) return -1;
    result = ghc_udp_recvfrom_ipv4(
        socket_fd,
        packet,
        sizeof(packet),
        &received,
        source_ipv4,
        &source_port
    );
    if (result != 0) return result;
    if (source_port != stun_port) return -2;
    {
        uint32_t expected_address;
        uint32_t source_address;
        if (gh_parse_ipv4(stun_ipv4, &expected_address) != 0
            || gh_parse_ipv4(source_ipv4, &source_address) != 0
            || source_address != expected_address) return -2;
    }
    return ghc_stun_parse_binding_response(packet, received, transaction_id, mapped);
}
