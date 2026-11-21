#include <netdb.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define CB_DNS_PACKET_MAX 512u
#define CB_DNS_TIMEOUT_MS 1500
#define CB_DNS_TYPE_A 1u
#define CB_DNS_CLASS_IN 1u

static uint16_t read_u16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static int skip_dns_name(const uint8_t *packet, size_t packet_length,
                         size_t *offset)
{
    size_t position = *offset;
    size_t consumed = 0;
    size_t hops = 0;
    int jumped = 0;

    while (position < packet_length && hops++ < 32u) {
        uint8_t label = packet[position];

        if (label == 0) {
            if (!jumped) consumed++;
            *offset += consumed;
            return 0;
        }

        if ((label & 0xc0u) == 0xc0u) {
            size_t pointer;
            if (position + 1u >= packet_length) return -1;
            pointer = ((size_t)(label & 0x3fu) << 8) | packet[position + 1u];
            if (pointer >= packet_length) return -1;
            if (!jumped) consumed += 2u;
            position = pointer;
            jumped = 1;
            continue;
        }

        if ((label & 0xc0u) != 0 || label > 63u ||
            position + 1u + label > packet_length) return -1;
        if (!jumped) consumed += 1u + label;
        position += 1u + label;
    }

    return -1;
}

static int build_dns_query(const char *hostname, uint8_t packet[CB_DNS_PACKET_MAX],
                           size_t *packet_length, uint16_t identifier)
{
    const char *label = hostname;
    size_t offset = 12u;
    size_t hostname_length;

    if (!hostname || !*hostname || !packet || !packet_length) return -1;
    hostname_length = strlen(hostname);
    if (hostname_length > 253u) return -1;

    memset(packet, 0, CB_DNS_PACKET_MAX);
    packet[0] = (uint8_t)(identifier >> 8);
    packet[1] = (uint8_t)identifier;
    packet[2] = 0x01u; /* recursion desired */
    packet[5] = 0x01u; /* one question */

    while (*label) {
        const char *end = strchr(label, '.');
        size_t length = end ? (size_t)(end - label) : strlen(label);
        size_t i;

        if (length == 0 || length > 63u || offset + length + 1u >= CB_DNS_PACKET_MAX) {
            return -1;
        }
        packet[offset++] = (uint8_t)length;
        for (i = 0; i < length; ++i) {
            char c = label[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            packet[offset++] = (uint8_t)c;
        }

        if (!end) break;
        label = end + 1;
        if (!*label) break; /* allow a trailing dot */
    }

    packet[offset++] = 0;
    packet[offset++] = 0;
    packet[offset++] = CB_DNS_TYPE_A;
    packet[offset++] = 0;
    packet[offset++] = CB_DNS_CLASS_IN;
    *packet_length = offset;
    return 0;
}

static int parse_dns_a_response(const uint8_t *packet, size_t packet_length,
                                uint16_t identifier, struct in_addr *address)
{
    size_t offset = 12u;
    uint16_t flags;
    uint16_t questions;
    uint16_t answers;
    unsigned int i;

    if (!packet || packet_length < 12u || !address) return -1;
    if (read_u16(packet) != identifier) return -1;

    flags = read_u16(packet + 2u);
    if ((flags & 0x8000u) == 0 || (flags & 0x000fu) != 0 ||
        (flags & 0x0200u) != 0) return -1;

    questions = read_u16(packet + 4u);
    answers = read_u16(packet + 6u);
    if (answers == 0 || questions > 8u || answers > 64u) return -1;

    for (i = 0; i < questions; ++i) {
        if (skip_dns_name(packet, packet_length, &offset) != 0 ||
            offset + 4u > packet_length) return -1;
        offset += 4u;
    }

    for (i = 0; i < answers; ++i) {
        uint16_t type;
        uint16_t record_class;
        uint16_t data_length;

        if (skip_dns_name(packet, packet_length, &offset) != 0 ||
            offset + 10u > packet_length) return -1;
        type = read_u16(packet + offset);
        record_class = read_u16(packet + offset + 2u);
        data_length = read_u16(packet + offset + 8u);
        offset += 10u;
        if (offset + data_length > packet_length) return -1;

        if (type == CB_DNS_TYPE_A && record_class == CB_DNS_CLASS_IN &&
            data_length == 4u) {
            memcpy(&address->s_addr, packet + offset, 4u);
            return 0;
        }
        offset += data_length;
    }

    return -1;
}

static int resolve_ipv4_dns(const char *hostname, struct in_addr *address)
{
    static const char *dns_servers[] = { "1.1.1.1", "8.8.8.8" };
    uint8_t query[CB_DNS_PACKET_MAX];
    uint8_t response[CB_DNS_PACKET_MAX];
    size_t query_length = 0;
    uint16_t identifier = (uint16_t)((uint32_t)time(NULL) ^
                                     (uint32_t)getpid() ^
                                     (uint32_t)(uintptr_t)hostname);
    size_t server_index;

    if (build_dns_query(hostname, query, &query_length, identifier) != 0) return -1;

    for (server_index = 0;
         server_index < sizeof(dns_servers) / sizeof(dns_servers[0]);
         ++server_index) {
        struct sockaddr_in server_address;
        struct timeval timeout;
        int fd;
        ssize_t received;

        memset(&server_address, 0, sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(53u);
        if (inet_pton(AF_INET, dns_servers[server_index],
                      &server_address.sin_addr) != 1) continue;

        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd < 0) continue;

        timeout.tv_sec = CB_DNS_TIMEOUT_MS / 1000;
        timeout.tv_usec = (CB_DNS_TIMEOUT_MS % 1000) * 1000;
        (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        if (sendto(fd, query, query_length, 0,
                   (const struct sockaddr *)&server_address,
                   sizeof(server_address)) < 0) {
            close(fd);
            continue;
        }

        received = recvfrom(fd, response, sizeof(response), 0, NULL, NULL);
        close(fd);
        if (received > 0 &&
            parse_dns_a_response(response, (size_t)received, identifier, address) == 0) {
            return 0;
        }
    }

    return -1;
}

int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **result)
{
    struct addrinfo *entry;
    struct sockaddr_in *address;
    unsigned long port = 0;
    char *end = NULL;

    if (!result) return EAI_FAIL;
    *result = NULL;
    if (!node && !(hints && (hints->ai_flags & AI_PASSIVE))) return EAI_NONAME;

    if (service && service[0]) {
        port = strtoul(service, &end, 10);
        if (!end || *end != '\0' || port > 65535UL) return EAI_SERVICE;
    }

    entry = (struct addrinfo *)calloc(1, sizeof(*entry));
    address = (struct sockaddr_in *)calloc(1, sizeof(*address));
    if (!entry || !address) {
        free(entry);
        free(address);
        return EAI_FAIL;
    }

    address->sin_family = AF_INET;
    address->sin_port = htons((uint16_t)port);
    if (!node) {
        address->sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, node, &address->sin_addr) != 1) {
        if (resolve_ipv4_dns(node, &address->sin_addr) != 0) {
            free(address);
            free(entry);
            return EAI_NONAME;
        }
    }

    entry->ai_flags = hints ? hints->ai_flags : 0;
    entry->ai_family = AF_INET;
    entry->ai_socktype = hints ? hints->ai_socktype : SOCK_DGRAM;
    entry->ai_protocol = hints ? hints->ai_protocol : 0;
    entry->ai_addrlen = sizeof(*address);
    entry->ai_addr = (struct sockaddr *)address;
    entry->ai_canonname = NULL;
    entry->ai_next = NULL;
    *result = entry;
    return 0;
}

void freeaddrinfo(struct addrinfo *result)
{
    while (result) {
        struct addrinfo *next = result->ai_next;
        free(result->ai_addr);
        free(result->ai_canonname);
        free(result);
        result = next;
    }
}

int gethostname(char *name, size_t length)
{
    static const char host[] = "PS3";
    if (!name || length == 0) return -1;
    if (length < sizeof(host)) {
        memcpy(name, host, length - 1);
        name[length - 1] = '\0';
        return -1;
    }
    memcpy(name, host, sizeof(host));
    return 0;
}
