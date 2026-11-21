#include "ghc_p2p_udp.h"

static void gh_copy_bytes(uint8_t *destination, const uint8_t *source, size_t length) {
    size_t i;
    for (i = 0; i < length; ++i) destination[i] = source[i];
}

static void gh_copy_text(char *destination, const char *source, size_t capacity) {
    size_t i = 0;
    if (!destination || !source || capacity == 0) return;
    while (i + 1u < capacity && source[i]) {
        destination[i] = source[i];
        ++i;
    }
    destination[i] = '\0';
}

static int gh_same_endpoint(
    const char *left_address,
    uint16_t left_port,
    const char *right_address,
    uint16_t right_port
) {
    size_t i = 0;
    if (left_port != right_port) return 0;
    while (left_address[i] && right_address[i]) {
        if (left_address[i] != right_address[i]) return 0;
        ++i;
    }
    return left_address[i] == right_address[i];
}

void ghc_p2p_link_reset(GhcP2PLink *link) {
    size_t i;
    if (!link) return;
    link->socket_fd = -1;
    for (i = 0; i < GHC_RELAY_ID_SIZE; ++i) {
        link->local_relay_id[i] = 0;
        link->peer_relay_id[i] = 0;
    }
    for (i = 0; i < GHC_RELAY_KEY_SIZE; ++i) {
        link->local_relay_key[i] = 0;
        link->peer_relay_key[i] = 0;
    }
    link->peer_public_address[0] = '\0';
    link->peer_local_address[0] = '\0';
    link->peer_observed_address[0] = '\0';
    link->peer_public_port = 0;
    link->peer_local_port = 0;
    link->peer_observed_port = 0;
    link->next_sequence = 1;
    ghc_replay_window_reset(&link->receive_window);
    link->initialized = 0;
}

int ghc_p2p_link_init(
    GhcP2PLink *link,
    int socket_fd,
    const GhcGuestSession *local_session,
    const GhcPeerSnapshot *peer
) {
    if (!link || socket_fd < 0 || !local_session || !peer
        || !local_session->token[0]
        || !peer->peer_present || !peer->candidate_present
        || peer->peer_slot < GHC_PLAYER_ONE || peer->peer_slot > GHC_PLAYER_TWO
        || peer->candidate.public_port == 0 || peer->candidate.local_port == 0) return -1;

    ghc_p2p_link_reset(link);
    link->socket_fd = socket_fd;
    gh_copy_bytes(link->local_relay_id, local_session->relay_id, GHC_RELAY_ID_SIZE);
    gh_copy_bytes(link->local_relay_key, local_session->relay_key, GHC_RELAY_KEY_SIZE);
    gh_copy_bytes(link->peer_relay_id, peer->relay_id, GHC_RELAY_ID_SIZE);
    gh_copy_bytes(link->peer_relay_key, peer->relay_key, GHC_RELAY_KEY_SIZE);
    gh_copy_text(link->peer_public_address, peer->candidate.public_address, sizeof(link->peer_public_address));
    gh_copy_text(link->peer_local_address, peer->candidate.local_address, sizeof(link->peer_local_address));
    link->peer_public_port = peer->candidate.public_port;
    link->peer_local_port = peer->candidate.local_port;
    link->next_sequence = 1;
    ghc_replay_window_reset(&link->receive_window);
    link->initialized = 1;
    return 0;
}

