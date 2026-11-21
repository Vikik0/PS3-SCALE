#include "net_adapter.h"
#include "relay_client.h"

#include <cell/libnet.h>
#include <cell/libnetctl.h>
#include <cell/sysmodule.h>
#include <sys/time_util.h>
#include <sys/timer.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/memory.h>
#include <sys/time.h>
#include <unistd.h>
#include <wchar.h>

#include "../upstream/crash_bash_decomp/port/platform/net.h"
#include "../upstream/crash_bash_decomp/port/platform/platform.h"
#include "../upstream/crash_bash_decomp/port/platform/portmap.h"
#include "../upstream/crash_bash_decomp/port/platform/saves.h"

/* net.c needs only these three settings. The real PC settings header is
 * intentionally skipped: the PS3 PRX has no SDL controls/settings UI. */
#define SETTINGS_H
typedef struct Settings {
    int skip_intro;
    int skip_loading;
    int cheat_menu;
} Settings;

static Settings ps3_settings;
const Settings *settings(void);
void settings_borrow(int skip_intro, int skip_loading, int cheat_menu, int extra_heroes);
void settings_return(void);

static CbRelayClient relay_client;
static CbRelayBatchQueue relay_batch;
static uint8_t relay_host_id[16];

typedef struct CbRelayPeerAddress {
    uint8_t session_id[16];
    struct sockaddr_in address;
    int used;
} CbRelayPeerAddress;

static CbRelayPeerAddress relay_peers[4];
static uint8_t relay_rx_packet[CB_RELAY_MAX_PACKET];
static uint8_t relay_rx_sender[16];
static size_t relay_rx_length;
static int relay_rx_valid;
static uint64_t relay_last_flush_ms;

static int relay_hex_to_bytes(const char *hex, uint8_t output[16])
{
    size_t i;
    if (!hex || strlen(hex) != 32u) return -1;
    for (i = 0; i < 16u; ++i) {
        char pair[3];
        char *end;
        pair[0] = hex[i * 2u];
        pair[1] = hex[i * 2u + 1u];
        pair[2] = '\0';
        output[i] = (uint8_t)strtoul(pair, &end, 16);
        if (*end != '\0') return -1;
    }
    return 0;
}

static void relay_session_address(const uint8_t session_id[16],
                                  struct sockaddr_in *address)
{
    uint32_t address_network = 0;
    unsigned short port_host = 0;

    memset(address, 0, sizeof(*address));
    address->sin_family = AF_INET;
    if (cb_relay_session_address(session_id, &address_network, &port_host) == 0) {
        address->sin_addr.s_addr = address_network;
        address->sin_port = htons(port_host);
    }
}

static void relay_peer_add(const uint8_t session_id[16])
{
    size_t i;
    size_t slot = 4u;
    struct sockaddr_in address;

    relay_session_address(session_id, &address);
    for (i = 0; i < 4u; ++i) {
        if (relay_peers[i].used &&
            memcmp(relay_peers[i].session_id, session_id, 16u) == 0) {
            relay_peers[i].address = address;
            return;
        }
        if (!relay_peers[i].used && slot == 4u) slot = i;
    }
    if (slot == 4u) return;
    memcpy(relay_peers[slot].session_id, session_id, 16u);
    relay_peers[slot].address = address;
    relay_peers[slot].used = 1;
}

static void relay_sync_roster(void)
{
    size_t i;

    memset(relay_peers, 0, sizeof(relay_peers));
    for (i = 0; i < CB_RELAY_MAX_PEERS; ++i) {
        const CbRelayPeer *peer = &relay_client.peers[i];
        struct sockaddr_in address;
        if (!peer->valid) continue;
        relay_session_address(peer->session_id, &address);
        memcpy(relay_peers[i].session_id, peer->session_id, 16u);
        relay_peers[i].address = address;
        relay_peers[i].used = 1;
    }
}

static uint64_t relay_now_ms(void)
{
    return sys_time_get_system_time() / 1000u;
}

#define RELAY_FLUSH_INTERVAL_MS 220u

