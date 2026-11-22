/* Read-only PS3 pad-port probe for Ghosts Campaign/Multiplayer. */
#include <stdint.h>
#include <stddef.h>
#include <cell/pad.h>
#include <cell/fs/cell_fs_file_api.h>
#include <sys/thread.h>
#include <sys/timer.h>
#include <sys/prx_module.h>

#ifndef SYS_PRX_RESIDENT
#define SYS_PRX_RESIDENT 0
#endif

/* Direct sysPrxForUser imports from liblv2_stub; no pthread/librt wrapper. */
extern int sys_ppu_thread_create(
    sys_ppu_thread_t *thread_id,
    void (*entry)(uint64_t),
    uint64_t arg,
    int priority,
    size_t stack_size,
    uint64_t flags,
    const char *thread_name
);
extern void sys_ppu_thread_exit(uint64_t exit_code);

#define PROBE_MAGIC 0x47505242u
#define MODE_PATH "/dev_hdd0/tmp/ghosts_probe_mode.txt"
#define ERROR_LOG_PATH "/dev_hdd0/tmp/ghosts_pad_probe_error.log"
#define CAMPAIGN_LOG_PATH "/dev_hdd0/tmp/ghosts_campaign_pad_probe.log"
#define MULTIPLAYER_LOG_PATH "/dev_hdd0/tmp/ghosts_multiplayer_pad_probe.log"
#define PROBE_SAMPLES 30u
#define PROBE_INTERVAL_US 1000000u
#define PROBE_PRIORITY 3000
#define PROBE_STACK_SIZE 0x8000u
#define LINE_CAPACITY 320u

static volatile uint32_t g_stop;
static uint32_t g_thread_started;
static sys_ppu_thread_t g_thread;
static const char *g_mode_name;
static const char *g_log_path;

static void zero_bytes(void *memory, uint32_t length)
{
    uint8_t *bytes = (uint8_t *)memory;
    while (length--) *bytes++ = 0u;
}

static int text_equal(const char *left, const char *right)
{
    uint32_t i = 0;
    while (left[i] && right[i] && left[i] == right[i]) ++i;
    return left[i] == '\0' && right[i] == '\0';
}

static void append_char(char *buffer, uint32_t capacity, uint32_t *used, char value)
{
    if (*used + 1u >= capacity) return;
    buffer[(*used)++] = value;
    buffer[*used] = '\0';
}

static void append_text(char *buffer, uint32_t capacity, uint32_t *used, const char *value)
{
    uint32_t i = 0;
    while (value[i]) append_char(buffer, capacity, used, value[i++]);
}

