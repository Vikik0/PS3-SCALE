#include "ws_client.h"

#include <errno.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define WS_HEADER_LIMIT 4096

static uint32_t rotate_left(uint32_t value, unsigned int bits)
{
    return (value << bits) | (value >> (32u - bits));
}

static void sha1_digest(const uint8_t *data, size_t length, uint8_t output[20])
{
    uint8_t padded[128];
    uint32_t h0 = 0x67452301u;
    uint32_t h1 = 0xefcdab89u;
    uint32_t h2 = 0x98badcfeu;
    uint32_t h3 = 0x10325476u;
    uint32_t h4 = 0xc3d2e1f0u;
    uint64_t bit_length = (uint64_t)length * 8u;
    size_t total = ((length + 9u + 63u) / 64u) * 64u;
    size_t offset;

    if (total > sizeof(padded)) {
        memset(output, 0, 20);
        return;
    }

    memset(padded, 0, sizeof(padded));
    memcpy(padded, data, length);
    padded[length] = 0x80u;
    padded[total - 8u] = (uint8_t)(bit_length >> 56);
    padded[total - 7u] = (uint8_t)(bit_length >> 48);
    padded[total - 6u] = (uint8_t)(bit_length >> 40);
    padded[total - 5u] = (uint8_t)(bit_length >> 32);
    padded[total - 4u] = (uint8_t)(bit_length >> 24);
    padded[total - 3u] = (uint8_t)(bit_length >> 16);
    padded[total - 2u] = (uint8_t)(bit_length >> 8);
    padded[total - 1u] = (uint8_t)bit_length;

    for (offset = 0; offset < total; offset += 64u) {
        uint32_t words[80];
        uint32_t a, b, c, d, e;
        unsigned int i;

        for (i = 0; i < 16u; ++i) {
            const uint8_t *p = padded + offset + (size_t)i * 4u;
            words[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                       ((uint32_t)p[2] << 8) | (uint32_t)p[3];
        }
        for (i = 16u; i < 80u; ++i) {
            words[i] = rotate_left(words[i - 3u] ^ words[i - 8u] ^
                                   words[i - 14u] ^ words[i - 16u], 1u);
        }

        a = h0;
        b = h1;
        c = h2;
        d = h3;
        e = h4;

        for (i = 0; i < 80u; ++i) {
            uint32_t f;
            uint32_t k;
            uint32_t temp;

            if (i < 20u) {
                f = (b & c) | ((~b) & d);
                k = 0x5a827999u;
            } else if (i < 40u) {
                f = b ^ c ^ d;
                k = 0x6ed9eba1u;
            } else if (i < 60u) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8f1bbcdcu;
            } else {
                f = b ^ c ^ d;
                k = 0xca62c1d6u;
            }

            temp = rotate_left(a, 5u) + f + e + k + words[i];
            e = d;
            d = c;
            c = rotate_left(b, 30u);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
    }

    output[0] = (uint8_t)(h0 >> 24);
    output[1] = (uint8_t)(h0 >> 16);
    output[2] = (uint8_t)(h0 >> 8);
    output[3] = (uint8_t)h0;
    output[4] = (uint8_t)(h1 >> 24);
    output[5] = (uint8_t)(h1 >> 16);
    output[6] = (uint8_t)(h1 >> 8);
    output[7] = (uint8_t)h1;
    output[8] = (uint8_t)(h2 >> 24);
    output[9] = (uint8_t)(h2 >> 16);
    output[10] = (uint8_t)(h2 >> 8);
    output[11] = (uint8_t)h2;
    output[12] = (uint8_t)(h3 >> 24);
    output[13] = (uint8_t)(h3 >> 16);
    output[14] = (uint8_t)(h3 >> 8);
    output[15] = (uint8_t)h3;
    output[16] = (uint8_t)(h4 >> 24);
    output[17] = (uint8_t)(h4 >> 16);
    output[18] = (uint8_t)(h4 >> 8);
    output[19] = (uint8_t)h4;
}

static int base64_encode(const uint8_t *input, size_t length,
                         char *output, size_t output_capacity)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t required = ((length + 2u) / 3u) * 4u;
    size_t in = 0;
    size_t out = 0;

    if (output_capacity <= required) return -1;

    while (in < length) {
        uint32_t value = (uint32_t)input[in++] << 16;
        int have_second = in < length;
        int have_third;

        if (have_second) value |= (uint32_t)input[in++] << 8;
        have_third = in < length;
        if (have_third) value |= input[in++];

        output[out++] = alphabet[(value >> 18) & 63u];
        output[out++] = alphabet[(value >> 12) & 63u];
        output[out++] = have_second ? alphabet[(value >> 6) & 63u] : '=';
        output[out++] = have_third ? alphabet[value & 63u] : '=';
    }

    output[out] = '\0';
    return (int)out;
}

