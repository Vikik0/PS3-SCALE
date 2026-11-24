import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools import verify_ghosts_tu116_inputs as verifier
from tools.ps3_self_inspect import APP_INFO, ELF64_PHDR, ELF_HEADER, EXT_HEADER, SCE_HEADER, SEGMENT_INFO
from tools.ps3_sfo_inspect import PSF_TYPE_INTEGER, PSF_TYPE_STRING


def make_sfo(entries):
    key_table, data_table, index_entries = bytearray(), bytearray(), []
    for key, fmt, value in entries:
        key_offset = len(key_table)
        key_table.extend(key.encode("ascii") + b"\0")
        if fmt == PSF_TYPE_STRING:
            raw = value.encode("utf-8") + b"\0"
        else:
            raw = struct.pack("<I", value)
        value_offset = len(data_table)
        data_table.extend(raw)
        index_entries.append((key_offset, fmt, len(raw), len(raw), value_offset))
    key_start = 20 + len(index_entries) * 16
    data_start = key_start + len(key_table)
    return (
        struct.pack("<4sIIII", b"\0PSF", 0x101, key_start, data_start, len(index_entries))
        + b"".join(struct.pack("<HHIII", *item) for item in index_entries)
        + bytes(key_table) + bytes(data_table)
    )




def make_decrypted_ppu_elf(entry_point):
    """Minimal ELF64 BE PowerPC64 image with eight PT_LOAD headers."""
    ident = b"\x7fELF" + bytes([2, 2, 1, 0]) + bytes(8)
    header = struct.pack(
        ">16sHHIQQQIHHHHHH", ident, 2, 21, 1,
        entry_point, 64, 0, 0, 64, 56, 8, 64, 0, 0,
    )
    data = bytearray(0x500)
    data[:64] = header
    for index in range(8):
        file_offset = 0x300 + index * 0x40
        virtual_address = entry_point + index * 0x1000
        program_header = struct.pack(
            ">IIQQQQQQ", 1, 5, file_offset, virtual_address,
            virtual_address, 0x40, 0x40, 0x1000,
        )
        start = 64 + index * 56
        data[start:start + 56] = program_header
        data[file_offset:file_offset + 0x40] = bytes([index + 1]) * 0x40
    return bytes(data)


def make_self(entry_point):
    data = bytearray(0x1300)
    SCE_HEADER.pack_into(data, 0, b"SCE\0", 2, 28, 1, 0x4A0, 0xA80, 0x20000)
    EXT_HEADER.pack_into(data, 0x20, 3, 0x70, 0x90, 0xD0, 0, 0x290, 0x390, 0x3C0, 0x70, 0)
    APP_INFO.pack_into(data, 0x70, 0x1010000001000003, 0x01000002, 8, 0x0001000000000000, 0)
    ident = b"\x7fELF" + bytes([2, 2, 1, 0x66]) + bytes(8)
    ELF_HEADER.pack_into(data, 0x90, ident, 2, 21, 1, entry_point, 64, 0, 0, 64, 56, 8, 64, 0, 0)
    for index in range(8):
        ELF64_PHDR.pack_into(
            data, 0xD0 + index * 56, 1, 5, 0, entry_point, entry_point,
            0x100, 0x100, 0x1000,
        )
        SEGMENT_INFO.pack_into(
            data, 0x290 + index * 32, 0xA80 + index * 0x100, 0x100,
            0x200000000, 1 if index < 5 else 0,
        )
    return bytes(data)


