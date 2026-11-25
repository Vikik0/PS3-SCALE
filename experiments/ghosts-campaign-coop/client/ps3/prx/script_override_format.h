#ifndef GHC_SCRIPT_OVERRIDE_FORMAT_H
#define GHC_SCRIPT_OVERRIDE_FORMAT_H

#include <stdint.h>
#include "script_override.h"

#define GHC_SCRIPT_OVERRIDE_GSC_HEADER_SIZE 16u

typedef struct {
    uint32_t comp_len;
    uint32_t stack_len;
    uint32_t bytecode_len;
    uint32_t total_len;
    uint32_t hash32;
} GhcScriptOverrideBlobInfo;

int32_t ghc_script_override_inspect_blob(
    const uint8_t *bytes,
    uint32_t bytes_read,
    uint32_t capacity,
    GhcScriptOverrideBlobInfo *info
);

#endif
