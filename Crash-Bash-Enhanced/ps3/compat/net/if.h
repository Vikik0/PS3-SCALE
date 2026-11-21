#ifndef CRASH_BASH_PS3_NET_IF_H
#define CRASH_BASH_PS3_NET_IF_H

/* Minimal interface flags used by the PS3 net adapter's synthetic ifaddrs. */
#ifndef IFF_UP
#define IFF_UP 0x1
#endif
#ifndef IFF_BROADCAST
#define IFF_BROADCAST 0x2
#endif
#ifndef IFF_LOOPBACK
#define IFF_LOOPBACK 0x8
#endif

#endif