static void append_u32(char *buffer, uint32_t capacity, uint32_t *used, uint32_t value)
{
    char digits[10];
    uint32_t count = 0;
    do {
        digits[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value && count < sizeof(digits));
    while (count) append_char(buffer, capacity, used, digits[--count]);
}

static void append_hex8(char *buffer, uint32_t capacity, uint32_t *used, uint32_t value)
{
    static const char hex[] = "0123456789abcdef";
    int shift;
    for (shift = 28; shift >= 0; shift -= 4)
        append_char(buffer, capacity, used, hex[(value >> shift) & 0x0fu]);
}

static int write_all(int fd, const char *buffer, uint64_t length)
{
    const char *cursor = buffer;
    while (length) {
        uint64_t written = 0;
        if (cellFsWrite(fd, cursor, length, &written) != CELL_FS_SUCCEEDED || written == 0)
            return 0;
        cursor += written;
        length -= written;
    }
    return 1;
}

static int open_log(int truncate_file)
{
    int fd = -1;
    int flags = CELL_FS_O_WRONLY | CELL_FS_O_CREAT;
    if (truncate_file) flags |= CELL_FS_O_TRUNC;
    else flags |= CELL_FS_O_APPEND;
    if (cellFsOpen(g_log_path, flags, &fd, NULL, 0) != CELL_FS_SUCCEEDED)
        return -1;
    return fd;
}

static void log_literal(int fd, const char *value)
{
    uint32_t length = 0;
    while (value[length]) ++length;
    (void)write_all(fd, value, length);
}

static void log_error(const char *message)
{
    int fd;
    g_log_path = ERROR_LOG_PATH;
    fd = open_log(1);
    if (fd < 0) return;
    log_literal(fd, "Ghosts Pad Probe v4\nREAD_ONLY=1\nERROR=");
    log_literal(fd, message);
    log_literal(fd, "\nNo pad sampling was started.\n");
    cellFsClose(fd);
}

static int select_mode(void)
{
    char mode[32];
    uint64_t read_count = 0;
    int fd = -1;
    uint32_t i;

    zero_bytes(mode, sizeof(mode));
    if (cellFsOpen(MODE_PATH, CELL_FS_O_RDONLY, &fd, NULL, 0) != CELL_FS_SUCCEEDED)
        return 0;
    if (cellFsRead(fd, mode, sizeof(mode) - 1u, &read_count) != CELL_FS_SUCCEEDED) {
        cellFsClose(fd);
        return 0;
    }
    cellFsClose(fd);
    if (read_count >= sizeof(mode)) return 0;
    mode[read_count] = '\0';

    for (i = 0; mode[i]; ++i) {
        if (mode[i] == '\r' || mode[i] == '\n' || mode[i] == ' ' || mode[i] == '\t') {
            mode[i] = '\0';
            break;
        }
    }
    if (text_equal(mode, "CAMPAIGN")) {
        g_mode_name = "CAMPAIGN";
        g_log_path = CAMPAIGN_LOG_PATH;
        return 1;
    }
    if (text_equal(mode, "MULTIPLAYER")) {
        g_mode_name = "MULTIPLAYER";
        g_log_path = MULTIPLAYER_LOG_PATH;
        return 1;
    }
    return 0;
}

static void write_sample_line(int fd, uint32_t sample, const CellPadInfo2 *info, uint32_t mask)
{
    char line[LINE_CAPACITY];
    uint32_t used = 0;
    line[0] = '\0';
    append_text(line, sizeof(line), &used, "sample=");
    append_u32(line, sizeof(line), &used, sample);
    append_text(line, sizeof(line), &used, " result=OK max_connect=");
    append_u32(line, sizeof(line), &used, info->max_connect);
    append_text(line, sizeof(line), &used, " now_connect=");
    append_u32(line, sizeof(line), &used, info->now_connect);
    append_text(line, sizeof(line), &used, " system_info=0x");
    append_hex8(line, sizeof(line), &used, info->system_info);
    append_text(line, sizeof(line), &used, " port_mask=0x");
    append_hex8(line, sizeof(line), &used, mask);
    append_char(line, sizeof(line), &used, '\n');
    (void)write_all(fd, line, used);
}

static void write_error_line(int fd, uint32_t sample, int32_t result)
{
    char line[LINE_CAPACITY];
    uint32_t used = 0;
    line[0] = '\0';
    append_text(line, sizeof(line), &used, "sample=");
    append_u32(line, sizeof(line), &used, sample);
    append_text(line, sizeof(line), &used, " result=ERROR rc=0x");
    append_hex8(line, sizeof(line), &used, (uint32_t)result);
    append_char(line, sizeof(line), &used, '\n');
    (void)write_all(fd, line, used);
}

static void write_port_lines(int fd, const CellPadInfo2 *info)
{
    uint32_t port;
    for (port = 0; port < CELL_PAD_MAX_PORT_NUM; ++port) {
        char line[LINE_CAPACITY];
        uint32_t used = 0;
        line[0] = '\0';
        append_text(line, sizeof(line), &used, "  port");
        append_u32(line, sizeof(line), &used, port);
        append_text(line, sizeof(line), &used, " status=0x");
        append_hex8(line, sizeof(line), &used, info->port_status[port]);
        append_text(line, sizeof(line), &used, " capability=0x");
        append_hex8(line, sizeof(line), &used, info->device_capability[port]);
        append_text(line, sizeof(line), &used, " device_type=");
        append_u32(line, sizeof(line), &used, info->device_type[port]);
        append_text(line, sizeof(line), &used,
            (info->port_status[port] & CELL_PAD_STATUS_CONNECTED) ? " CONNECTED\n" : " disconnected\n");
        (void)write_all(fd, line, used);
    }
}

static void pad_probe_thread(uint64_t arg)
{
    int fd;
    uint32_t sample;
    uint32_t previous_mask = 0xffffffffu;
    (void)arg;

    fd = open_log(0);
    if (fd < 0) {
        sys_ppu_thread_exit(1u);
        return;
    }

    for (sample = 0; sample < PROBE_SAMPLES && !g_stop; ++sample) {
        CellPadInfo2 info;
        uint32_t mask = 0;
        uint32_t port;
        int32_t result;
        zero_bytes(&info, sizeof(info));
        result = cellPadGetInfo2(&info);
        if (result != CELL_PAD_OK) {
            write_error_line(fd, sample, result);
        } else {
            for (port = 0; port < CELL_PAD_MAX_PORT_NUM; ++port) {
                if (info.port_status[port] & CELL_PAD_STATUS_CONNECTED)
                    mask |= (1u << port);
            }
            if (mask != previous_mask || (sample % 5u) == 0u) {
                write_sample_line(fd, sample, &info, mask);
                write_port_lines(fd, &info);
            }
            previous_mask = mask;
        }
        sys_timer_usleep(PROBE_INTERVAL_US);
    }

    log_literal(fd, "Pad probe finished.\n");
    cellFsClose(fd);
    sys_ppu_thread_exit(0u);
}

int ghosts_probe_ping(void)
{
    return (int)PROBE_MAGIC;
}

int module_start(size_t args, void *argp)
{
    int fd;
    int result;
    (void)args;
    (void)argp;

    g_stop = 0;
    g_thread_started = 0;
    if (!select_mode()) {
        log_error("MODE_FILE_MUST_CONTAIN_EXACTLY_CAMPAIGN_OR_MULTIPLAYER");
        return SYS_PRX_RESIDENT;
    }

    fd = open_log(1);
    if (fd < 0) return SYS_PRX_RESIDENT;
    log_literal(fd, "Ghosts Campaign Co-op Pad Probe v4\nMODE=");
    log_literal(fd, g_mode_name);
    log_literal(fd, "\nREAD_ONLY=1\n");
    log_literal(fd, "Calls: cellPadGetInfo2 only; no cellPadInit/End/GetData\n");
    log_literal(fd, "No game memory writes, hooks, or dvar changes.\n\n");
    cellFsClose(fd);

    result = sys_ppu_thread_create(
        &g_thread, pad_probe_thread, 0u, PROBE_PRIORITY, PROBE_STACK_SIZE,
        THREAD_JOINABLE, "GhostsPadProbe"
    );
    if (result == 0) {
        g_thread_started = 1;
    } else {
        fd = open_log(0);
        if (fd >= 0) {
            write_error_line(fd, 0u, (int32_t)result);
            cellFsClose(fd);
        }
    }
    return SYS_PRX_RESIDENT;
}

int module_stop(size_t args, void *argp)
{
    uint64_t thread_result = 0;
    (void)args;
    (void)argp;
    g_stop = 1;
    if (g_thread_started) {
        (void)sysThreadJoin(g_thread, &thread_result);
        g_thread_started = 0;
    }
    return SYS_PRX_RESIDENT;
}
