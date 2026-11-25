#include "../client/ps3/prx/script_override_format.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t blob[64];

static void write_le32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}

static uint32_t make_valid_blob(void)
{
    memset(blob, 0, sizeof(blob));
    blob[0] = 'G';
    blob[1] = 'S';
    blob[2] = 'C';
    blob[3] = 0u;
    write_le32(&blob[4], 4u);
    write_le32(&blob[8], 64u);
    write_le32(&blob[12], 3u);
    memcpy(&blob[16], "ABCDxyz", 7u);
    return 23u;
}

static void assert_info_zero(const GhcScriptOverrideBlobInfo *info)
{
    assert(info->comp_len == 0u);
    assert(info->stack_len == 0u);
    assert(info->bytecode_len == 0u);
    assert(info->total_len == 0u);
    assert(info->hash32 == 0u);
}

static void test_valid_blob_reports_exact_segments_and_hash(void)
{
    GhcScriptOverrideBlobInfo info;
    uint32_t length = make_valid_blob();
    assert(ghc_script_override_inspect_blob(blob, length, sizeof(blob), &info) == GHC_SCRIPT_OVERRIDE_READY);
    assert(info.comp_len == 4u);
    assert(info.stack_len == 64u);
    assert(info.bytecode_len == 3u);
    assert(info.total_len == 23u);
    assert(info.hash32 == 0x131cc566u);
}

static void test_exact_capacity_boundary_is_valid(void)
{
    GhcScriptOverrideBlobInfo info;
    uint32_t length = make_valid_blob();
    assert(ghc_script_override_inspect_blob(blob, length, length, &info) == GHC_SCRIPT_OVERRIDE_READY);
    assert(info.total_len == length);
}

static void test_null_arguments_fail_closed(void)
{
    GhcScriptOverrideBlobInfo info;
    uint32_t length = make_valid_blob();
    assert(ghc_script_override_inspect_blob(NULL, length, sizeof(blob), &info) ==
           GHC_SCRIPT_OVERRIDE_INVALID_ARGUMENT);
    assert(ghc_script_override_inspect_blob(blob, length, sizeof(blob), NULL) ==
           GHC_SCRIPT_OVERRIDE_INVALID_ARGUMENT);
}

static void test_input_larger_than_capacity_is_rejected(void)
{
    GhcScriptOverrideBlobInfo info;
    uint32_t length = make_valid_blob();
    assert(ghc_script_override_inspect_blob(blob, length, length - 1u, &info) ==
           GHC_SCRIPT_OVERRIDE_TOO_LARGE);
    assert_info_zero(&info);
}

static void test_short_blob_and_bad_magic_are_rejected(void)
{
    GhcScriptOverrideBlobInfo info;
    uint32_t length = make_valid_blob();
    assert(ghc_script_override_inspect_blob(blob, 15u, sizeof(blob), &info) ==
           GHC_SCRIPT_OVERRIDE_BAD_MAGIC);
    blob[0] = 'X';
    assert(ghc_script_override_inspect_blob(blob, length, sizeof(blob), &info) ==
           GHC_SCRIPT_OVERRIDE_BAD_MAGIC);
    assert_info_zero(&info);
}

static void test_each_zero_length_field_is_rejected(void)
{
    GhcScriptOverrideBlobInfo info;
    uint32_t length = make_valid_blob();
    write_le32(&blob[4], 0u);
    assert(ghc_script_override_inspect_blob(blob, length, sizeof(blob), &info) ==
           GHC_SCRIPT_OVERRIDE_BAD_LENGTHS);
    length = make_valid_blob();
    write_le32(&blob[8], 0u);
    assert(ghc_script_override_inspect_blob(blob, length, sizeof(blob), &info) ==
           GHC_SCRIPT_OVERRIDE_BAD_LENGTHS);
    length = make_valid_blob();
    write_le32(&blob[12], 0u);
    assert(ghc_script_override_inspect_blob(blob, length, sizeof(blob), &info) ==
           GHC_SCRIPT_OVERRIDE_BAD_LENGTHS);
}

static void test_compressed_segment_cannot_exceed_capacity(void)
{
    GhcScriptOverrideBlobInfo info;
    uint32_t length = make_valid_blob();
    write_le32(&blob[4], 0xffffffffu);
    assert(ghc_script_override_inspect_blob(blob, length, sizeof(blob), &info) ==
           GHC_SCRIPT_OVERRIDE_BAD_LENGTHS);
}

static void test_bytecode_segment_cannot_exceed_remaining_capacity(void)
{
    GhcScriptOverrideBlobInfo info;
    uint32_t length = make_valid_blob();
    write_le32(&blob[12], 0xffffffffu);
    assert(ghc_script_override_inspect_blob(blob, length, sizeof(blob), &info) ==
           GHC_SCRIPT_OVERRIDE_BAD_LENGTHS);
}

static void test_declared_total_must_match_bytes_read(void)
{
    GhcScriptOverrideBlobInfo info;
    uint32_t length = make_valid_blob();
    assert(ghc_script_override_inspect_blob(blob, length - 1u, sizeof(blob), &info) ==
           GHC_SCRIPT_OVERRIDE_BAD_LENGTHS);
    assert_info_zero(&info);
}

static void test_capacity_smaller_than_wrapper_header_is_rejected(void)
{
    GhcScriptOverrideBlobInfo info;
    uint32_t length = make_valid_blob();
    assert(ghc_script_override_inspect_blob(blob, length, 8u, &info) ==
           GHC_SCRIPT_OVERRIDE_TOO_LARGE);
}

int main(void)
{
    test_valid_blob_reports_exact_segments_and_hash();
    test_exact_capacity_boundary_is_valid();
    test_null_arguments_fail_closed();
    test_input_larger_than_capacity_is_rejected();
    test_short_blob_and_bad_magic_are_rejected();
    test_each_zero_length_field_is_rejected();
    test_compressed_segment_cannot_exceed_capacity();
    test_bytecode_segment_cannot_exceed_remaining_capacity();
    test_declared_total_must_match_bytes_read();
    test_capacity_smaller_than_wrapper_header_is_rejected();
    puts("GSC wrapper format tests: 10/10 PASS");
    return 0;
}
