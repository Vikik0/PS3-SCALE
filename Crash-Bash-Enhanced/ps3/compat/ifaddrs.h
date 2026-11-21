#ifndef CRASH_BASH_PS3_IFADDRS_H
#define CRASH_BASH_PS3_IFADDRS_H

#include <sys/socket.h>

struct ifaddrs {
    struct ifaddrs *ifa_next;
    char *ifa_name;
    unsigned int ifa_flags;
    struct sockaddr *ifa_addr;
    struct sockaddr *ifa_netmask;
    struct sockaddr *ifa_broadaddr;
    struct sockaddr *ifa_dstaddr;
    void *ifa_data;
};

int getifaddrs(struct ifaddrs **ifap);
void freeifaddrs(struct ifaddrs *ifap);

#endif
