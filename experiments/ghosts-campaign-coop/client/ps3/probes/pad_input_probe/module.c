/* Read-only DualShock input telemetry for Ghosts Campaign/Multiplayer. */
#include <stdint.h>
#include <stddef.h>
#include "../../local_input.h"
#include <cell/fs/cell_fs_file_api.h>
#include <sys/lv2_syscall.h>
#include <sys/timer.h>
#include <sys/prx_module.h>
#include <sys/prx.h>

#ifndef SYS_PRX_RESIDENT
#define SYS_PRX_RESIDENT 0
#endif

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

#define MODE_PATH "/dev_hdd0/tmp/ghosts_probe_mode.txt"
#define ERROR_LOG_PATH "/dev_hdd0/tmp/ghosts_pad_input_probe_error.log"
#define CAMPAIGN_LOG_PATH "/dev_hdd0/tmp/ghosts_campaign_pad_input_probe.log"
#define MULTIPLAYER_LOG_PATH "/dev_hdd0/tmp/ghosts_multiplayer_pad_input_probe.log"
#define PROBE_SAMPLES 30u
#define PROBE_INTERVAL_US 1000000u
#define PROBE_PRIORITY 3000
#define PROBE_STACK_SIZE 0x8000u
#define LINE_CAPACITY 512u
#define PROBE_MAGIC 0x47504950u

typedef uint64_t sys_ppu_thread_t;
#define THREAD_JOINABLE 1u

static volatile uint32_t g_stop;
static uint32_t g_thread_started;
static sys_ppu_thread_t g_thread;
static const char *g_mode_name;
static const char *g_log_path;

static int thread_join(sys_ppu_thread_t thread_id, uint64_t *result)
{
    lv2syscall2(44, thread_id, (uint64_t)(uintptr_t)result);
    return_to_user_prog(int);
}

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

static void append_i32(char *buffer, uint32_t capacity, uint32_t *used, int32_t value)
{
    uint32_t magnitude;
    if (value < 0) {
        append_char(buffer, capacity, used, '-');
        magnitude = (uint32_t)(-(value + 1)) + 1u;
    } else {
        magnitude = (uint32_t)value;
    }
    append_u32(buffer, capacity, used, magnitude);
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
    log_literal(fd, "Ghosts DualShock Input Probe v1\nREAD_ONLY=1\nERROR=");
    log_literal(fd, message);
    log_literal(fd, "\nNo input sampling was started.\n");
    cellFsClose(fd);
}

