/*
 * Crash Bash Enhanced VSH-side process loader.
 *
 * Load this small PRX into VSH using Cobra/PS3MAPI. It watches the process
 * table and injects crash_bash_input.fake.sprx only into Sony's PS1 emulator
 * processes. The input bridge itself remains process-local to the emulator.
 *
 * PS3MAPI syscall8 opcodes are public Cobra/Mamba interfaces:
 *   0x7777 / 0x0021  enumerate process IDs
 *   0x7777 / 0x0022  get process name
 *   0x7777 / 0x0044  load a PRX into a target process
 */
#include <stdint.h>
#include <string.h>

#include <ppu-lv2.h>
#include <sys/thread.h>
#include <sys/timer.h>

#define SC_COBRA_SYSCALL8 8u
#define PS3MAPI_MAGIC 0x7777u
#define PS3MAPI_GET_ALL_PROC_PID 0x0021u
#define PS3MAPI_GET_PROC_NAME_BY_PID 0x0022u
#define PS3MAPI_LOAD_PROC_MODULE 0x0044u
#define PS3MAPI_MAX_PROCESS 16u

#ifndef CB_INPUT_PRX_PATH
#define CB_INPUT_PRX_PATH "/dev_hdd0/game/CBAS00001/USRDIR/crash_bash_input.fake.sprx"
#endif

#define LOADER_POLL_US 500000u
#define LOADER_THREAD_PRIORITY 1200
#define LOADER_THREAD_STACK (64u * 1024u)

static volatile int loader_running;
static volatile int loader_thread_exited;
static sys_ppu_thread_t loader_thread_id;
static uint32_t attempted_pid;

static int ps3mapi_get_all_processes_pid(uint32_t pids[PS3MAPI_MAX_PROCESS])
{
    lv2syscall3(SC_COBRA_SYSCALL8, PS3MAPI_MAGIC,
                PS3MAPI_GET_ALL_PROC_PID, (u64)(uint32_t)pids);
    return (int)p1;
}

static int ps3mapi_get_process_name_by_pid(uint32_t pid, char *name)
{
    lv2syscall4(SC_COBRA_SYSCALL8, PS3MAPI_MAGIC,
                PS3MAPI_GET_PROC_NAME_BY_PID, (u64)pid,
                (u64)(uint32_t)name);
    return (int)p1;
}

static int ps3mapi_load_process_module(uint32_t pid, const char *path)
{
    lv2syscall6(SC_COBRA_SYSCALL8, PS3MAPI_MAGIC,
                PS3MAPI_LOAD_PROC_MODULE, (u64)pid,
                (u64)(uint32_t)path, 0, 0);
    return (int)p1;
}

static int contains_text(const char *text, const char *needle)
{
    size_t needle_length;
    const char *start;

    if (!text || !needle || !*needle) return 0;
    needle_length = strlen(needle);

    for (start = text; *start; ++start) {
        size_t i;
        for (i = 0; i < needle_length; ++i) {
            if (start[i] == '\0' || start[i] != needle[i]) break;
        }
        if (i == needle_length) return 1;
    }

    return 0;
}

static int is_ps1_emulator(const char *name)
{
    return contains_text(name, "ps1_netemu") ||
           contains_text(name, "ps1_emu") ||
           contains_text(name, "ps1_newemu");
}

static void loader_scan_once(void)
{
    uint32_t pids[PS3MAPI_MAX_PROCESS];
    uint32_t target_pid = 0;
    int previous_pid_seen = 0;
    unsigned int i;
    int result;

    memset(pids, 0, sizeof(pids));
    result = ps3mapi_get_all_processes_pid(pids);
    if (result < 0) {
        attempted_pid = 0;
        return;
    }

    for (i = 0; i < PS3MAPI_MAX_PROCESS; ++i) {
        char name[32];
        uint32_t pid = pids[i];

        if (pid <= 2u) continue;
        if (pid == attempted_pid) previous_pid_seen = 1;

        memset(name, 0, sizeof(name));
        if (ps3mapi_get_process_name_by_pid(pid, name) < 0) continue;
        name[sizeof(name) - 1u] = '\0';

        if (is_ps1_emulator(name)) {
            target_pid = pid;
            break;
        }
    }

    if (!previous_pid_seen) attempted_pid = 0;
    if (target_pid == 0 || target_pid == attempted_pid) return;

    result = ps3mapi_load_process_module(target_pid, CB_INPUT_PRX_PATH);
    if (result >= 0) {
        attempted_pid = target_pid;
    }
}

static void loader_thread(void *argument)
{
    (void)argument;

    while (loader_running) {
        loader_scan_once();
        sys_timer_usleep(LOADER_POLL_US);
    }

    loader_thread_exited = 1;
}

extern void __libc_init_array(void);
extern void __libc_fini_array(void);
static int runtime_initialized;

int module_start(size_t args, void *argp)
{
    int result;

    (void)args;
    (void)argp;

    if (!runtime_initialized) {
        __libc_init_array();
        runtime_initialized = 1;
    }

    loader_running = 1;
    loader_thread_exited = 0;
    attempted_pid = 0;

    result = sysThreadCreate(&loader_thread_id, loader_thread, NULL,
                             LOADER_THREAD_PRIORITY, LOADER_THREAD_STACK,
                             THREAD_JOINABLE, "CrashBashLoader");
    if (result != 0) {
        loader_running = 0;
        if (runtime_initialized) {
            __libc_fini_array();
            runtime_initialized = 0;
        }
        return -1;
    }

    return 0;
}

int module_stop(size_t args, void *argp)
{
    (void)args;
    (void)argp;

    loader_running = 0;
    (void)sysThreadJoin(loader_thread_id, NULL);

    if (runtime_initialized) {
        __libc_fini_array();
        runtime_initialized = 0;
    }

    return 0;
}
