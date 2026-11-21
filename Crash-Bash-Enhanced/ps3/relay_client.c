#include "relay_client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define CB_HTTP_RESPONSE_MAX 4096
#define CB_RELAY_ENVELOPE_SIZE 22

static int send_all(int fd, const char *data, size_t length)
{
    size_t sent = 0;
    while (sent < length) {
        ssize_t count = send(fd, data + sent, length - sent, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -1;
        sent += (size_t)count;
    }
    return 0;
}

static int find_header_end(const char *data, size_t length, size_t *end)
{
    size_t i;
    if (length < 4u) return 0;
    for (i = 0; i + 4u <= length; ++i) {
        if (data[i] == '\r' && data[i + 1u] == '\n' &&
            data[i + 2u] == '\r' && data[i + 3u] == '\n') {
            *end = i + 4u;
            return 1;
        }
    }
    return 0;
}

static int equal_ci_n(const char *left, const char *right, size_t length)
{
    size_t i;
    for (i = 0; i < length; ++i) {
        char a = left[i];
        char b = right[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static int http_content_length(const char *headers, size_t header_length,
                               size_t *content_length)
{
    const char *line = strstr(headers, "\r\n");
    if (!line) return 0;
    line += 2;

    while ((size_t)(line - headers) < header_length) {
        const char *end = strstr(line, "\r\n");
        const char *colon;
        const char *value;
        char *number_end;
        unsigned long parsed;

        if (!end || end == line) break;
        colon = (const char *)memchr(line, ':', (size_t)(end - line));
        if (colon && (size_t)(colon - line) == 14u &&
            equal_ci_n(line, "Content-Length", 14u)) {
            value = colon + 1;
            while (value < end && (*value == ' ' || *value == '\t')) value++;
            parsed = strtoul(value, &number_end, 10);
            if (number_end == value || parsed > CB_HTTP_RESPONSE_MAX) return 0;
            *content_length = (size_t)parsed;
            return 1;
        }
        line = end + 2;
    }
    return 0;
}

static int json_hex_field(const char *json, const char *field,
                          char *output, size_t output_capacity,
                          size_t expected_length)
{
    char needle[48];
    const char *p;
    size_t i;

    if (snprintf(needle, sizeof(needle), "\"%s\"", field) >= (int)sizeof(needle)) return -1;
    p = strstr(json, needle);
    if (!p) return -1;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p++ != ':') return -1;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p++ != '"') return -1;
    if (expected_length + 1u > output_capacity) return -1;

    for (i = 0; i < expected_length; ++i) {
        char c = p[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) return -1;
        output[i] = c;
    }
    if (p[expected_length] != '"') return -1;
    output[expected_length] = '\0';
    return 0;
}

static int json_string_field(const char *json, const char *field,
                             char *output, size_t output_capacity)
{
    char needle[48];
    const char *p;
    const char *end;
    size_t length;

    if (snprintf(needle, sizeof(needle), "\"%s\"", field) >= (int)sizeof(needle)) return -1;
    p = strstr(json, needle);
    if (!p) return -1;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p++ != ':') return -1;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p++ != '"') return -1;
    end = strchr(p, '"');
    if (!end) return -1;
    length = (size_t)(end - p);
    if (length == 0 || length >= output_capacity) return -1;
    memcpy(output, p, length);
    output[length] = '\0';
    return 0;
}

static int json_uint_field(const char *json, const char *field, unsigned int *output)
{
    char needle[48];
    const char *p;
    char *end;
    unsigned long value;

    if (!json || !output ||
        snprintf(needle, sizeof(needle), "\"%s\"", field) >= (int)sizeof(needle)) return -1;
    p = strstr(json, needle);
    if (!p) return -1;
    p += strlen(needle);
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p++ != ':') return -1;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    value = strtoul(p, &end, 10);
    if (end == p || value > 0xfffffffful) return -1;
    *output = (unsigned int)value;
    return 0;
}

