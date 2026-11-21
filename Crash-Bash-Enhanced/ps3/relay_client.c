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

static int request_guest_ticket(const char *host, unsigned short port,
                                const char *room_code, const char *display_name,
                                int timeout_ms, char ticket[65], char session_id[33], char host_session_id[33])
{
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    struct timeval timeout;
    char service[8];
    char host_header[288];
    char safe_name[17];
    char request_body[96];
    char request[1024];
    char response[CB_HTTP_RESPONSE_MAX + 1];
    size_t received = 0;
    size_t header_end = 0;
    size_t body_length = 0;
    int have_content_length = 0;
    int fd = -1;
    int request_length;
    int result = -1;

    if (!host || !*host || !room_code || strlen(room_code) != 6u || port == 0) return -1;
    clean_display_name(display_name, safe_name);

    for (size_t i = 0; i < 6u; ++i) {
        char c = room_code[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return -1;
    }

    snprintf(request_body, sizeof(request_body), "{\"display_name\":\"%s\"}", safe_name);
    if (port == 80u) {
        if (snprintf(host_header, sizeof(host_header), "%s", host) >= (int)sizeof(host_header)) return -1;
    } else {
        if (snprintf(host_header, sizeof(host_header), "%s:%u", host,
                     (unsigned int)port) >= (int)sizeof(host_header)) return -1;
    }

    request_length = snprintf(request, sizeof(request),
        "POST /v1/rooms/%s/join-guest HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "Content-Type: application/json\r\n"
        "Accept: application/json\r\n"
        "Content-Length: %lu\r\n"
        "\r\n%s",
        room_code, host_header, (unsigned long)strlen(request_body), request_body);
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

int cb_relay_join(CbRelayClient *client, const char *host, unsigned short port,
                  const char *room_code, const char *display_name, int timeout_ms)
{
    char ticket[65];
    char protocols[112];
    char path[96];
    int length;

    if (!client || !host || !room_code || strlen(room_code) != 6u) return -1;
    memset(client, 0, sizeof(*client));
    client->websocket.fd = -1;

    if (request_guest_ticket(host, port, room_code, display_name,
                             timeout_ms, ticket, client->session_id, client->host_session_id) != 0) {
        return -1;
    }

    length = snprintf(protocols, sizeof(protocols), "cbash-v1, cbash-ticket.%s", ticket);
    if (length < 0 || (size_t)length >= sizeof(protocols)) return -1;
    length = snprintf(path, sizeof(path), "/v1/rooms/%s/transport", room_code);
    if (length < 0 || (size_t)length >= sizeof(path)) return -1;

    if (cb_ws_connect(&client->websocket, host, port, path, protocols, timeout_ms) != 0) {
        return -1;
    }

    memcpy(client->room_code, room_code, 6u);
    client->room_code[6] = '\0';
    if (strlen(host) >= sizeof(client->host)) {
        cb_ws_close(&client->websocket);
        return -1;
    }
    strcpy(client->host, host);
    client->port = port;
    client->last_ping_ms = monotonic_ms();
    client->connected = 1;
    return 0;
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

int cb_relay_receive_packet(CbRelayClient *client, void *packet, size_t capacity,
                            size_t *length, uint8_t sender_id[16],
                            char *control, size_t control_capacity,
                            int timeout_ms)
{
    uint8_t frame[CB_RELAY_ENVELOPE_SIZE + CB_RELAY_MAX_PACKET];
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

    result = cb_ws_receive(&client->websocket, frame, sizeof(frame),
                           &frame_length, &opcode, timeout_ms);
    if (result <= 0) return result;

    if (opcode == CB_WS_OPCODE_TEXT) {
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

int cb_relay_host_address(const CbRelayClient *client, char *address,
                          size_t address_capacity, unsigned short *port)
{
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int port_value;

    if (!client || !client->host_session_id[0] || !address ||
        address_capacity == 0 || !port) return -1;

    {
        unsigned int bytes[5];
        size_t i;
        for (i = 0; i < 5u; ++i) {
            char pair[3];
            char *end;
            pair[0] = client->host_session_id[i * 2u];
            pair[1] = client->host_session_id[i * 2u + 1u];
            pair[2] = '\0';
            bytes[i] = (unsigned int)strtoul(pair, &end, 16);
            if (*end != '\0') return -1;
        }
        a = 1u + (bytes[0] % 250u);
        b = 1u + (bytes[1] % 250u);
        c = 1u + (bytes[2] % 250u);
        port_value = 20000u + (((bytes[3] << 8) | bytes[4]) % 40000u);
    }

    if (snprintf(address, address_capacity, "10.%u.%u.%u", a, b, c) >=
        (int)address_capacity) return -1;
    *port = (unsigned short)port_value;
    return 0;
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
