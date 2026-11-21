#include <netdb.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **result)
{
    struct addrinfo *entry;
    struct sockaddr_in *address;
    struct hostent *host = NULL;
    unsigned long port = 0;
    char *end = NULL;

    if (!result) return EAI_FAIL;
    *result = NULL;
    if (!node && !(hints && (hints->ai_flags & AI_PASSIVE))) return EAI_NONAME;

    if (service && service[0]) {
        port = strtoul(service, &end, 10);
        if (!end || *end != '\0' || port > 65535UL) {
            struct servent *named = getservbyname(service,
                hints && hints->ai_socktype == SOCK_STREAM ? "tcp" : "udp");
            if (!named) return EAI_SERVICE;
            port = (unsigned long)ntohs((uint16_t)named->s_port);
        }
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
        host = gethostbyname(node);
        if (!host || host->h_addrtype != AF_INET || host->h_length < 4 ||
            !host->h_addr_list || !host->h_addr_list[0]) {
            free(address);
            free(entry);
            return EAI_NONAME;
        }
        memcpy(&address->sin_addr, host->h_addr_list[0], 4);
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