static int parse_session_id(const char hex[33], uint8_t output[16])
{
    size_t i;
    for (i = 0; i < 16u; ++i) {
        char pair[3];
        char *end;
        unsigned long value;
        pair[0] = hex[i * 2u];
        pair[1] = hex[i * 2u + 1u];
        pair[2] = '\0';
        value = strtoul(pair, &end, 16);
        if (*end != '\0') return -1;
        output[i] = (uint8_t)value;
    }
    return 0;
}

static void parse_room_roster(CbRelayClient *client, const char *json)
{
    const char *players;
    const char *cursor;
    const char *array_end;
    CbRelayPeer next[CB_RELAY_MAX_PEERS];
    size_t count = 0;

    if (!client || !json) return;
    players = strstr(json, "\"players\"");
    if (!players) return;
    players = strchr(players, '[');
    if (!players) return;
    array_end = strchr(players, ']');
    if (!array_end) return;

    memset(next, 0, sizeof(next));
    cursor = players + 1;
    while (cursor < array_end && count < CB_RELAY_MAX_PEERS) {
        const char *object = strchr(cursor, '{');
        const char *object_end;
        char buffer[256];
        size_t length;
        char id_hex[33];
        unsigned int slot;
        CbRelayPeer *peer;

        if (!object || object >= array_end) break;
        object_end = strchr(object, '}');
        if (!object_end || object_end > array_end) break;
        length = (size_t)(object_end - object + 1);
        if (length >= sizeof(buffer)) {
            cursor = object_end + 1;
            continue;
        }
        memcpy(buffer, object, length);
        buffer[length] = '\0';
        if (json_hex_field(buffer, "player_id", id_hex, sizeof(id_hex), 32u) == 0 &&
            json_uint_field(buffer, "slot", &slot) == 0 &&
            slot >= 1u && slot <= CB_RELAY_MAX_PEERS) {
            peer = &next[slot - 1u];
            if (parse_session_id(id_hex, peer->session_id) == 0 &&
                cb_relay_session_address(peer->session_id, &peer->address_network,
                                         &peer->port_host) == 0) {
                peer->slot = (uint8_t)(slot - 1u);
                peer->valid = 1;
                count++;
            }
        }
        cursor = object_end + 1;
    }

    memcpy(client->peers, next, sizeof(next));
    client->peer_count = count;
}

static int clean_display_name(const char *input, char output[17])
{
    size_t i;
    size_t out = 0;
    if (!input || !*input) input = "PS3 Player";

    for (i = 0; input[i] && out < 16u; ++i) {
        char c = input[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-') {
            output[out++] = c;
        }
    }
    while (out > 0 && output[out - 1u] == ' ') out--;
    if (out == 0) {
        memcpy(output, "PS3 Player", 10u);
        out = 10u;
    }
    output[out] = '\0';
    return 0;
}

