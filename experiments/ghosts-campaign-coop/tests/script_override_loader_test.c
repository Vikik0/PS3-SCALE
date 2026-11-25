#include "../client/ps3/prx/script_override.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_FILE_CAPACITY 65537u
#define TEST_FD 7

static uint8_t g_file[TEST_FILE_CAPACITY];
static uint64_t g_file_length;
static uint64_t g_file_offset;
static int32_t g_open_result;
static int32_t g_fail_read_call;
static int32_t g_read_calls;
static int32_t g_close_calls;

static void reset_fs(void)
{
    memset(g_file, 0, sizeof(g_file));
    g_file_length = 0u;
    g_file_offset = 0u;
    g_open_result = CELL_FS_SUCCEEDED;
    g_fail_read_call = 0;
    g_read_calls = 0;
    g_close_calls = 0;
}

static void write_le32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

static void prepare_valid_blob(void)
{
    reset_fs();
    g_file[0] = 'G';
    g_file[1] = 'S';
    g_file[2] = 'C';
    g_file[3] = 0u;
    write_le32(&g_file[4], 4u);
    write_le32(&g_file[8], 64u);
    write_le32(&g_file[12], 3u);
    memcpy(&g_file[16], "ABCDxyz", 7u);
    g_file_length = 23u;
}

int32_t cellFsOpen(
    const char *path,
    int flags,
    int *fd,
    const void *arg,
    uint64_t size
)
{
    (void)arg;
    (void)size;
    assert(path != NULL);
    assert(strcmp(path, "/dev_hdd0/tmp/ghosts/maps__clockwork_interior.gscbin") == 0);
    assert(flags == CELL_FS_O_RDONLY);
    if (g_open_result != CELL_FS_SUCCEEDED)
        return g_open_result;
    assert(fd != NULL);
    *fd = TEST_FD;
    return CELL_FS_SUCCEEDED;
}

int32_t cellFsRead(int fd, void *buffer, uint64_t bytes_to_read, uint64_t *bytes_read)
{
    uint64_t remaining;
    uint64_t count;
    assert(fd == TEST_FD);
    assert(buffer != NULL);
    assert(bytes_read != NULL);
    g_read_calls++;
    if (g_fail_read_call == g_read_calls)
        return -77;

    if (g_file_offset >= g_file_length) {
        *bytes_read = 0u;
        return CELL_FS_SUCCEEDED;
    }

    remaining = g_file_length - g_file_offset;
    count = bytes_to_read < remaining ? bytes_to_read : remaining;
    memcpy(buffer, &g_file[g_file_offset], (size_t)count);
    g_file_offset += count;
    *bytes_read = count;
    return CELL_FS_SUCCEEDED;
}

int32_t cellFsClose(int fd)
{
    assert(fd == TEST_FD);
    g_close_calls++;
    return CELL_FS_SUCCEEDED;
}

static void assert_empty_state(void)
{
    assert(ghc_script_override_comp_len() == 0u);
    assert(ghc_script_override_stack_len() == 0u);
    assert(ghc_script_override_bytecode_len() == 0u);
    assert(ghc_script_override_total_len() == 0u);
    assert(ghc_script_override_hash32() == 0u);
    assert(ghc_script_override_blob_address() == 0u);
    assert(ghc_script_override_stack_address() == 0u);
    assert(ghc_script_override_bytecode_address() == 0u);
}

static void test_valid_file_loads_and_exposes_diagnostics(void)
{
    prepare_valid_blob();
    assert(ghc_script_override_reload() == GHC_SCRIPT_OVERRIDE_READY);
    assert(ghc_script_override_status() == GHC_SCRIPT_OVERRIDE_READY);
    assert(ghc_script_override_comp_len() == 4u);
    assert(ghc_script_override_stack_len() == 64u);
    assert(ghc_script_override_bytecode_len() == 3u);
    assert(ghc_script_override_total_len() == 23u);
    assert(ghc_script_override_hash32() == 0x131cc566u);
    assert(ghc_script_override_blob_address() != 0u);
    assert(ghc_script_override_stack_address() != 0u);
    assert(ghc_script_override_bytecode_address() != 0u);
    assert(g_read_calls == 1);
    assert(g_close_calls == 1);
}

