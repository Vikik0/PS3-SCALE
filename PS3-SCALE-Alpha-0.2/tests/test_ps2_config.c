#include "ps3scale/ps2_config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const uint8_t expected_all[] = {
        0x3D, 0x00, 0x00, 0x00, 0x57, 0x44, 0x00, 0x00,
        0x44, 0x00, 0x00, 0x00,
        0x46, 0x00, 0x00, 0x00,
        0x48, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x20, 0x4E, 0x00, 0x00,
        0x50, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    static const uint8_t expected_frame_pacing[] = {
        0x3D, 0x00, 0x00, 0x00, 0x57, 0x44, 0x00, 0x00,
        0x48, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x20, 0x4E, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    static const uint8_t expected_pressure[] = {
        0x3D, 0x00, 0x00, 0x00, 0x57, 0x44, 0x00, 0x00,
        0x50, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    static const uint8_t expected_patch[] = {
        0x3D, 0x00, 0x00, 0x00, 0x57, 0x44, 0x00, 0x00,
        0x0A, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00,
        0x60, 0x18, 0x16, 0x00,
        0x01, 0x02, 0x03, 0x04,
        0x00, 0x00, 0x62, 0x2C,
        0x00, 0x00, 0x00, 0x00
    };
    uint8_t data[128];
    size_t size = 0;
    ps3scale_ps2_code_patch_t patch = {
        0x00161860u, 0x04030201u, 0x2C620000u
    };

    assert(ps3scale_ps2_config_build(
        data, sizeof(data),
        PS3SCALE_PS2_CFG_DISABLE_SMOOTHING |
        PS3SCALE_PS2_CFG_ENABLE_L2H |
        PS3SCALE_PS2_CFG_FRAME_PACING |
        PS3SCALE_PS2_CFG_ENABLE_PRESSURE,
        &size) == 0);
    assert(size == sizeof(expected_all));
    assert(memcmp(data, expected_all, sizeof(expected_all)) == 0);
    assert(ps3scale_ps2_config_validate_generated(data, size) == 0);

    assert(ps3scale_ps2_config_build(
        data, sizeof(data), PS3SCALE_PS2_CFG_ENABLE_PRESSURE, &size) == 0);
    assert(size == sizeof(expected_pressure));
    assert(memcmp(data, expected_pressure, sizeof(expected_pressure)) == 0);
    assert(ps3scale_ps2_config_validate_generated(data, size) == 0);

    assert(ps3scale_ps2_config_build(
        data, sizeof(data),
        PS3SCALE_PS2_CFG_FRAME_PACING, &size) == 0);
    assert(size == sizeof(expected_frame_pacing));
    assert(memcmp(data, expected_frame_pacing,
                  sizeof(expected_frame_pacing)) == 0);
    assert(ps3scale_ps2_config_validate_generated(data, size) == 0);

    assert(ps3scale_ps2_config_build_with_patches(
        data, sizeof(data), 0, &patch, 1, &size) == 0);
    assert(size == sizeof(expected_patch));
    assert(memcmp(data, expected_patch, sizeof(expected_patch)) == 0);
    assert(ps3scale_ps2_config_validate_generated(data, size) == 0);

    {
        uint8_t base[64];
        uint8_t merged[128];
        uint8_t patch_base[64];
        uint8_t patch_merged[128];
        size_t base_size = 0;
        size_t merged_size = 0;
        size_t patch_base_size = 0;
        size_t patch_merged_size = 0;
        ps3scale_ps2_code_patch_t second_patch = {
            0x00161864u, 0x11223344u, 0x55667788u
        };

        assert(ps3scale_ps2_config_build(
            base, sizeof(base), PS3SCALE_PS2_CFG_ENABLE_L2H,
            &base_size) == 0);
        assert(ps3scale_ps2_config_append_patches(
            base, base_size, &patch, 1, merged, sizeof(merged),
            &merged_size) == 0);
        assert(merged_size == base_size + 20u);
        assert(memcmp(merged, base, base_size - 4u) == 0);
        assert(ps3scale_ps2_config_validate_generated(
            merged, merged_size) == 0);
        assert(ps3scale_ps2_config_append_patches(
            merged, merged_size, &patch, 1, base, sizeof(base),
            &base_size) == -6);

        assert(ps3scale_ps2_config_build_with_patches(
            patch_base, sizeof(patch_base), PS3SCALE_PS2_CFG_ENABLE_L2H,
            &patch, 1, &patch_base_size) == 0);
        assert(ps3scale_ps2_config_append_patches(
            patch_base, patch_base_size, &second_patch, 1,
            patch_merged, sizeof(patch_merged), &patch_merged_size) == 0);
        assert(patch_merged_size == patch_base_size + 12u);
        assert(ps3scale_ps2_config_validate_generated(
            patch_merged, patch_merged_size) == 0);
        assert(patch_merged[12] == 0x0A && patch_merged[16] == 0x02);
        assert(patch_merged[20] == 0x60 && patch_merged[21] == 0x18);
        assert(patch_merged[32] == 0x64 && patch_merged[33] == 0x18);
        {
            uint8_t already_output[128];
            size_t already_size = 0;
            memset(already_output, 0xA5, sizeof(already_output));
            assert(ps3scale_ps2_config_append_patches(
                patch_merged, patch_merged_size, &second_patch, 1,
                already_output, sizeof(already_output), &already_size) == -6);
            assert(already_size == 0);
            assert(already_output[0] == 0xA5 &&
                   already_output[sizeof(already_output) - 1u] == 0xA5);
        }
        {
            ps3scale_ps2_code_patch_t conflicting_patch = {
                0x00161860u, 0x04030201u, 0x2C620001u
            };
            ps3scale_ps2_code_patch_t partial_patches[2] = {
                patch, second_patch
            };
            assert(ps3scale_ps2_config_append_patches(
                patch_base, patch_base_size, &conflicting_patch, 1,
                patch_merged, sizeof(patch_merged), &patch_merged_size) == -3);
            assert(ps3scale_ps2_config_append_patches(
                patch_base, patch_base_size, partial_patches, 2,
                patch_merged, sizeof(patch_merged), &patch_merged_size) == -3);
        }

        {
            uint8_t title_id_config[64] = {
                0x3D, 0x00, 0x00, 0x00, 0x57, 0x44, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
                'S', 'L', 'U', 'S', '-', '2', '1', '2', '6', '0',
                0x00, 0x00, 0x00, 0x00
            };
            assert(ps3scale_ps2_config_append_patches(
                title_id_config, sizeof(title_id_config),
                &patch, 1, merged, sizeof(merged), &merged_size) == -2);
        }
    }

    {
        static const uint8_t community_config[] = {
            0x3D, 0x00, 0x00, 0x00, 0x57, 0x44, 0x00, 0x00,
            0x21, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        };
        uint8_t merged[128];
        size_t merged_size = 0;

        assert(ps3scale_ps2_config_append_patches(
            community_config, sizeof(community_config),
            &patch, 1, merged, sizeof(merged), &merged_size) == 0);
        assert(merged_size == sizeof(community_config) + 20u);
        assert(memcmp(merged, community_config,
                      sizeof(community_config) - 4u) == 0);
        assert(ps3scale_ps2_config_validate_external(
            merged, merged_size, "CUSTOM") == 0);

        assert(ps3scale_ps2_config_append_patches(
            community_config, sizeof(community_config) - 4u,
            &patch, 1, merged, sizeof(merged), &merged_size) == -1);
    }

    patch.ee_address = 0x00161861u;
    assert(ps3scale_ps2_config_build_with_patches(
        data, sizeof(data), 0, &patch, 1, &size) < 0);
    patch.ee_address = 0x00161860u;
    patch.original_word = patch.replacement_word;
    assert(ps3scale_ps2_config_build_with_patches(
        data, sizeof(data), 0, &patch, 1, &size) < 0);
    patch.original_word = 0x04030201u;
    patch.ee_address = 0x10161860u;
    assert(ps3scale_ps2_config_build_with_patches(
        data, sizeof(data), 0, &patch, 1, &size) < 0);

    assert(ps3scale_ps2_config_build(data, sizeof(data), 0, &size) < 0);
    assert(ps3scale_ps2_config_build(data, sizeof(data), 0x80, &size) < 0);
    assert(ps3scale_ps2_config_build(data, 8,
        PS3SCALE_PS2_CFG_DISABLE_SMOOTHING, &size) < 0);

    data[8] = 0x45;
    assert(ps3scale_ps2_config_validate_generated(data, 12) < 0);

    assert(ps3scale_ps2_config_build(
        data, sizeof(data), PS3SCALE_PS2_CFG_FRAME_PACING, &size) == 0);
    data[12] = 0x02;
    assert(ps3scale_ps2_config_validate_generated(data, size) < 0);
    assert(ps3scale_ps2_config_build(
        data, sizeof(data), PS3SCALE_PS2_CFG_FRAME_PACING, &size) == 0);
    data[16] = 0x21;
    assert(ps3scale_ps2_config_validate_generated(data, size) < 0);

    {
        static const uint8_t external_config[] = {
            0x3D, 0x00, 0x00, 0x00, 0x57, 0x44, 0x00, 0x00,
            0x44, 0x00, 0x00, 0x00
        };
        static const uint8_t external_enc[] = {
            0x50, 0x53, 0x32, 0x00, 0x01, 0x02, 0x03, 0x04,
            0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C
        };
        assert(ps3scale_ps2_config_validate_external(
            external_config, sizeof(external_config), "CUSTOM") == 0);
        assert(ps3scale_ps2_config_validate_external(
            external_enc, sizeof(external_enc), "ENC") == 0);
        assert(ps3scale_ps2_config_validate_external(
            external_config, sizeof(external_config), "UNKNOWN") < 0);
    }

    puts("PS3 SCALE PS2 CONFIG builder tests: PASS");
    return 0;
}