static int relay_flush_due(int force)
{
    uint64_t now;
    if (!relay_client.connected || relay_batch.count == 0) return 0;
    now = relay_now_ms();
    if (!force && now >= relay_last_flush_ms &&
        now - relay_last_flush_ms < RELAY_FLUSH_INTERVAL_MS) return 0;
    if (cb_relay_batch_flush(&relay_client, &relay_batch) != 0) return -1;
    relay_last_flush_ms = now;
    return 0;
}

static int relay_peer_find(const struct sockaddr *destination,
                           socklen_t destination_length, uint8_t session_id[16])
{
    const struct sockaddr_in *wanted;
    size_t i;

    if (!destination || destination_length < (socklen_t)sizeof(struct sockaddr_in) ||
        destination->sa_family != AF_INET) return -1;
    wanted = (const struct sockaddr_in *)destination;
    for (i = 0; i < 4u; ++i) {
        if (relay_peers[i].used &&
            relay_peers[i].address.sin_addr.s_addr == wanted->sin_addr.s_addr &&
            relay_peers[i].address.sin_port == wanted->sin_port) {
            memcpy(session_id, relay_peers[i].session_id, 16u);
            return 0;
        }
    }
    return -1;
}

static int relay_receive_queue(int timeout_ms)
{
    int result;
    int controls = 0;

    if (relay_rx_valid) return 1;
    do {
        relay_rx_length = 0;
        result = cb_relay_receive_packet(&relay_client, relay_rx_packet,
                                         sizeof(relay_rx_packet), &relay_rx_length,
                                         relay_rx_sender, NULL, 0, timeout_ms);
        if (result == 2) {
            relay_sync_roster();
            timeout_ms = 0;
            controls++;
        }
    } while (result == 2 && controls < 8);
    if (result != 1) return result;
    relay_peer_add(relay_rx_sender);
    relay_rx_valid = 1;
    return 1;
}

static ssize_t cb_relay_sendto_shim(int fd, const void *buffer, size_t length,
                                    int flags, const struct sockaddr *destination,
                                    socklen_t destination_length)
{
    uint8_t recipient[16];
    if (!relay_client.connected) {
        return sendto(fd, buffer, length, flags, destination, destination_length);
    }
    if (!buffer || length == 0 || length > CB_RELAY_MAX_PACKET ||
        relay_peer_find(destination, destination_length, recipient) != 0) {
        errno = EHOSTUNREACH;
        return -1;
    }
    if (cb_relay_queue_packet(&relay_batch, recipient, buffer, length) != 0) {
        errno = EMSGSIZE;
        return -1;
    }
    if (relay_flush_due(0) != 0) {
        errno = ENETDOWN;
        return -1;
    }
    return (ssize_t)length;
}

static ssize_t cb_relay_recvfrom_shim(int fd, void *buffer, size_t length,
                                      int flags, struct sockaddr *source,
                                      socklen_t *source_length)
{
    struct sockaddr_in address;
    int result;
    (void)flags;

    if (!relay_client.connected) {
        return recvfrom(fd, buffer, length, flags, source, source_length);
    }
    if (!relay_rx_valid) {
        result = relay_receive_queue(0);
        if (result != 1) {
            errno = result < 0 ? ECONNRESET : EWOULDBLOCK;
            return -1;
        }
    }
    if (!buffer || length < relay_rx_length) {
        relay_rx_valid = 0;
        errno = EMSGSIZE;
        return -1;
    }

    memcpy(buffer, relay_rx_packet, relay_rx_length);
    relay_session_address(relay_rx_sender, &address);
    if (source && source_length) {
        socklen_t copy_length = *source_length < (socklen_t)sizeof(address)
            ? *source_length : (socklen_t)sizeof(address);
        memcpy(source, &address, copy_length);
        *source_length = (socklen_t)sizeof(address);
    }
    relay_rx_valid = 0;
    return (ssize_t)relay_rx_length;
}