int ghc_p2p_send(GhcP2PLink *link, const uint8_t *payload, size_t payload_length) {
    uint8_t packet[GHC_MAX_DATAGRAM];
    size_t packet_length = 0;
    uint32_t sequence;
    int public_result;
    int local_result = -1;

    if (!link || !link->initialized || !payload || payload_length == 0
        || payload_length > GHC_MAX_DATAGRAM - GHC_CLIENT_OVERHEAD) return -1;

    sequence = link->next_sequence++;
    if (ghc_encode_client_packet(
            link->local_relay_id,
            link->peer_relay_key,
            sequence,
            payload,
            payload_length,
            packet,
            sizeof(packet),
            &packet_length
        ) != 0) return -2;

    public_result = -1;
    if (link->peer_observed_address[0] && link->peer_observed_port) {
        public_result = ghc_udp_sendto_ipv4(
            link->socket_fd,
            link->peer_observed_address,
            link->peer_observed_port,
            packet,
            packet_length
        );
    }
    if (public_result < 0) {
        public_result = ghc_udp_sendto_ipv4(
            link->socket_fd,
            link->peer_public_address,
            link->peer_public_port,
            packet,
            packet_length
        );
    }

    if (!gh_same_endpoint(
            link->peer_public_address,
            link->peer_public_port,
            link->peer_local_address,
            link->peer_local_port
        ) && !gh_same_endpoint(
            link->peer_observed_address,
            link->peer_observed_port,
            link->peer_local_address,
            link->peer_local_port
        )) {
        local_result = ghc_udp_sendto_ipv4(
            link->socket_fd,
            link->peer_local_address,
            link->peer_local_port,
            packet,
            packet_length
        );
    }

    if (public_result < 0 && local_result < 0) return -3;
    return (int)payload_length;
}

int ghc_p2p_receive(
    GhcP2PLink *link,
    uint8_t *payload,
    size_t payload_capacity,
    uint32_t *sequence
) {
    uint8_t packet[GHC_MAX_DATAGRAM];
    char source_address[16];
    uint16_t source_port = 0;
    size_t packet_length = 0;
    GhcClientPacket decoded;
    int result;

    if (!link || !link->initialized || !payload || payload_capacity == 0 || !sequence) return -1;

    result = ghc_udp_recvfrom_ipv4(
        link->socket_fd,
        packet,
        sizeof(packet),
        &packet_length,
        source_address,
        &source_port
    );
    if (result != 0) return -2;

    /*
     * Do not hard-reject an authenticated peer because a symmetric NAT changed
     * its source port. GHC2 HMAC + paired relay ID are the identity checks.
     * Learn the observed endpoint only after authentication and replay checks.
     */
    if (ghc_decode_client_packet(link->local_relay_key, packet, packet_length, &decoded) != 0) return -4;
    {
        size_t i;
        uint8_t difference = 0;
        for (i = 0; i < GHC_RELAY_ID_SIZE; ++i) difference |= (uint8_t)(decoded.relay_id[i] ^ link->peer_relay_id[i]);
        if (difference != 0) return -5;
    }
    if (decoded.payload_length > payload_capacity) return -7;
    if (!ghc_replay_window_accept(&link->receive_window, decoded.sequence)) return -6;

    gh_copy_text(link->peer_observed_address, source_address, sizeof(link->peer_observed_address));
    link->peer_observed_port = source_port;
    gh_copy_bytes(payload, decoded.payload, decoded.payload_length);
    *sequence = decoded.sequence;
    return (int)decoded.payload_length;
}


int ghc_p2p_send_game_message(GhcP2PLink *link, const GhcGameMessage *message) {
    uint8_t packet[GHC_GAME_MAX_PACKET_SIZE];
    size_t packet_length = 0;
    if (ghc_game_encode_message(message, packet, sizeof(packet), &packet_length) != 0) return -1;
    return ghc_p2p_send(link, packet, packet_length);
}

int ghc_p2p_receive_game_message(GhcP2PLink *link, GhcGameMessage *message, uint32_t *sequence) {
    uint8_t packet[GHC_GAME_MAX_PACKET_SIZE];
    int packet_length;
    if (!message || !sequence) return -1;
    packet_length = ghc_p2p_receive(link, packet, sizeof(packet), sequence);
    if (packet_length < 0) return packet_length;
    return ghc_game_decode_message(packet, (size_t)packet_length, message);
}
