#include "script_override_format.h"

static uint32_t read_le32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0])
        | ((uint32_t)bytes[1] << 8)
        | ((uint32_t)bytes[2] << 16)
        | ((uint32_t)bytes[3] << 24);
}

static uint32_t fnv1a32(const uint8_t *bytes, uint32_t length)
{
    uint32_t hash = 2166136261u;
    uint32_t i;
    for (i = 0; i < length; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static void clear_info(GhcScriptOverrideBlobInfo *info)
{
    info->comp_len = 0u;
    info->stack_len = 0u;
    info->bytecode_len = 0u;
    info->total_len = 0u;
    info->hash32 = 0u;
}

int32_t ghc_script_override_inspect_blob(
    const uint8_t *bytes,
    uint32_t bytes_read,
    uint32_t capacity,
    GhcScriptOverrideBlobInfo *info
)
{
    uint32_t comp_len;
    uint32_t stack_len;
    uint32_t bytecode_len;
    uint32_t total_len;

    if (!info)
        return GHC_SCRIPT_OVERRIDE_INVALID_ARGUMENT;

    clear_info(info);

    if (!bytes)
        return GHC_SCRIPT_OVERRIDE_INVALID_ARGUMENT;

    if (bytes_read > capacity)
        return GHC_SCRIPT_OVERRIDE_TOO_LARGE;

    if (capacity < GHC_SCRIPT_OVERRIDE_GSC_HEADER_SIZE
        || bytes_read < GHC_SCRIPT_OVERRIDE_GSC_HEADER_SIZE)
        return GHC_SCRIPT_OVERRIDE_BAD_MAGIC;

    if (bytes[0] != 'G' || bytes[1] != 'S' || bytes[2] != 'C' || bytes[3] != 0u)
        return GHC_SCRIPT_OVERRIDE_BAD_MAGIC;

    comp_len = read_le32(&bytes[4]);
    stack_len = read_le32(&bytes[8]);
    bytecode_len = read_le32(&bytes[12]);

    if (comp_len == 0u || stack_len == 0u || bytecode_len == 0u)
        return GHC_SCRIPT_OVERRIDE_BAD_LENGTHS;

    if (comp_len > capacity - GHC_SCRIPT_OVERRIDE_GSC_HEADER_SIZE)
        return GHC_SCRIPT_OVERRIDE_BAD_LENGTHS;

    if (bytecode_len > capacity - GHC_SCRIPT_OVERRIDE_GSC_HEADER_SIZE - comp_len)
        return GHC_SCRIPT_OVERRIDE_BAD_LENGTHS;

    total_len = GHC_SCRIPT_OVERRIDE_GSC_HEADER_SIZE + comp_len + bytecode_len;
    if (total_len != bytes_read)
        return GHC_SCRIPT_OVERRIDE_BAD_LENGTHS;

    info->comp_len = comp_len;
    info->stack_len = stack_len;
    info->bytecode_len = bytecode_len;
    info->total_len = total_len;
    info->hash32 = fnv1a32(bytes, total_len);
    return GHC_SCRIPT_OVERRIDE_READY;
}