static int select_mode(void)
{
    char mode[32];
    uint64_t read_count = 0;
    int fd = -1;
    uint32_t i;
    uint32_t length;

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
    length = (uint32_t)read_count;
    while (length > 0u) {
        char last = mode[length - 1u];
        if (last != '\r' && last != '\n' && last != ' ' && last != '\t') break;
        mode[--length] = '\0';
    }
    for (i = 0; i < length; ++i) {
        if (mode[i] == '\r' || mode[i] == '\n' || mode[i] == ' ' || mode[i] == '\t')
            return 0;
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

static void write_sample_line(
    int fd,
    uint32_t sample,
    int32_t result,
    const GhcLocalInputSnapshot *snapshot,
    const GhcLocalInputFrame *frame
)
{
    char line[LINE_CAPACITY];
    uint32_t used = 0;
    line[0] = '\0';
    append_text(line, sizeof(line), &used, "sample=");
    append_u32(line, sizeof(line), &used, sample);
    append_text(line, sizeof(line), &used, result == GHC_LOCAL_INPUT_OK ? " result=OK" : " result=ERROR rc=0x");
    if (result != GHC_LOCAL_INPUT_OK)
        append_hex8(line, sizeof(line), &used, (uint32_t)result);
    append_text(line, sizeof(line), &used, " connected_mask=0x");
    append_hex8(line, sizeof(line), &used, snapshot->connected_mask);
    append_text(line, sizeof(line), &used, " connected_count=");
    append_u32(line, sizeof(line), &used, snapshot->connected_count);
    append_text(line, sizeof(line), &used, " read_error_mask=0x");
    append_hex8(line, sizeof(line), &used, snapshot->read_error_mask);
    append_text(line, sizeof(line), &used, " valid_mask=0x");
    append_hex8(line, sizeof(line), &used, frame->valid_mask);
    append_text(line, sizeof(line), &used, " frame_connected_mask=0x");
    append_hex8(line, sizeof(line), &used, frame->connected_mask);
    append_char(line, sizeof(line), &used, '\n');
    (void)write_all(fd, line, used);
}

static void write_player_line(
    int fd,
    uint32_t sample,
    uint32_t port,
    const GhcLocalInputSnapshot *snapshot,
    const GhcLocalInputFrame *frame
)
{
    const GhcLocalPlayerInput *input = &snapshot->players[port];
    const GhcLocalPlayerFrame *state = &frame->players[port];
    char line[LINE_CAPACITY];
    uint32_t used = 0;
    line[0] = '\0';

    append_text(line, sizeof(line), &used, "  sample=");
    append_u32(line, sizeof(line), &used, sample);
    append_text(line, sizeof(line), &used, " port=");
    append_u32(line, sizeof(line), &used, port);
    append_text(line, sizeof(line), &used, " connected=");
    append_u32(line, sizeof(line), &used, input->connected);
    append_text(line, sizeof(line), &used, " read_error=");
    append_u32(line, sizeof(line), &used, (snapshot->read_error_mask >> port) & 1u);
    append_text(line, sizeof(line), &used, " digital1=0x");
    append_hex8(line, sizeof(line), &used, input->digital1);
    append_text(line, sizeof(line), &used, " digital2=0x");
    append_hex8(line, sizeof(line), &used, input->digital2);
    append_text(line, sizeof(line), &used, " buttons=0x");
    append_hex8(line, sizeof(line), &used, input->buttons);
    append_text(line, sizeof(line), &used, " pressed=0x");
    append_hex8(line, sizeof(line), &used, state->pressed);
    append_text(line, sizeof(line), &used, " released=0x");
    append_hex8(line, sizeof(line), &used, state->released);
    append_text(line, sizeof(line), &used, " left_x=");
    append_u32(line, sizeof(line), &used, input->left_x);
    append_text(line, sizeof(line), &used, " left_y=");
    append_u32(line, sizeof(line), &used, input->left_y);
    append_text(line, sizeof(line), &used, " right_x=");
    append_u32(line, sizeof(line), &used, input->right_x);
    append_text(line, sizeof(line), &used, " right_y=");
    append_u32(line, sizeof(line), &used, input->right_y);
    append_text(line, sizeof(line), &used, " norm_left_x=");
    append_i32(line, sizeof(line), &used, state->left_x);
    append_text(line, sizeof(line), &used, " norm_left_y=");
    append_i32(line, sizeof(line), &used, state->left_y);
    append_text(line, sizeof(line), &used, " norm_right_x=");
    append_i32(line, sizeof(line), &used, state->right_x);
    append_text(line, sizeof(line), &used, " norm_right_y=");
    append_i32(line, sizeof(line), &used, state->right_y);
    append_text(line, sizeof(line), &used, " l2_pressure=");
    append_u32(line, sizeof(line), &used, input->l2_pressure);
    append_text(line, sizeof(line), &used, " r2_pressure=");
    append_u32(line, sizeof(line), &used, input->r2_pressure);
    append_char(line, sizeof(line), &used, '\n');
    (void)write_all(fd, line, used);
}

static void pad_input_probe_thread(uint64_t arg)
{
    int fd;
    uint32_t sample;
    GhcLocalInputTracker tracker;
    GhcLocalInputFrame frame;
    (void)arg;

    ghc_local_input_tracker_reset(&tracker);
    zero_bytes(&frame, sizeof(frame));

    fd = open_log(0);
    if (fd < 0) {
        sys_ppu_thread_exit(1u);
        return;
    }

    for (sample = 0; sample < PROBE_SAMPLES && !g_stop; ++sample) {
        GhcLocalInputSnapshot snapshot;
        int32_t result;
        ghc_local_input_snapshot_clear(&snapshot);
        result = ghc_local_input_poll_ps3(&snapshot);
        if (result == GHC_LOCAL_INPUT_OK) {
            (void)ghc_local_input_tracker_update(
                &snapshot, &tracker, GHC_LOCAL_STICK_DEADZONE_DEFAULT, &frame
            );
        }
        write_sample_line(fd, sample, result, &snapshot, &frame);
        write_player_line(fd, sample, 0u, &snapshot, &frame);
        write_player_line(fd, sample, 1u, &snapshot, &frame);
        sys_timer_usleep(PROBE_INTERVAL_US);
    }

    log_literal(fd, "Pad input probe finished.\n");
    cellFsClose(fd);
    sys_ppu_thread_exit(0u);
}

int ghosts_input_probe_ping(void)
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
    log_literal(fd, "Ghosts DualShock Input Probe v1\nMODE=");
    log_literal(fd, g_mode_name);
    log_literal(fd, "\nREAD_ONLY=1\nMODE_SOURCE=manual_mode_file\nMODE_LABEL_IS_OPERATOR_SELECTED=1\n");
    log_literal(fd, "CALLS=cellPadGetInfo2,cellPadGetData\nNO_CELL_PAD_INIT_END=1\nNO_GAME_MEMORY_WRITES=1\nNO_HOOKS=1\n");
    log_literal(fd, "Samples: 30 at one-second intervals; ports 0 and 1 only.\n");
    log_literal(fd, "Hold different buttons on both controllers during the sample window to test independent port data.\n\n");
    cellFsClose(fd);

    result = sys_ppu_thread_create(
        &g_thread, pad_input_probe_thread, 0u, PROBE_PRIORITY, PROBE_STACK_SIZE,
        THREAD_JOINABLE, "GhostsPadInputProbe"
    );
    if (result == 0) {
        g_thread_started = 1;
    } else {
        fd = open_log(0);
        if (fd >= 0) {
            char line[LINE_CAPACITY];
            uint32_t used = 0;
            line[0] = '\0';
            append_text(line, sizeof(line), &used, "thread_create_error=0x");
            append_hex8(line, sizeof(line), &used, (uint32_t)result);
            append_char(line, sizeof(line), &used, '\n');
            (void)write_all(fd, line, used);
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
        int join_status = thread_join(g_thread, &thread_result);
        if (join_status != 0)
            return SYS_PRX_STOP_FAIL;
        g_thread_started = 0;
    }
    return SYS_PRX_STOP_OK;
}