class VerifyGhostsTu116InputsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.disc = self.root / "disc" / "PS3_GAME"
        self.update = self.root / "update"
        (self.disc / "USRDIR").mkdir(parents=True)
        (self.update / "USRDIR").mkdir(parents=True)
        self.disc_sfo = make_sfo([
            ("TITLE_ID", PSF_TYPE_STRING, "BLUS31270"),
            ("TITLE", PSF_TYPE_STRING, "Call of Duty: Ghosts"),
            ("APP_VER", PSF_TYPE_STRING, "01.00"),
            ("CATEGORY", PSF_TYPE_STRING, "DG"),
        ])
        self.update_sfo = make_sfo([
            ("TITLE_ID", PSF_TYPE_STRING, "BLUS31270"),
            ("TITLE", PSF_TYPE_STRING, "Call of Duty: Ghosts"),
            ("APP_VER", PSF_TYPE_STRING, "01.16"),
            ("VERSION", PSF_TYPE_STRING, "01.00"),
            ("CATEGORY", PSF_TYPE_STRING, "GD"),
        ])
        (self.disc / "PARAM.SFO").write_bytes(self.disc_sfo)
        (self.update / "PARAM.SFO").write_bytes(self.update_sfo)
        self.self_bytes = {
            "EBOOT.BIN": make_self(0x20080),
            "default.self": make_self(0x7E9508),
            "default_mp.self": make_self(0xA75E38),
        }
        for name, data in self.self_bytes.items():
            (self.update / "USRDIR" / name).write_bytes(data)
        self.expected_sfo_hash = hashlib.sha256(self.update_sfo).hexdigest()
        self.expected_self = {
            name: {
                "size_bytes": len(data),
                "sha256": hashlib.sha256(data).hexdigest(),
                "entry_point": {
                    "EBOOT.BIN": "0x20080",
                    "default.self": "0x7E9508",
                    "default_mp.self": "0xA75E38",
                }[name],
            }
            for name, data in self.self_bytes.items()
        }
        self.expected_fastfiles = {}

    def tearDown(self):
        self.temp.cleanup()

    def patched_expectations(self):
        return (
            patch.object(verifier, "EXPECTED_UPDATE_SFO_SHA256", self.expected_sfo_hash),
            patch.object(verifier, "EXPECTED_SELF_FILES", self.expected_self),
            patch.object(verifier, "EXPECTED_FASTFILES", self.expected_fastfiles),
        )

    def test_verifies_exact_local_tu_profile_without_decrypting_or_uploading(self):
        patches = self.patched_expectations()
        with patches[0], patches[1], patches[2]:
            report = verifier.verify_inputs(self.disc, self.update)
        self.assertEqual(report["status"], "MATCHED_RETAIL_INPUTS")
        self.assertTrue(report["readiness"]["retail_input_identity_verified"])
        self.assertFalse(report["readiness"]["native_ppu_comparison_ready"])
        self.assertFalse(report["readiness"]["self_segments_are_decrypted_by_this_tool"])
        self.assertEqual(report["inputs"]["installed_update_self_files"]["default.self"]["encrypted_segment_indices"], [0, 1, 2, 3, 4])
        self.assertEqual(report["inputs"]["installed_update_param_sfo"]["app_version"], "01.16")

    def test_rejects_wrong_disc_title_id(self):
        wrong = make_sfo([
            ("TITLE_ID", PSF_TYPE_STRING, "BLES01945"),
            ("TITLE", PSF_TYPE_STRING, "Call of Duty: Ghosts"),
            ("APP_VER", PSF_TYPE_STRING, "01.00"),
            ("CATEGORY", PSF_TYPE_STRING, "DG"),
        ])
        (self.disc / "PARAM.SFO").write_bytes(wrong)
        patches = self.patched_expectations()
        with patches[0], patches[1], patches[2]:
            report = verifier.verify_inputs(self.disc, self.update)
        self.assertEqual(report["status"], "MISMATCH_OR_INCOMPLETE")
        self.assertTrue(any("disc PARAM.SFO" in error for error in report["errors"]))

    def test_rejects_duplicate_self_files_instead_of_guessing(self):
        duplicate = self.update / "backup"
        duplicate.mkdir()
        (duplicate / "default.self").write_bytes(self.self_bytes["default.self"])
        patches = self.patched_expectations()
        with patches[0], patches[1], patches[2]:
            report = verifier.verify_inputs(self.disc, self.update)
        self.assertEqual(report["inputs"]["installed_update_self_files"]["default.self"]["status"], "MISSING_OR_AMBIGUOUS")
        self.assertTrue(any("ambiguous default.self" in error for error in report["errors"]))

    def test_rejects_tampered_self_hash_even_when_container_is_valid(self):
        path = self.update / "USRDIR" / "default_mp.self"
        changed = bytearray(path.read_bytes())
        changed[-1] ^= 0xFF
        path.write_bytes(changed)
        patches = self.patched_expectations()
        with patches[0], patches[1], patches[2]:
            report = verifier.verify_inputs(self.disc, self.update)
        self.assertEqual(report["inputs"]["installed_update_self_files"]["default_mp.self"]["status"], "MISMATCH")
        self.assertTrue(any("default_mp.self: sha256" in error for error in report["errors"]))

    def test_optional_fastfile_mismatch_is_reported_without_silent_replacement(self):
        (self.disc / "USRDIR" / "common.ff").write_bytes(b"wrong fastfile")
        patches = self.patched_expectations()
        with patches[0], patches[1], patch.object(
            verifier, "EXPECTED_FASTFILES",
            {"common.ff": {"size_bytes": 10, "sha256": "0" * 64}},
        ):
            report = verifier.verify_inputs(self.disc, self.update)
        self.assertEqual(report["inputs"]["optional_base_fastfiles"]["common.ff"]["status"], "MISMATCH")
        self.assertTrue(any("common.ff" in error for error in report["errors"]))



    def test_verifies_both_decrypted_ppu_elfs_and_sets_native_ready(self):
        campaign_elf = self.root / "default.elf"
        multiplayer_elf = self.root / "default_mp.elf"
        campaign_elf.write_bytes(make_decrypted_ppu_elf(0x7E9508))
        multiplayer_elf.write_bytes(make_decrypted_ppu_elf(0xA75E38))

        patches = self.patched_expectations()
        with patches[0], patches[1], patches[2]:
            report = verifier.verify_inputs(
                self.disc, self.update, campaign_elf, multiplayer_elf
            )

        self.assertEqual(report["status"], "MATCHED_RETAIL_INPUTS")
        self.assertTrue(report["readiness"]["native_ppu_comparison_ready"])
        self.assertEqual(
            report["inputs"]["optional_decrypted_ppu_elfs"]["campaign"]["program_header_count"],
            8,
        )
        self.assertEqual(
            report["inputs"]["optional_decrypted_ppu_elfs"]["multiplayer"]["entry_point"],
            "0xA75E38",
        )
        self.assertEqual(
            report["inputs"]["optional_decrypted_ppu_elfs"]["campaign"]["machine"],
            "PowerPC64",
        )

    def test_rejects_decrypted_elf_with_wrong_installed_tu_entry_point(self):
        campaign_elf = self.root / "wrong-campaign.elf"
        multiplayer_elf = self.root / "default_mp.elf"
        campaign_elf.write_bytes(make_decrypted_ppu_elf(0x123456))
        multiplayer_elf.write_bytes(make_decrypted_ppu_elf(0xA75E38))

        patches = self.patched_expectations()
        with patches[0], patches[1], patches[2]:
            report = verifier.verify_inputs(
                self.disc, self.update, campaign_elf, multiplayer_elf
            )

        self.assertEqual(report["status"], "MISMATCH_OR_INCOMPLETE")
        self.assertFalse(report["readiness"]["native_ppu_comparison_ready"])
        self.assertTrue(any(
            "Campaign decrypted ELF: entry_point" in error
            for error in report["errors"]
        ))

if __name__ == "__main__":
    unittest.main()
