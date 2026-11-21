#ifndef CRASH_BASH_PS3_NETDB_H
#define CRASH_BASH_PS3_NETDB_H

#include_next <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PS3DK exposes the legacy host database but not getaddrinfo. The online
 * adapter only needs IPv4 UDP/TCP resolution, implemented in netdb_compat.c. */
#ifndef EAI_FAIL
#define EAI_FAIL (-4)
#endif
#ifndef EAI_FAMILY
#define EAI_FAMILY (-6)
#endif
#ifndef EAI_NONAME
#define EAI_NONAME (-2)
#endif
#ifndef EAI_SERVICE
#define EAI_SERVICE (-8)
#endif

int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **result);
void freeaddrinfo(struct addrinfo *result);

#ifdef __cplusplus
}
#endif

#endif