static int cb_relay_select_shim(int nfds, fd_set *read_set, fd_set *write_set,
                                fd_set *except_set, struct timeval *timeout)
{
    int ready_fd = -1;
    int timeout_ms = -1;
    int result;
    int i;

    if (!relay_client.connected) return select(nfds, read_set, write_set, except_set, timeout);
    if (!read_set) return 0;
    for (i = 0; i < nfds; ++i) {
        if (FD_ISSET(i, read_set)) {
            ready_fd = i;
            break;
        }
    }
    if (ready_fd < 0) return 0;
    if (relay_flush_due(0) != 0) return -1;
    if (timeout) {
        timeout_ms = (int)(timeout->tv_sec * 1000L + timeout->tv_usec / 1000L);
    }

    result = relay_receive_queue(timeout_ms);
    FD_ZERO(read_set);
    if (result == 1) {
        FD_SET(ready_fd, read_set);
        return 1;
    }
    return result < 0 ? -1 : 0;
}

/* Only net.c's UDP send/receive/select calls are redirected. STUN and the
 * non-relay direct-UDP path continue to use the native socket functions. */
#define sendto cb_relay_sendto_shim
#define recvfrom cb_relay_recvfrom_shim
#define select cb_relay_select_shim
#define net_lobby_start net_lobby_start_original
#include "../upstream/crash_bash_decomp/port/platform/net.c"
#undef net_lobby_start
#undef select
#undef recvfrom
#undef sendto

void net_lobby_start_original(void);

void net_lobby_start(void)
{
    int m;
    int next_pad = 0;

    if (!relay_client.connected) {
        net_lobby_start_original();
        return;
    }
    if (Lobby != LOBBY_OPEN || NumMachines < 2) return;

    /* Relay batching adds about 200 ms. Use a fixed eight-step input delay
     * instead of the upstream 10 ms ping burst, which exceeds the free relay cap. */
    Delay = Options.delay > 0 ? Options.delay : 8;
    Delay = Delay > 30 ? 30 : Delay < 1 ? 1 : Delay;
    Machines[0].pads = own_pads();
    for (m = 0; m < NumMachines; ++m) {
        Machines[m].first = next_pad;
        Machines[m].pads = Machines[m].pads > PADS - next_pad
            ? PADS - next_pad : Machines[m].pads;
        Machines[m].ready = 0;
        next_pad += Machines[m].pads;
    }
    LobbyBegan = SDL_GetTicks();
    LobbySent = 0;
    Lobby = LOBBY_STARTING;
}

static volatile int net_active;
static volatile int lobby_started;
static volatile int have_previous_step;
static uint64_t last_frame_step_ns;
static int finish_code;
static NetOptions ps3_net_options;

#define CB_PS1_NETEMU_RAM_BASE 0x00770780u
#define CB_PS1_NETEMU_RAM_SIZE 0x00200000u
static int ps1_ram_probe_done;
static int ps1_ram_probe_ok;

static int ps1_ram_is_readable(void)
{
    sys_page_attr_t first_page;
    sys_page_attr_t last_page;
    sys_addr_t first = (sys_addr_t)CB_PS1_NETEMU_RAM_BASE;
    sys_addr_t last = (sys_addr_t)(CB_PS1_NETEMU_RAM_BASE + CB_PS1_NETEMU_RAM_SIZE - 1u);

    if (sys_memory_get_page_attribute(first, &first_page) != 0 ||
        sys_memory_get_page_attribute(last, &last_page) != 0) {
        return 0;
    }
    if ((first_page.access_right & SYS_MEMORY_ACCESS_RIGHT_PPU_THR) == 0 ||
        (last_page.access_right & SYS_MEMORY_ACCESS_RIGHT_PPU_THR) == 0) {
        return 0;
    }
    return 1;
}

