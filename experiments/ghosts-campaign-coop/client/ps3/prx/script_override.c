/* Read-only loader for a compiled GSC Tool wrapper.
 * This prepares a stable blob for a later, verified ScriptFile lookup hook.
 * It does not patch game memory or construct an engine ScriptFile structure.
 */
#include <stdint.h>
#include <stddef.h>
#include <cell/fs/cell_fs_file_api.h>
#include "script_override.h"
#include "script_override_format.h"

#define GHC_SCRIPT_GSCBIN_PATH "/dev_hdd0/tmp/ghosts/maps__clockwork_interior.gscbin"
#define GHC_SCRIPT_GSCBIN_CAPACITY 65536u

static uint8_t g_gscbin[GHC_SCRIPT_GSCBIN_CAPACITY] __attribute__((aligned(16)));
static volatile uint32_t g_status = GHC_SCRIPT_OVERRIDE_EMPTY;
static uint32_t g_comp_len;
static uint32_t g_stack_len;
static uint32_t g_bytecode_len;
static uint32_t g_total_len;
static uint32_t g_hash32;

static void clear_blob(void)
{
    uint32_t i;
    for (i = 0; i < GHC_SCRIPT_GSCBIN_CAPACITY; ++i)
        g_gscbin[i] = 0u;
    g_comp_len = 0u;
    g_stack_len = 0u;
    g_bytecode_len = 0u;
    g_total_len = 0u;
    g_hash32 = 0u;
}

int32_t ghc_script_override_reload(void)
{
    int fd = -1;
    uint64_t bytes_read = 0u;
    uint64_t extra_read = 0u;
    uint8_t extra_byte = 0u;
    GhcScriptOverrideBlobInfo blob_info;
    int32_t validation;
    int32_t result;

    clear_blob();
    g_status = GHC_SCRIPT_OVERRIDE_EMPTY;

    result = cellFsOpen(GHC_SCRIPT_GSCBIN_PATH, CELL_FS_O_RDONLY, &fd, NULL, 0);
    if (result != CELL_FS_SUCCEEDED) {
        g_status = GHC_SCRIPT_OVERRIDE_OPEN_FAILED;
        return (int32_t)g_status;
    }

    result = cellFsRead(fd, g_gscbin, sizeof(g_gscbin), &bytes_read);
    if (result != CELL_FS_SUCCEEDED) {
        (void)cellFsClose(fd);
        g_status = GHC_SCRIPT_OVERRIDE_READ_FAILED;
        return (int32_t)g_status;
    }

    if (bytes_read == sizeof(g_gscbin)) {
        result = cellFsRead(fd, &extra_byte, 1u, &extra_read);
        if (result != CELL_FS_SUCCEEDED) {
            (void)cellFsClose(fd);
            g_status = GHC_SCRIPT_OVERRIDE_READ_FAILED;
            return (int32_t)g_status;
        }
        if (extra_read != 0u) {
            (void)cellFsClose(fd);
            g_status = GHC_SCRIPT_OVERRIDE_TOO_LARGE;
            return (int32_t)g_status;
        }
    }
    (void)cellFsClose(fd);

    validation = ghc_script_override_inspect_blob(
        g_gscbin,
        (uint32_t)bytes_read,
        GHC_SCRIPT_GSCBIN_CAPACITY,
        &blob_info
    );
    if (validation != GHC_SCRIPT_OVERRIDE_READY) {
        clear_blob();
        g_status = (uint32_t)validation;
        return (int32_t)g_status;
    }

    g_comp_len = blob_info.comp_len;
    g_stack_len = blob_info.stack_len;
    g_bytecode_len = blob_info.bytecode_len;
    g_total_len = blob_info.total_len;
    g_hash32 = blob_info.hash32;
    g_status = GHC_SCRIPT_OVERRIDE_READY;
    return (int32_t)g_status;
}

uint32_t ghc_script_override_status(void) { return g_status; }
uint32_t ghc_script_override_comp_len(void) { return g_comp_len; }
uint32_t ghc_script_override_stack_len(void) { return g_stack_len; }
uint32_t ghc_script_override_bytecode_len(void) { return g_bytecode_len; }
uint32_t ghc_script_override_total_len(void) { return g_total_len; }
uint32_t ghc_script_override_hash32(void) { return g_hash32; }

uint64_t ghc_script_override_blob_address(void)
{
    return (g_status == GHC_SCRIPT_OVERRIDE_READY) ? (uint64_t)(uintptr_t)g_gscbin : 0u;
}

uint64_t ghc_script_override_stack_address(void)
{
    return (g_status == GHC_SCRIPT_OVERRIDE_READY)
        ? (uint64_t)(uintptr_t)&g_gscbin[GHC_SCRIPT_OVERRIDE_GSC_HEADER_SIZE] : 0u;
}

uint64_t ghc_script_override_bytecode_address(void)
{
    return (g_status == GHC_SCRIPT_OVERRIDE_READY)
        ? (uint64_t)(uintptr_t)&g_gscbin[GHC_SCRIPT_OVERRIDE_GSC_HEADER_SIZE + g_comp_len] : 0u;
}