static int request_room_ticket(CbRelayClient *client, const char *host, unsigned short port,
                                const char *join_room_code, int create_room,
                                const char *display_name, int timeout_ms,
                                char ticket[65], char session_id[33],
                                char host_session_id[33], char room_code_out[7])
{
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    struct timeval timeout;
    char service[8];
    char host_header[288];
    char safe_name[17];
    char request_body[96];
    char api_path[96];
    char request[1024];
    char response[CB_HTTP_RESPONSE_MAX + 1];
    size_t received = 0;
    size_t header_end = 0;
    size_t body_length = 0;
    int have_content_length = 0;
    int fd = -1;
    int request_length;
    int result = -1;

    if (!host || !*host || port == 0 || !room_code_out) return -1;
    if (create_room) {
        strcpy(api_path, "/v1/rooms/host-guest");
    } else {
        if (!join_room_code || strlen(join_room_code) != 6u) return -1;
        for (size_t i = 0; i < 6u; ++i) {
            char c = join_room_code[i];
            if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return -1;
        }
        if (snprintf(api_path, sizeof(api_path), "/v1/rooms/%s/join-guest",
                     join_room_code) >= (int)sizeof(api_path)) return -1;
    }

    clean_display_name(display_name, safe_name);
    snprintf(request_body, sizeof(request_body), "{\"display_name\":\"%s\"}", safe_name);
    if (port == 80u) {
        if (snprintf(host_header, sizeof(host_header), "%s", host) >= (int)sizeof(host_header)) return -1;
    } else {
        if (snprintf(host_header, sizeof(host_header), "%s:%u", host,
                     (unsigned int)port) >= (int)sizeof(host_header)) return -1;
    }

    request_length = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "Content-Type: application/json\r\n"
        "Accept: application/json\r\n"
        "Content-Length: %lu\r\n"
        "\r\n%s",
        api_path, host_header, (unsigned long)strlen(request_body), request_body);
    if (request_length < 0 || (size_t)request_length >= sizeof(request)) return -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    snprintf(service, sizeof(service), "%u", (unsigned int)port);
    if (getaddrinfo(host, service, &hints, &addresses) != 0) return -1;

    timeout.tv_sec = timeout_ms > 0 ? timeout_ms / 1000 : 5;
    timeout.tv_usec = timeout_ms > 0 ? (timeout_ms % 1000) * 1000 : 0;

    for (address = addresses; address; address = address->ai_next) {
        fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (fd < 0) continue;
        (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        if (connect(fd, address->ai_addr, (socklen_t)address->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(addresses);
    if (fd < 0) return -1;

    if (send_all(fd, request, (size_t)request_length) != 0) goto cleanup;

    memset(response, 0, sizeof(response));
    while (received < CB_HTTP_RESPONSE_MAX) {
        ssize_t count = recv(fd, response + received, CB_HTTP_RESPONSE_MAX - received, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) goto cleanup;
        if (count == 0) break;
        received += (size_t)count;
        response[received] = '\0';

        if (!header_end && find_header_end(response, received, &header_end)) {
            have_content_length = http_content_length(response, header_end, &body_length);
            if (have_content_length && received >= header_end + body_length) break;
        }
    }

    if (!header_end ||
        (strncmp(response, "HTTP/1.1 201", 12) != 0 &&
         strncmp(response, "HTTP/1.0 201", 12) != 0)) goto cleanup;
    if (have_content_length && received < header_end + body_length) goto cleanup;
    if (json_hex_field(response + header_end, "ticket", ticket, 65u, 64u) != 0 ||
        json_hex_field(response + header_end, "session_id", session_id, 33u, 32u) != 0 ||
        json_hex_field(response + header_end, "host_session_id", host_session_id, 33u, 32u) != 0) {
        goto cleanup;
    }
    if (create_room) {
        if (json_string_field(response + header_end, "room_code",
                              room_code_out, 7u) != 0 || strlen(room_code_out) != 6u) goto cleanup;
    } else {
        memcpy(room_code_out, join_room_code, 6u);
        room_code_out[6] = '\0';
    }
    parse_room_roster(client, response + header_end);
    result = 0;

cleanup:
    close(fd);
    return result;
}
static uint64_t monotonic_ms(void)
{
    struct timeval now;
    if (gettimeofday(&now, NULL) != 0) return 0;
    return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_usec / 1000u;
}

static int connect_relay_ticket(CbRelayClient *client, const char *host,
                                 unsigned short port, const char *room_code,
                                 const char *ticket, int timeout_ms)
{
    char protocols[112];
    char path[96];
    int length;

    length = snprintf(protocols, sizeof(protocols), "cbash-v1, cbash-ticket.%s", ticket);
    if (length < 0 || (size_t)length >= sizeof(protocols)) return -1;
    length = snprintf(path, sizeof(path), "/v1/rooms/%s/transport", room_code);
    if (length < 0 || (size_t)length >= sizeof(path)) return -1;

    if (cb_ws_connect(&client->websocket, host, port, path, protocols, timeout_ms) != 0) {
        return -1;
    }
    if (strlen(host) >= sizeof(client->host)) {
        cb_ws_close(&client->websocket);
        return -1;
    }
    memcpy(client->room_code, room_code, 6u);
    client->room_code[6] = '\0';
    strcpy(client->host, host);
    client->port = port;
    client->last_ping_ms = monotonic_ms();
    client->connected = 1;
    return 0;
}

int cb_relay_join(CbRelayClient *client, const char *host, unsigned short port,
                  const char *room_code, const char *display_name, int timeout_ms)
{
    char ticket[65];
    char room_code_out[7];
    if (!client || !host || !room_code) return -1;
    memset(client, 0, sizeof(*client));
    client->websocket.fd = -1;

    if (request_room_ticket(client, host, port, room_code, 0, display_name, timeout_ms,
                            ticket, client->session_id, client->host_session_id,
                            room_code_out) != 0) return -1;
    return connect_relay_ticket(client, host, port, room_code_out, ticket, timeout_ms);
}

int cb_relay_host(CbRelayClient *client, const char *host, unsigned short port,
                  const char *display_name, int timeout_ms)
{
    char ticket[65];
    char room_code[7];
    if (!client || !host) return -1;
    memset(client, 0, sizeof(*client));
    client->websocket.fd = -1;

    if (request_room_ticket(client, host, port, NULL, 1, display_name, timeout_ms,
                            ticket, client->session_id, client->host_session_id,
                            room_code) != 0) return -1;
    return connect_relay_ticket(client, host, port, room_code, ticket, timeout_ms);
}

int cb_relay_start_match(CbRelayClient *client, int timeout_ms)
{
    uint8_t packet[CB_RELAY_MAX_PACKET];
    size_t packet_length = 0;
    char control[512];
    uint64_t deadline;
    int result;

    if (!client || !client->connected) return -1;
    if (cb_ws_send_text(&client->websocket, "START", 5u) != 0) return -1;
    if (timeout_ms <= 0) timeout_ms = 5000;
    deadline = monotonic_ms() + (uint64_t)timeout_ms;

    while (monotonic_ms() < deadline) {
        uint64_t remaining = deadline - monotonic_ms();
        int wait_ms = remaining > 1000u ? 1000 : (int)remaining;
        control[0] = '\0';
        result = cb_relay_receive_packet(client, packet, sizeof(packet),
                                         &packet_length, NULL, control,
                                         sizeof(control), wait_ms);
        if (result == 2) {
            if (strcmp(control, "STARTED") == 0) return 0;
            if (strncmp(control, "ERROR:", 6u) == 0) return -1;
        } else if (result < 0) {
            return -1;
        }
    }
    return -1;
}

int cb_relay_send_batch(CbRelayClient *client, const CbRelayPacket *packets, size_t count)
{
    uint8_t *message;
    size_t total = 5u;
    size_t offset = 5u;
    size_t i;
    int result;

    if (!client || !client->connected || !packets ||
        count == 0 || count > CB_RELAY_MAX_BATCH_PACKETS) return -1;

    for (i = 0; i < count; ++i) {
        if (!packets[i].payload || packets[i].length == 0 ||
            packets[i].length > CB_RELAY_MAX_PACKET ||
            total > CB_RELAY_MAX_MESSAGE - 18u - packets[i].length) return -1;
        total += 18u + packets[i].length;
    }

    message = (uint8_t *)malloc(total);
    if (!message) return -1;
    message[0] = 0x43u;
    message[1] = 0x42u;
    message[2] = 0x55u;
    message[3] = 0x31u; /* CBU1 */
    message[4] = (uint8_t)count;

    for (i = 0; i < count; ++i) {
        memcpy(message + offset, packets[i].recipient_id, 16u);
        offset += 16u;
        message[offset++] = (uint8_t)(packets[i].length >> 8);
        message[offset++] = (uint8_t)packets[i].length;
        memcpy(message + offset, packets[i].payload, packets[i].length);
        offset += packets[i].length;
    }

    result = cb_ws_send_binary(&client->websocket, message, total);
    free(message);
    if (result == 0) client->last_ping_ms = monotonic_ms();
    return result;
}

int cb_relay_send_to(CbRelayClient *client, const uint8_t recipient_id[16],
                     const void *packet, size_t length)
{
    CbRelayPacket entry;
    memset(&entry, 0, sizeof(entry));
    if (recipient_id) memcpy(entry.recipient_id, recipient_id, 16u);
    entry.payload = packet;
    entry.length = length;
    return cb_relay_send_batch(client, &entry, 1u);
}

int cb_relay_send_packet(CbRelayClient *client, const void *packet, size_t length)
{
    return cb_relay_send_to(client, NULL, packet, length);
}

int cb_relay_queue_packet(CbRelayBatchQueue *queue, const uint8_t recipient_id[16],
                          const void *packet, size_t length)
{
    CbRelayPacket *entry;
    if (!queue || !packet || length == 0 || length > CB_RELAY_MAX_PACKET ||
        queue->count >= CB_RELAY_MAX_BATCH_PACKETS) return -1;

    {
        size_t i;
        const uint8_t *bytes = (const uint8_t *)packet;
        for (i = 0; i < queue->count; ++i) {
            CbRelayPacket *queued = &queue->packets[i];
            int same_recipient = recipient_id
                ? memcmp(queued->recipient_id, recipient_id, 16u) == 0
                : memcmp(queued->recipient_id, (uint8_t[16]){0}, 16u) == 0;
            /* Lockstep retransmissions contain all still-unacknowledged records.
             * Keep only the newest packet of a given type for each destination. */
            if (same_recipient && queued->length >= 5u && length >= 5u &&
                memcmp(queued->payload, bytes, 5u) == 0) {
                memmove(queue->payloads[i], packet, length);
                queued->payload = queue->payloads[i];
                queued->length = length;
                return 0;
            }
        }
    }

    entry = &queue->packets[queue->count];
    memset(entry->recipient_id, 0, sizeof(entry->recipient_id));
    if (recipient_id) memcpy(entry->recipient_id, recipient_id, 16u);
    memcpy(queue->payloads[queue->count], packet, length);
    entry->payload = queue->payloads[queue->count];
    entry->length = length;
    queue->count++;
    return 0;
}

int cb_relay_batch_flush(CbRelayClient *client, CbRelayBatchQueue *queue)
{
    int result;
    if (!client || !queue) return -1;
    if (queue->count == 0) {
        queue->frame_count = 0;
        return 0;
    }
    result = cb_relay_send_batch(client, queue->packets, queue->count);
    if (result == 0) {
        queue->count = 0;
        queue->frame_count = 0;
    }
    return result;
}

int cb_relay_batch_tick(CbRelayClient *client, CbRelayBatchQueue *queue,
                        unsigned int frames_per_batch)
{
    if (!client || !queue || !client->connected) return -1;
    if (frames_per_batch == 0) frames_per_batch = 6u;
    if (queue->frame_count < 0xffffffffu) queue->frame_count++;
    if (queue->count >= CB_RELAY_MAX_BATCH_PACKETS ||
        queue->frame_count >= frames_per_batch) {
        return cb_relay_batch_flush(client, queue);
    }
    return 0;
}

int cb_relay_receive_packet(CbRelayClient *client, void *packet, size_t capacity,
                            size_t *length, uint8_t sender_id[16],
                            char *control, size_t control_capacity,
                            int timeout_ms)
{
    uint8_t frame[CB_WS_MAX_FRAME + 1u];
    size_t frame_length = 0;
    int opcode = 0;
    int result;
    uint64_t now;

    if (!client || !client->connected || !length) return -1;
    now = monotonic_ms();
    if (now && now - client->last_ping_ms >= 20000u) {
        if (cb_ws_send_text(&client->websocket, "PING", 4u) != 0) return -1;
        client->last_ping_ms = now;
    }

    result = cb_ws_receive(&client->websocket, frame, CB_WS_MAX_FRAME,
                           &frame_length, &opcode, timeout_ms);
    if (result <= 0) return result;
    client->last_ping_ms = monotonic_ms();
    frame[frame_length] = '\0';

    if (opcode == CB_WS_OPCODE_TEXT) {
        parse_room_roster(client, (const char *)frame);
        if (control && control_capacity) {
            size_t copy = frame_length < control_capacity - 1u
                ? frame_length : control_capacity - 1u;
            memcpy(control, frame, copy);
            control[copy] = '\0';
        }
        if (frame_length == 4u && memcmp(frame, "PONG", 4u) == 0) return 0;
        return 2;
    }

    if (opcode != CB_WS_OPCODE_BINARY || frame_length < CB_RELAY_ENVELOPE_SIZE ||
        memcmp(frame, "CBR1", 4u) != 0) return -1;

    {
        size_t payload_length = ((size_t)frame[20] << 8) | frame[21];
        if (payload_length == 0 || payload_length > CB_RELAY_MAX_PACKET ||
            frame_length != CB_RELAY_ENVELOPE_SIZE + payload_length ||
            payload_length > capacity || !packet) return -1;
        if (sender_id) memcpy(sender_id, frame + 4, 16u);
        memcpy(packet, frame + CB_RELAY_ENVELOPE_SIZE, payload_length);
        *length = payload_length;
    }
    return 1;
}

int cb_relay_session_address(const uint8_t session_id[16],
                             uint32_t *address_network, unsigned short *port_host)
{
    uint8_t octets[4];
    unsigned int port_value;
    if (!session_id || !address_network || !port_host) return -1;

    /* Reserve 10/8 for virtual relay peers. The first five random session
     * bytes provide a stable 24-bit address plus a 16-bit UDP-style port. */
    octets[0] = 10u;
    octets[1] = session_id[0];
    octets[2] = session_id[1];
    octets[3] = session_id[2];
    memcpy(address_network, octets, sizeof(octets));
    port_value = ((unsigned int)session_id[3] << 8) | session_id[4];
    *port_host = (unsigned short)(port_value == 0u ? 1u : port_value);
    return 0;
}

int cb_relay_host_address(const CbRelayClient *client, char *address,
                          size_t address_capacity, unsigned short *port)
{
    uint8_t session_id[16];
    uint32_t address_network;
    struct in_addr ip;

    if (!client || !client->host_session_id[0] || !address ||
        address_capacity == 0 || !port ||
        parse_session_id(client->host_session_id, session_id) != 0 ||
        cb_relay_session_address(session_id, &address_network, port) != 0) return -1;

    ip.s_addr = address_network;
    if (!inet_ntop(AF_INET, &ip, address, (socklen_t)address_capacity)) return -1;
    return 0;
}

int cb_relay_peer_for_address(const CbRelayClient *client, uint32_t address_network,
                              unsigned short port_host, uint8_t session_id[16])
{
    size_t i;
    if (!client || !session_id) return -1;
    for (i = 0; i < CB_RELAY_MAX_PEERS; ++i) {
        const CbRelayPeer *peer = &client->peers[i];
        if (peer->valid && peer->address_network == address_network &&
            peer->port_host == port_host) {
            memcpy(session_id, peer->session_id, 16u);
            return 0;
        }
    }
    return -1;
}

void cb_relay_leave(CbRelayClient *client)
{
    if (!client) return;
    if (client->websocket.open) cb_ws_close(&client->websocket);
    client->connected = 0;
    client->room_code[0] = '\0';
    client->session_id[0] = '\0';
    client->host_session_id[0] = '\0';
    client->host[0] = '\0';
    client->port = 0;
}