static uint32_t ps1_ram_fingerprint(void)
{
    const volatile uint8_t *ram;
    uint32_t hash = 2166136261u;
    uint32_t i;

    if (!ps1_ram_probe_done) {
        ps1_ram_probe_ok = ps1_ram_is_readable();
        ps1_ram_probe_done = 1;
    }
    if (!ps1_ram_probe_ok) return 0;

    ram = (const volatile uint8_t *)(uintptr_t)CB_PS1_NETEMU_RAM_BASE;
    for (i = 0; i < CB_PS1_NETEMU_RAM_SIZE; ++i) {
        hash ^= ram[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int stun_binding(int socket_fd, const char *server, uint16_t server_port,
                        char *public_ip, size_t public_ip_room, int *public_port)
{
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    struct sockaddr_in from;
    socklen_t from_length;
    struct timeval timeout;
    fd_set readable;
    uint8_t request[20];
    uint8_t packet[512];
    uint64_t seed;
    int sent = 0;
    int result = -1;
    char service[8];
    size_t i;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    snprintf(service, sizeof(service), "%u", (unsigned int)server_port);
    if (getaddrinfo(server, service, &hints, &addresses) != 0) {
        return -1;
    }

    for (address = addresses; address; address = address->ai_next) {
        memset(request, 0, sizeof(request));
        request[0] = 0x00;
        request[1] = 0x01; /* Binding Request */
        request[4] = 0x21;
        request[5] = 0x12;
        request[6] = 0xa4;
        request[7] = 0x42;
        seed = sys_time_get_system_time();
        for (i = 0; i < 12; ++i) {
            seed = seed * 6364136223846793005ULL + 1ULL;
            request[8 + i] = (uint8_t)(seed >> 32);
        }

        if (sendto(socket_fd, request, sizeof(request), 0,
                   address->ai_addr, (socklen_t)address->ai_addrlen) < 0) {
            continue;
        }
        sent = 1;
        FD_ZERO(&readable);
        FD_SET(socket_fd, &readable);
        timeout.tv_sec = 1;
        timeout.tv_usec = 200000;
        if (select(socket_fd + 1, &readable, NULL, NULL, &timeout) <= 0) {
            continue;
        }

        from_length = sizeof(from);
        memset(&from, 0, sizeof(from));
        {
            ssize_t received = recvfrom(socket_fd, packet, sizeof(packet), 0,
                                        (struct sockaddr *)&from, &from_length);
            size_t offset;
            size_t message_end;

            if (received < 20 || packet[0] != 0x01 || packet[1] != 0x01 ||
                packet[4] != 0x21 || packet[5] != 0x12 ||
                packet[6] != 0xa4 || packet[7] != 0x42 ||
                memcmp(packet + 8, request + 8, 12) != 0) {
                continue;
            }

            message_end = 20u + read_be16(packet + 2);
            if (message_end > (size_t)received) {
                continue;
            }

            for (offset = 20; offset + 4 <= message_end;) {
                uint16_t type = read_be16(packet + offset);
                uint16_t length = read_be16(packet + offset + 2);
                const uint8_t *value = packet + offset + 4;

                if (offset + 4u + length > message_end) {
                    break;
                }

                if ((type == 0x0020 || type == 0x0001) && length >= 8 &&
                    value[1] == 0x01) {
                    uint16_t mapped_port = read_be16(value + 2);
                    uint32_t mapped_address;

                    if (type == 0x0020) {
                        mapped_port ^= 0x2112;
                        mapped_address = read_be32(value + 4) ^ 0x2112a442u;
                    } else {
                        mapped_address = read_be32(value + 4);
                    }

                    {
                        struct in_addr mapped;
                        mapped.s_addr = htonl(mapped_address);
                        if (!inet_ntop(AF_INET, &mapped, public_ip,
                                       (socklen_t)public_ip_room)) {
                            break;
                        }
                    }
                    *public_port = mapped_port;
                    result = 0;
                    break;
                }

                offset += 4u + ((length + 3u) & ~3u);
            }

            if (result == 0) {
                break;
            }
        }
    }

    freeaddrinfo(addresses);
    return sent ? result : -1;
}

/* SDL3 timing and file helpers used by the upstream netplay code. */
Uint64 SDL_GetTicks(void)
{
    return sys_time_get_system_time() / 1000u;
}

Uint64 SDL_GetTicksNS(void)
{
    return sys_time_get_system_time() * 1000u;
}

Uint64 SDL_GetPerformanceCounter(void)
{
    return sys_time_get_system_time();
}

void SDL_Delay(Uint32 milliseconds)
{
    sys_timer_usleep((uint64_t)milliseconds * 1000u);
}

size_t SDL_strlcpy(char *destination, const char *source, size_t size)
{
    size_t length = source ? strlen(source) : 0;
    if (size) {
        size_t copy = length < size - 1 ? length : size - 1;
        if (copy) memcpy(destination, source, copy);
        destination[copy] = '\0';
    }
    return length;
}

int SDL_snprintf(char *buffer, size_t size, const char *format, ...)
{
    int result;
    va_list args;
    va_start(args, format);
    result = vsnprintf(buffer, size, format, args);
    va_end(args);
    return result;
}

char *SDL_iconv_string(const char *to, const char *from, const char *input, size_t length)
{
    char *copy;
    (void)to;
    (void)from;
    if (!input) return NULL;
    copy = (char *)malloc(length + 1);
    if (!copy) return NULL;
    memcpy(copy, input, length);
    copy[length] = '\0';
    return copy;
}

size_t SDL_wcslen(const wchar_t *string)
{
    return string ? wcslen(string) : 0;
}

void SDL_free(void *memory)
{
    free(memory);
}

SDL_IOStream *SDL_IOFromFile(const char *file, const char *mode)
{
    return fopen(file, mode);
}

Sint64 SDL_GetIOSize(SDL_IOStream *stream)
{
    long current;
    long end;
    if (!stream) return -1;
    current = ftell(stream);
    if (current < 0 || fseek(stream, 0, SEEK_END) != 0) return -1;
    end = ftell(stream);
    (void)fseek(stream, current, SEEK_SET);
    return end;
}

size_t SDL_ReadIO(SDL_IOStream *stream, void *destination, size_t size)
{
    return stream ? fread(destination, 1, size, stream) : 0;
}

int SDL_CloseIO(SDL_IOStream *stream)
{
    return stream ? fclose(stream) == 0 : 0;
}

/* A single IPv4 interface is enough for the upstream lobby's adapter list. */
int getifaddrs(struct ifaddrs **ifap)
{
    static struct ifaddrs interface;
    static struct sockaddr_in address;
    static struct sockaddr_in netmask;
    static struct sockaddr_in broadcast;
    static char interface_name[] = "ps3";
    union CellNetCtlInfo ip_info;
    union CellNetCtlInfo mask_info;
    uint32_t ip;
    uint32_t mask;

    if (!ifap) return -1;
    *ifap = NULL;
    memset(&interface, 0, sizeof(interface));
    memset(&address, 0, sizeof(address));
    memset(&netmask, 0, sizeof(netmask));
    memset(&broadcast, 0, sizeof(broadcast));

    if (cellNetCtlGetInfo(CELL_NET_CTL_INFO_IP_ADDRESS, &ip_info) != 0 ||
        cellNetCtlGetInfo(CELL_NET_CTL_INFO_NETMASK, &mask_info) != 0) {
        return -1;
    }

    address.sin_family = AF_INET;
    netmask.sin_family = AF_INET;
    broadcast.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip_info.ip_address, &address.sin_addr) != 1 ||
        inet_pton(AF_INET, mask_info.netmask, &netmask.sin_addr) != 1) {
        return -1;
    }

    ip = ntohl(address.sin_addr.s_addr);
    mask = ntohl(netmask.sin_addr.s_addr);
    broadcast.sin_addr.s_addr = htonl(ip | ~mask);

    interface.ifa_name = interface_name;
    interface.ifa_flags = IFF_UP | IFF_BROADCAST;
    interface.ifa_addr = (struct sockaddr *)&address;
    interface.ifa_netmask = (struct sockaddr *)&netmask;
    interface.ifa_broadaddr = (struct sockaddr *)&broadcast;
    interface.ifa_next = NULL;
    *ifap = &interface;
    return 0;
}

void freeifaddrs(struct ifaddrs *ifap)
{
    (void)ifap;
}

const Settings *settings(void)
{
    return &ps3_settings;
}

void settings_borrow(int skip_intro, int skip_loading, int cheat_menu, int extra_heroes)
{
    ps3_settings.skip_intro = skip_intro;
    ps3_settings.skip_loading = skip_loading;
    ps3_settings.cheat_menu = cheat_menu;
    (void)extra_heroes;
}

void settings_return(void)
{
}

void saves_in_memory(int on)
{
    (void)on;
}

void saves_put(int slot, const void *data, int size)
{
    (void)slot;
    (void)data;
    (void)size;
}

int saves_get(int slot, void *data, int room)
{
    (void)slot;
    (void)data;
    (void)room;
    return 0; /* Empty save slots for the first Battle-mode prototype. */
}

int card_check(const void *data, int size, char *why, size_t room)
{
    (void)data;
    if (why && room) why[0] = '\0';
    return size == 0;
}

const char *platform_data_path(void)
{
    return "/dev_hdd0/tmp/";
}

const char *saves_folder(void)
{
    return platform_data_path();
}

int platform_local_pad(int port)
{
    CbPadState state;
    if (port != 0 || cb_input_read_physical(&state) != 0) return -1;
    return (int)(state.digital1 | state.digital2);
}

int platform_local_pad_sticks(int port, unsigned char sticks[4])
{
    CbPadState state;
    if (port != 0 || !sticks || cb_input_read_physical(&state) != 0) return 0;
    sticks[0] = state.right_x;
    sticks[1] = state.right_y;
    sticks[2] = state.left_x;
    sticks[3] = state.left_y;
    return 1;
}

int platform_local_pad_rumbles(int port)
{
    (void)port;
    return 0;
}

int platform_online_players(void)
{
    return net_players();
}

void platform_online_start(void)
{
    net_active = 1;
    have_previous_step = 0;
    last_frame_step_ns = 0;
    cb_input_set_active(1, 0);
    cb_input_set_netplay_active(1);
}

void platform_online_stop(const char *why)
{
    (void)why;
    net_active = 0;
    have_previous_step = 0;
    cb_input_set_netplay_active(0);
    cb_input_set_active(0, 0);
}

int platform_idle(void)
{
    return 1;
}

void platform_finish(int code)
{
    finish_code = code;
}

void platform_pad_step(void)
{
}

void platform_step_done(uint32_t fingerprint)
{
    if (net_on()) net_step_done(fingerprint);
}

void platform_vblank(void)
{
    if (net_on()) net_vblank();
}

int platform_pad(int port)
{
    return net_on() ? net_pad(port) : platform_local_pad(port);
}

int platform_pad_sticks(int port, unsigned char sticks[4])
{
    return net_on() ? net_pad_sticks(port, sticks) : platform_local_pad_sticks(port, sticks);
}

int platform_pad_rumbles(int port)
{
    return net_on() ? net_pad_rumbles(port) : platform_local_pad_rumbles(port);
}

void platform_rumble(int port, int small_on, int big)
{
    (void)port;
    (void)small_on;
    (void)big;
}

int platform_vblanks(void)
{
    return 0;
}

void platform_wait_vblanks(int count)
{
    if (count > 0) sys_timer_usleep((uint64_t)count * 16667u);
}

void platform_fatal(const char *message)
{
    (void)message;
}

static int portmap_status = PORTMAP_OFF;
static char portmap_ip[16];
static int portmap_external_port;

void portmap_open(int port, int first, int pretend)
{
    char first_ip[16];
    int first_port = 0;
    char second_ip[16];
    int second_port = 0;
    (void)port;
    (void)first;

    portmap_status = PORTMAP_WORKING;
    portmap_ip[0] = '\0';
    portmap_external_port = 0;

    if (pretend) {
        portmap_status = pretend;
        return;
    }

    if (Sock == INVALID_SOCKET ||
        stun_binding((int)Sock, "stun.l.google.com", 19302,
                     first_ip, sizeof(first_ip), &first_port) != 0) {
        portmap_status = PORTMAP_NO_ROUTER;
        return;
    }

    /* A second destination distinguishes endpoint-independent NAT from
     * symmetric/CGNAT mappings. If the mapped port changes, direct P2P is
     * unlikely to work without a relay. */
    if (stun_binding((int)Sock, "stun1.l.google.com", 19302,
                     second_ip, sizeof(second_ip), &second_port) == 0 &&
        (first_port != second_port || strcmp(first_ip, second_ip) != 0)) {
        portmap_status = PORTMAP_SHARED;
        return;
    }

    SDL_strlcpy(portmap_ip, first_ip, sizeof(portmap_ip));
    portmap_external_port = first_port;
    portmap_status = PORTMAP_OPEN;
}

int portmap_state(void)
{
    return portmap_status;
}

const char *portmap_address(void)
{
    return portmap_ip;
}

int portmap_port(void)
{
    return portmap_external_port;
}

int portmap_check(int port, int first)
{
    portmap_open(port, first, 0);
    if (portmap_status == PORTMAP_OPEN) {
        portmap_close();
        return 1;
    }
    return 0;
}

void portmap_close(void)
{
    portmap_status = PORTMAP_OFF;
    portmap_ip[0] = '\0';
    portmap_external_port = 0;
}

static int relay_batch_tick_once(void)
{
    return relay_flush_due(0);
}

static void configure_options(void)
{
    memset(&ps3_net_options, 0, sizeof(ps3_net_options));
    ps3_net_options.joiners = 3;
    ps3_net_options.delay = 0;
    ps3_net_options.patience = 5000;
    ps3_net_options.differ_at = -1;
    ps3_net_options.vanish_at = -1;
    ps3_net_options.stall_at = -1;
    ps3_net_options.stall_ms = 0;
    ps3_net_options.port = NET_PORT;
    ps3_net_options.pretend_router = 0;
    net_configure(&ps3_net_options);
}

int cb_net_init(void)
{
    (void)cellSysmoduleLoadModule(CELL_SYSMODULE_NET);
    (void)cellSysmoduleLoadModule(CELL_SYSMODULE_NETCTL);
    (void)sys_net_initialize_network();
    (void)cellNetCtlInit();
    configure_options();
    return 0;
}

int cb_net_host(void)
{
    int result;
    if (relay_client.connected) cb_net_leave();
    configure_options();
    net_active = 0;
    lobby_started = 0;
    result = net_lobby_host(NULL);
    if (result) lobby_started = 1;
    return result ? 0 : -1;
}

int cb_net_join_code(const char *code)
{
    char address[64];
    if (relay_client.connected) cb_net_leave();
    if (!code || !net_code_read(code, address, sizeof(address))) return -1;
    configure_options();
    net_active = 0;
    lobby_started = 0;
    if (!net_lobby_join(address)) return -1;
    lobby_started = 1;
    return 0;
}

int cb_net_relay_host(const char *worker_host, unsigned short worker_port)
{
    int result;

    if (!worker_host || !*worker_host) return -1;
    cb_net_leave();
    (void)cb_net_init();
    configure_options();
    ps3_net_options.delay = 12;
    ps3_net_options.local = 1;
    net_configure(&ps3_net_options);

    if (cb_relay_host(&relay_client, worker_host, worker_port, "PS3 Host", 5000) != 0 ||
        relay_hex_to_bytes(relay_client.host_session_id, relay_host_id) != 0) {
        cb_relay_leave(&relay_client);
        return -1;
    }

    memset(&relay_batch, 0, sizeof(relay_batch));
    memset(relay_peers, 0, sizeof(relay_peers));
    relay_rx_valid = 0;
    relay_peer_add(relay_host_id);
    relay_sync_roster();
    relay_last_flush_ms = relay_now_ms();
    net_active = 0;
    lobby_started = 0;
    have_previous_step = 0;
    last_frame_step_ns = 0;

    result = net_lobby_host(NULL);
    if (!result) {
        cb_relay_leave(&relay_client);
        memset(relay_peers, 0, sizeof(relay_peers));
        return -1;
    }
    lobby_started = 1;
    return 0;
}

int cb_net_relay_join_code(const char *worker_host, unsigned short worker_port,
                           const char *code)
{
    char host_address[32];
    char join_address[64];
    unsigned short host_port = 0;

    if (!worker_host || !*worker_host || !code) return -1;
    cb_net_leave();
    (void)cb_net_init();
    configure_options();
    ps3_net_options.delay = 12;
    net_configure(&ps3_net_options);

    if (cb_relay_join(&relay_client, worker_host, worker_port, code,
                      "PS3 Player", 5000) != 0 ||
        relay_hex_to_bytes(relay_client.host_session_id, relay_host_id) != 0 ||
        cb_relay_host_address(&relay_client, host_address, sizeof(host_address),
                              &host_port) != 0) {
        cb_relay_leave(&relay_client);
        return -1;
    }

    if (snprintf(join_address, sizeof(join_address), "%s:%u",
                 host_address, (unsigned int)host_port) >= (int)sizeof(join_address)) {
        cb_relay_leave(&relay_client);
        return -1;
    }

    memset(&relay_batch, 0, sizeof(relay_batch));
    memset(relay_peers, 0, sizeof(relay_peers));
    relay_rx_valid = 0;
    relay_peer_add(relay_host_id);
    relay_sync_roster();
    relay_last_flush_ms = relay_now_ms();
    net_active = 0;
    lobby_started = 0;
    have_previous_step = 0;
    last_frame_step_ns = 0;

    if (!net_lobby_join(join_address)) {
        cb_relay_leave(&relay_client);
        memset(relay_peers, 0, sizeof(relay_peers));
        return -1;
    }
    lobby_started = 1;
    return 0;
}

int cb_net_relay_start(void)
{
    if (!relay_client.connected || !lobby_started ||
        strcmp(relay_client.session_id, relay_client.host_session_id) != 0) return -1;
    if (cb_relay_start_match(&relay_client, 5000) != 0) return -1;
    relay_sync_roster();
    ps3_net_options.delay = 12;
    net_configure(&ps3_net_options);
    net_lobby_start();
    return 0;
}

int cb_net_lobby_poll(void)
{
    int status;

    if (!lobby_started) {
        return net_active ? LOBBY_GO : LOBBY_OFF;
    }

    status = net_lobby_poll();
    if (relay_client.connected && !net_active &&
        relay_batch_tick_once() != 0) {
        cb_relay_leave(&relay_client);
        lobby_started = 0;
        return LOBBY_FAILED;
    }
    if (status == LOBBY_GO && !net_active && net_on()) {
        lobby_started = 0;
        platform_online_start();
    } else if (status == LOBBY_FAILED || status == LOBBY_OFF) {
        lobby_started = 0;
    }
    return status;
}

int cb_net_lobby_start(void)
{
    if (relay_client.connected) return cb_net_relay_start();
    net_lobby_start();
    return 0;
}

void cb_net_leave(void)
{
    lobby_started = 0;
    net_lobby_leave();
    net_end();
    net_lobby_reset();
    if (relay_client.connected && relay_batch.count > 0) {
        (void)relay_flush_due(1);
    }
    cb_relay_leave(&relay_client);
    memset(&relay_batch, 0, sizeof(relay_batch));
    memset(relay_peers, 0, sizeof(relay_peers));
    memset(relay_host_id, 0, sizeof(relay_host_id));
    relay_rx_valid = 0;
    relay_rx_length = 0;
    net_active = 0;
    have_previous_step = 0;
    last_frame_step_ns = 0;
    last_relay_batch_tick_ns = 0;
    platform_online_stop(NULL);
    portmap_close();
}

int cb_net_is_active(void)
{
    return net_active && net_on();
}

const char *cb_net_join_code_text(void)
{
    return relay_client.connected ? relay_client.room_code : net_lobby_code();
}

void cb_net_frame(void)
{
    uint64_t now;
    int slot;

    if (!cb_net_is_active()) return;
    now = SDL_GetTicksNS();
    if (last_frame_step_ns && now - last_frame_step_ns < 30000000ULL) return;
    last_frame_step_ns = now;

    if (have_previous_step) {
        net_step_done(ps1_ram_fingerprint());
    }

    net_vblank();
    net_step();
    have_previous_step = 1;
    if (relay_client.connected &&
        relay_batch_tick_once() != 0) {
        net_active = 0;
        lobby_started = 0;
        cb_relay_leave(&relay_client);
        platform_online_stop(NULL);
        return;
    }

    for (slot = 0; slot < 4; ++slot) {
        CbPadState state;
        unsigned char sticks[4] = { 0x80, 0x80, 0x80, 0x80 };
        int buttons = net_pad(slot);

        state.digital1 = buttons < 0 ? 0 : (uint16_t)(buttons & 0x00ff);
        state.digital2 = buttons < 0 ? 0 : (uint16_t)(buttons & 0xff00);
        if (net_pad_sticks(slot, sticks)) {
            state.right_x = sticks[0];
            state.right_y = sticks[1];
            state.left_x = sticks[2];
            state.left_y = sticks[3];
        } else {
            state.right_x = 0x80;
            state.right_y = 0x80;
            state.left_x = 0x80;
            state.left_y = 0x80;
        }
        cb_input_set_remote_state(slot, &state);
    }
}