static uint32_t random_next(CbWsClient *client)
{
    uint32_t value = client->rng_state;
    if (value == 0) value = 0x9e3779b9u;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    client->rng_state = value;
    return value;
}

static int wait_readable(int fd, int timeout_ms)
{
    fd_set read_set;
    struct timeval timeout;
    struct timeval *timeout_pointer = NULL;
    int result;

    FD_ZERO(&read_set);
    FD_SET(fd, &read_set);

    if (timeout_ms >= 0) {
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        timeout_pointer = &timeout;
    }

    result = select(fd + 1, &read_set, NULL, NULL, timeout_pointer);
    if (result > 0 && FD_ISSET(fd, &read_set)) return 1;
    if (result == 0) return 0;
    return -1;
}

static int send_all(int fd, const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    size_t sent = 0;

    while (sent < length) {
        ssize_t count = send(fd, bytes + sent, length - sent, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return -1;
        sent += (size_t)count;
    }
    return 0;
}

static int read_exact(CbWsClient *client, uint8_t *output,
                      size_t length, int timeout_ms)
{
    size_t available;

    for (;;) {
        available = client->rx_end - client->rx_start;
        if (available >= length) break;

        if (client->rx_start > 0) {
            memmove(client->rx_buffer,
                    client->rx_buffer + client->rx_start, available);
            client->rx_start = 0;
            client->rx_end = available;
        }

        if (client->rx_end >= sizeof(client->rx_buffer)) return -1;
        {
            int ready = wait_readable(client->fd, timeout_ms);
            ssize_t received;
            if (ready == 0) return available == 0 ? 0 : -1;
            if (ready < 0) return -1;
            received = recv(client->fd, client->rx_buffer + client->rx_end,
                            sizeof(client->rx_buffer) - client->rx_end, 0);
            if (received <= 0) return -1;
            client->rx_end += (size_t)received;
        }
    }

    memcpy(output, client->rx_buffer + client->rx_start, length);
    client->rx_start += length;
    if (client->rx_start == client->rx_end) {
        client->rx_start = 0;
        client->rx_end = 0;
    }
    return 1;
}

static int read_http_headers(CbWsClient *client, size_t *header_length,
                             int timeout_ms)
{
    for (;;) {
        size_t i;
        size_t previous_end = client->rx_end;
        if (client->rx_end >= WS_HEADER_LIMIT) return -1;

        {
            int ready = wait_readable(client->fd, timeout_ms);
            ssize_t received;
            if (ready <= 0) return -1;
            received = recv(client->fd, client->rx_buffer + client->rx_end,
                            sizeof(client->rx_buffer) - client->rx_end, 0);
            if (received <= 0) return -1;
            client->rx_end += (size_t)received;
        }

        if (client->rx_end < 4u) continue;
        i = previous_end > 3u ? previous_end - 3u : 0u;
        for (; i + 4u <= client->rx_end; ++i) {
            if (client->rx_buffer[i] == '\r' &&
                client->rx_buffer[i + 1u] == '\n' &&
                client->rx_buffer[i + 2u] == '\r' &&
                client->rx_buffer[i + 3u] == '\n') {
                *header_length = i + 4u;
                return 0;
            }
        }
    }
}

static int equal_ci(const char *left, const char *right)
{
    while (*left && *right) {
        char a = *left++;
        char b = *right++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return *left == '\0' && *right == '\0';
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

static int header_value(const char *headers, const char *name,
                        char *output, size_t output_capacity)
{
    const char *line = strstr(headers, "\r\n");
    size_t name_length = strlen(name);
    if (!line) return 0;
    line += 2;

    while (*line) {
        const char *end = strstr(line, "\r\n");
        const char *colon;
        const char *value;
        size_t length;

        if (!end || end == line) break;
        colon = (const char *)memchr(line, ':', (size_t)(end - line));
        if (colon && (size_t)(colon - line) == name_length &&
            equal_ci_n(line, name, name_length)) {
            value = colon + 1;
            while (value < end && (*value == ' ' || *value == '\t')) value++;
            length = (size_t)(end - value);
            while (length > 0 && (value[length - 1u] == ' ' ||
                                  value[length - 1u] == '\t')) length--;
            if (length >= output_capacity) return 0;
            memcpy(output, value, length);
            output[length] = '\0';
            return 1;
        }
        line = end + 2;
    }
    return 0;
}

static int contains_ci(const char *text, const char *needle)
{
    size_t needle_length = strlen(needle);
    const char *p;
    for (p = text; *p; ++p) {
        if (strlen(p) >= needle_length && equal_ci_n(p, needle, needle_length)) return 1;
    }
    return 0;
}

static int websocket_send_frame(CbWsClient *client, uint8_t opcode,
                                const void *payload, size_t length)
{
    uint8_t frame[CB_WS_MAX_FRAME + 14];
    uint8_t mask[4];
    size_t header_length;
    size_t i;
    uint8_t *bytes = frame;

    if (!client || !client->open || length > CB_WS_MAX_FRAME) return -1;
    if (length > 125u && opcode >= 8u) return -1;

    bytes[0] = (uint8_t)(0x80u | (opcode & 0x0fu));
    if (length <= 125u) {
        bytes[1] = (uint8_t)(0x80u | (uint8_t)length);
        header_length = 2u;
    } else if (length <= 65535u) {
        bytes[1] = 0x80u | 126u;
        bytes[2] = (uint8_t)(length >> 8);
        bytes[3] = (uint8_t)length;
        header_length = 4u;
    } else {
        return -1;
    }

    for (i = 0; i < 4u; ++i) mask[i] = (uint8_t)random_next(client);
    memcpy(bytes + header_length, mask, sizeof(mask));
    header_length += sizeof(mask);

    for (i = 0; i < length; ++i) {
        bytes[header_length + i] = ((const uint8_t *)payload)[i] ^ mask[i & 3u];
    }

    return send_all(client->fd, frame, header_length + length);
}

int cb_ws_connect(CbWsClient *client, const char *host, unsigned short port,
                  const char *path, const char *subprotocols, int timeout_ms)
{
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    char service[8];
    char host_header[256];
    char key[32];
    char expected_accept[64];
    char request[2048];
    char response_header[WS_HEADER_LIMIT + 1];
    char accept_header[128];
    char protocol_header[128];
    char upgrade_header[64];
    char connection_header[128];
    uint8_t key_bytes[16];
    uint8_t digest[20];
    char key_material[128];
    size_t response_length = 0;
    size_t key_material_length;
    size_t i;
    int request_length;
    int fd = -1;
    int result = -1;

    if (!client || !host || !*host || !path || path[0] != '/' ||
        !subprotocols || !*subprotocols || port == 0) return -1;

    memset(client, 0, sizeof(*client));
    client->fd = -1;
    client->rng_state = (uint32_t)time(NULL) ^ (uint32_t)getpid() ^
                        (uint32_t)(uintptr_t)client;
    if (client->rng_state == 0) client->rng_state = 0x6d2b79f5u;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof(service), "%u", (unsigned int)port);
    if (getaddrinfo(host, service, &hints, &addresses) != 0) return -1;

    for (address = addresses; address; address = address->ai_next) {
        fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, address->ai_addr, (socklen_t)address->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(addresses);
    if (fd < 0) return -1;

    client->fd = fd;

    for (i = 0; i < sizeof(key_bytes); ++i) {
        key_bytes[i] = (uint8_t)random_next(client);
    }
    if (base64_encode(key_bytes, sizeof(key_bytes), key, sizeof(key)) < 0) goto cleanup;

    memcpy(key_material, key, strlen(key));
    memcpy(key_material + strlen(key), WS_GUID, strlen(WS_GUID));
    key_material_length = strlen(key) + strlen(WS_GUID);
    sha1_digest((const uint8_t *)key_material, key_material_length, digest);
    if (base64_encode(digest, sizeof(digest), expected_accept, sizeof(expected_accept)) < 0) goto cleanup;

    if (port == 80u) {
        if (snprintf(host_header, sizeof(host_header), "%s", host) >= (int)sizeof(host_header)) goto cleanup;
    } else {
        if (snprintf(host_header, sizeof(host_header), "%s:%u", host,
                     (unsigned int)port) >= (int)sizeof(host_header)) goto cleanup;
    }

    request_length = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Sec-WebSocket-Protocol: %s\r\n"
        "Origin: http://%s\r\n"
        "User-Agent: CrashBash-PS3/0.1\r\n"
        "\r\n",
        path, host_header, key, subprotocols, host);
    if (request_length < 0 || (size_t)request_length >= sizeof(request)) goto cleanup;
    if (send_all(fd, request, (size_t)request_length) != 0) goto cleanup;

    if (read_http_headers(client, &response_length, timeout_ms) != 0 ||
        response_length >= sizeof(response_header)) goto cleanup;
    memcpy(response_header, client->rx_buffer, response_length);
    response_header[response_length] = '\0';

    if (strncmp(response_header, "HTTP/1.1 101", 12) != 0 &&
        strncmp(response_header, "HTTP/1.0 101", 12) != 0) goto cleanup;
    if (!header_value(response_header, "Sec-WebSocket-Accept",
                      accept_header, sizeof(accept_header)) ||
        strcmp(accept_header, expected_accept) != 0) goto cleanup;
    if (!header_value(response_header, "Sec-WebSocket-Protocol",
                      protocol_header, sizeof(protocol_header)) ||
        strcmp(protocol_header, "cbash-v1") != 0) goto cleanup;
    if (!header_value(response_header, "Upgrade", upgrade_header, sizeof(upgrade_header)) ||
        !equal_ci(upgrade_header, "websocket")) goto cleanup;
    if (!header_value(response_header, "Connection",
                      connection_header, sizeof(connection_header)) ||
        !contains_ci(connection_header, "upgrade")) goto cleanup;

    client->rx_start = response_length;
    client->open = 1;
    return 0;

cleanup:
    if (fd >= 0) close(fd);
    client->fd = -1;
    client->open = 0;
    client->rx_start = 0;
    client->rx_end = 0;
    return result;
}

int cb_ws_send_binary(CbWsClient *client, const void *payload, size_t length)
{
    return websocket_send_frame(client, 2u, payload, length);
}

int cb_ws_send_text(CbWsClient *client, const char *payload, size_t length)
{
    return websocket_send_frame(client, 1u, payload, length);
}

int cb_ws_receive(CbWsClient *client, void *payload, size_t capacity,
                  size_t *length, int *opcode, int timeout_ms)
{
    uint8_t header[2];
    uint8_t extended[8];
    uint8_t mask[4];
    uint64_t payload_length;
    uint8_t frame_opcode;
    int masked;
    int result;
    size_t i;

    if (!client || !client->open || !payload || !length || !opcode) return -1;
    result = read_exact(client, header, sizeof(header), timeout_ms);
    if (result <= 0) return result;

    if ((header[0] & 0x70u) != 0 || (header[0] & 0x80u) == 0) return -1;
    frame_opcode = (uint8_t)(header[0] & 0x0fu);
    masked = (header[1] & 0x80u) != 0;
    payload_length = (uint64_t)(header[1] & 0x7fu);

    if (payload_length == 126u) {
        result = read_exact(client, extended, 2u, timeout_ms);
        if (result != 1) return -1;
        payload_length = ((uint64_t)extended[0] << 8) | extended[1];
    } else if (payload_length == 127u) {
        result = read_exact(client, extended, 8u, timeout_ms);
        if (result != 1) return -1;
        payload_length = 0;
        for (i = 0; i < 8u; ++i) payload_length = (payload_length << 8) | extended[i];
    }

    if (payload_length > capacity || payload_length > CB_WS_MAX_FRAME) return -1;
    if (frame_opcode >= 8u && payload_length > 125u) return -1;

    if (masked) {
        result = read_exact(client, mask, sizeof(mask), timeout_ms);
        if (result != 1) return -1;
    }

    result = read_exact(client, (uint8_t *)payload, (size_t)payload_length, timeout_ms);
    if (result != 1) return -1;

    if (masked) {
        for (i = 0; i < (size_t)payload_length; ++i) {
            ((uint8_t *)payload)[i] ^= mask[i & 3u];
        }
    }

    *length = (size_t)payload_length;
    *opcode = (int)frame_opcode;

    if (frame_opcode == 9u) {
        (void)websocket_send_frame(client, 10u, payload, (size_t)payload_length);
        return 0;
    }
    if (frame_opcode == 10u) return 0;
    if (frame_opcode == 8u) {
        (void)websocket_send_frame(client, 8u, payload, (size_t)payload_length);
        shutdown(client->fd, SHUT_RDWR);
        close(client->fd);
        client->fd = -1;
        client->open = 0;
        return -1;
    }
    if (frame_opcode != 1u && frame_opcode != 2u) return -1;

    return 1;
}

void cb_ws_close(CbWsClient *client)
{
    uint8_t close_payload[2] = { 0x03u, 0xe8u };
    if (!client || !client->open) return;
    (void)websocket_send_frame(client, 8u, close_payload, sizeof(close_payload));
    (void)shutdown(client->fd, SHUT_RDWR);
    close(client->fd);
    client->fd = -1;
    client->open = 0;
    client->rx_start = 0;
    client->rx_end = 0;
}
