#include "net_adapter.h"

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

/* Include the upstream four-player deterministic UDP lockstep implementation
 * in this translation unit. This lets the PS3 adapter provide the platform,
 * timing, socket-interface and port-mapping functions it expects. */
#include "../upstream/crash_bash_decomp/port/platform/net.c"

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
    if (!code || !net_code_read(code, address, sizeof(address))) return -1;
    configure_options();
    net_active = 0;
    lobby_started = 0;
    if (!net_lobby_join(address)) return -1;
    lobby_started = 1;
    return 0;
}

int cb_net_lobby_poll(void)
{
    int status;

    if (!lobby_started) {
        return net_active ? LOBBY_GO : LOBBY_OFF;
    }

    status = net_lobby_poll();
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
    net_lobby_start();
    return 0;
}

void cb_net_leave(void)
{
    lobby_started = 0;
    net_lobby_leave();
    net_end();
    net_lobby_reset();
    platform_online_stop(NULL);
    portmap_close();
}

int cb_net_is_active(void)
{
    return net_active && net_on();
}

const char *cb_net_join_code_text(void)
{
    return net_lobby_code();
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
