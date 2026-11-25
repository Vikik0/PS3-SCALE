#ifndef GHC_SCRIPT_OVERRIDE_H
#define GHC_SCRIPT_OVERRIDE_H

#include <stdint.h>

enum {
    GHC_SCRIPT_OVERRIDE_EMPTY = 0u,
    GHC_SCRIPT_OVERRIDE_READY = 1u,
    GHC_SCRIPT_OVERRIDE_OPEN_FAILED = 2u,
    GHC_SCRIPT_OVERRIDE_READ_FAILED = 3u,
    GHC_SCRIPT_OVERRIDE_BAD_MAGIC = 4u,
    GHC_SCRIPT_OVERRIDE_BAD_LENGTHS = 5u,
    GHC_SCRIPT_OVERRIDE_TOO_LARGE = 6u,
    GHC_SCRIPT_OVERRIDE_INVALID_ARGUMENT = 7u
};

int32_t ghc_script_override_reload(void);
uint32_t ghc_script_override_status(void);
uint32_t ghc_script_override_comp_len(void);
uint32_t ghc_script_override_stack_len(void);
uint32_t ghc_script_override_bytecode_len(void);
uint32_t ghc_script_override_total_len(void);
uint32_t ghc_script_override_hash32(void);
uint64_t ghc_script_override_blob_address(void);
uint64_t ghc_script_override_stack_address(void);
uint64_t ghc_script_override_bytecode_address(void);

#endif