static void test_exact_capacity_file_performs_clean_eof_probe(void)
{
    reset_fs();
    g_file[0] = 'G';
    g_file[1] = 'S';
    g_file[2] = 'C';
    g_file[3] = 0u;
    write_le32(&g_file[4], 65504u);
    write_le32(&g_file[8], 1u);
    write_le32(&g_file[12], 16u);
    g_file_length = 65536u;

    assert(ghc_script_override_reload() == GHC_SCRIPT_OVERRIDE_READY);
    assert(ghc_script_override_total_len() == 65536u);
    assert(g_read_calls == 2);
    assert(g_close_calls == 1);
}

static void test_open_failure_reports_status_and_keeps_blob_hidden(void)
{
    prepare_valid_blob();
    g_open_result = -11;
    assert(ghc_script_override_reload() == GHC_SCRIPT_OVERRIDE_OPEN_FAILED);
    assert(ghc_script_override_status() == GHC_SCRIPT_OVERRIDE_OPEN_FAILED);
    assert_empty_state();
    assert(g_read_calls == 0);
    assert(g_close_calls == 0);
}

static void test_first_read_failure_closes_file_and_clears_state(void)
{
    prepare_valid_blob();
    g_fail_read_call = 1;
    assert(ghc_script_override_reload() == GHC_SCRIPT_OVERRIDE_READ_FAILED);
    assert(ghc_script_override_status() == GHC_SCRIPT_OVERRIDE_READ_FAILED);
    assert_empty_state();
    assert(g_close_calls == 1);
}

static void test_extra_read_failure_is_reported(void)
{
    reset_fs();
    g_file_length = 65536u;
    g_fail_read_call = 2;
    assert(ghc_script_override_reload() == GHC_SCRIPT_OVERRIDE_READ_FAILED);
    assert(ghc_script_override_status() == GHC_SCRIPT_OVERRIDE_READ_FAILED);
    assert(g_read_calls == 2);
    assert(g_close_calls == 1);
    assert_empty_state();
}

static void test_oversized_file_is_rejected(void)
{
    reset_fs();
    g_file_length = 65537u;
    assert(ghc_script_override_reload() == GHC_SCRIPT_OVERRIDE_TOO_LARGE);
    assert(ghc_script_override_status() == GHC_SCRIPT_OVERRIDE_TOO_LARGE);
    assert(g_read_calls == 2);
    assert(g_close_calls == 1);
    assert(ghc_script_override_blob_address() == 0u);
}

static void test_bad_magic_is_rejected(void)
{
    prepare_valid_blob();
    g_file[0] = 'X';
    assert(ghc_script_override_reload() == GHC_SCRIPT_OVERRIDE_BAD_MAGIC);
    assert(ghc_script_override_status() == GHC_SCRIPT_OVERRIDE_BAD_MAGIC);
    assert_empty_state();
}

static void test_bad_lengths_are_rejected(void)
{
    prepare_valid_blob();
    write_le32(&g_file[4], 0xffffffffu);
    assert(ghc_script_override_reload() == GHC_SCRIPT_OVERRIDE_BAD_LENGTHS);
    assert(ghc_script_override_status() == GHC_SCRIPT_OVERRIDE_BAD_LENGTHS);
    assert_empty_state();
}

static void test_failed_reload_clears_previous_ready_snapshot(void)
{
    prepare_valid_blob();
    assert(ghc_script_override_reload() == GHC_SCRIPT_OVERRIDE_READY);
    g_open_result = -11;
    assert(ghc_script_override_reload() == GHC_SCRIPT_OVERRIDE_OPEN_FAILED);
    assert_empty_state();
}

int main(void)
{
    test_valid_file_loads_and_exposes_diagnostics();
    test_exact_capacity_file_performs_clean_eof_probe();
    test_open_failure_reports_status_and_keeps_blob_hidden();
    test_first_read_failure_closes_file_and_clears_state();
    test_extra_read_failure_is_reported();
    test_oversized_file_is_rejected();
    test_bad_magic_is_rejected();
    test_bad_lengths_are_rejected();
    test_failed_reload_clears_previous_ready_snapshot();
    puts("GSC wrapper loader tests: 9/9 PASS");
    return 0;
}
