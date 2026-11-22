import json
import struct
import tempfile
import unittest
from pathlib import Path

from tools.ps3mapi.reconstruct_elf_from_ps3mapi_dumps import (
    DUMP_CHUNK, DumpFormatError, ELF_HEADER, ELF64_PHDR, SCE_HEADER, EXT_HEADER,
    align_up, reconstruct,
)
from tools.ps3mapi.verify_tu116_dump import verify

def make_self(path: Path):
    data = bytearray(0x400)
    SCE_HEADER.pack_into(data, 0, b"SCE\0", 2, 28, 1, 0x80, 0x100, 0x300)
    EXT_HEADER.pack_into(data, 0x20, 0, 0x80, 0x100, 0x200, 0, 0, 0, 0, 0, 0)
    ident = b"\x7fELF" + bytes([2, 2, 1]) + bytes(9)
    header = ELF_HEADER.pack(
        ident, 2, 21, 1, 0x10080, 64, 0, 0,
        64, 56, 2, 0, 0, 0,
    )
    data[0x100:0x100 + len(header)] = header
    ph0 = ELF64_PHDR.pack(1, 5, 0, 0x10000, 0x10000, 0x12345, 0x12345, 0x10000)
    ph1 = ELF64_PHDR.pack(1, 6, 0x20000, 0x30000, 0x30000, 0x4567, 0x10000, 0x10000)
    data[0x200:0x200 + 56] = ph0
    data[0x238:0x238 + 56] = ph1
    path.write_bytes(data)

def make_dumps(folder: Path, mode="campaign"):
    prefix = "GHOSTS_TU116_CAMPAIGN_SEG" if mode == "campaign" else "GHOSTS_TU116_MULTIPLAYER_SEG"
    for index, length in ((0, 0x12345), (1, 0x4567)):
        padded = align_up(length, DUMP_CHUNK)
        payload = bytes((i * (index + 3)) & 0xff for i in range(length))
        (folder / f"{prefix}{index}.bin").write_bytes(payload + bytes(padded - length))

def make_map(path: Path, mode="campaign"):
    spec = {
        "entry_point": "0x10080",
        "pt_load_segments": [
            {"segment_index": 0, "virtual_address": "0x10000", "file_size": 0x12345, "memory_size": 0x12345, "alignment": 0x10000},
            {"segment_index": 1, "virtual_address": "0x30000", "file_size": 0x4567, "memory_size": 0x10000, "alignment": 0x10000},
        ],
    }
    path.write_text(json.dumps({mode: spec}), encoding="utf-8")

class Ps3MapiDumpTests(unittest.TestCase):
    def test_reconstructs_elf_from_mode_named_files_without_manual_rename(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self_file = root / "default.self"
            dumps = root / "campaign"
            dumps.mkdir()
            output = root / "campaign.elf"
            make_self(self_file)
            make_dumps(dumps, "campaign")
            report = reconstruct(self_file, dumps, output, mode="campaign")
            self.assertEqual(report["status"], "RECONSTRUCTED_ANALYSIS_ELF")
            self.assertEqual(report["entry_point"], "0x10080")
            self.assertEqual(report["program_header_count"], 2)
            header = ELF_HEADER.unpack_from(output.read_bytes())
            self.assertEqual(header[4], 0x10080)
            self.assertEqual(header[10], 2)

    def test_verifier_accepts_campaign_profile_and_emits_report(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self_file = root / "default.self"
            dumps = root / "campaign"
            dumps.mkdir()
            map_file = root / "map.json"
            output = root / "campaign.elf"
            make_self(self_file)
            make_dumps(dumps, "campaign")
            make_map(map_file, "campaign")
            report = verify(self_file, dumps, map_file, output, "campaign")
            self.assertEqual(report["status"], "PASS_METADATA")
            self.assertEqual(report["mode"], "campaign")
            self.assertEqual(len(report["segments"]), 2)
            self.assertEqual(report["reconstructed_elf"]["status"], "RECONSTRUCTED_ANALYSIS_ELF")

    def test_verifier_accepts_multiplayer_named_files(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self_file = root / "default_mp.self"
            dumps = root / "multiplayer"
            dumps.mkdir()
            map_file = root / "map.json"
            output = root / "multiplayer.elf"
            make_self(self_file)
            make_dumps(dumps, "multiplayer")
            make_map(map_file, "multiplayer")
            report = verify(self_file, dumps, map_file, output, "multiplayer")
            self.assertEqual(report["mode"], "multiplayer")
            self.assertEqual(report["status"], "PASS_METADATA")

    def test_rejects_wrong_program_header_map(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self_file = root / "default.self"
            dumps = root / "campaign"
            dumps.mkdir()
            map_file = root / "map.json"
            make_self(self_file)
            make_dumps(dumps, "campaign")
            make_map(map_file, "campaign")
            spec = json.loads(map_file.read_text())
            spec["campaign"]["pt_load_segments"][0]["file_size"] += 1
            map_file.write_text(json.dumps(spec))
            with self.assertRaisesRegex(ValueError, "disagrees with TU 1.16 map"):
                verify(self_file, dumps, map_file, root / "out.elf", "campaign")

    def test_rejects_wrong_padded_dump_size(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self_file = root / "default.self"
            dumps = root / "campaign"
            dumps.mkdir()
            map_file = root / "map.json"
            make_self(self_file)
            make_dumps(dumps, "campaign")
            make_map(map_file, "campaign")
            dump = dumps / "GHOSTS_TU116_CAMPAIGN_SEG0.bin"
            dump.write_bytes(dump.read_bytes()[:-1])
            with self.assertRaisesRegex(ValueError, "should write exactly"):
                verify(self_file, dumps, map_file, root / "out.elf", "campaign")

    def test_rejects_zero_filled_executable_dump(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self_file = root / "default.self"
            dumps = root / "campaign"
            dumps.mkdir()
            map_file = root / "map.json"
            make_self(self_file)
            make_dumps(dumps, "campaign")
            make_map(map_file, "campaign")
            (dumps / "GHOSTS_TU116_CAMPAIGN_SEG0.bin").write_bytes(bytes(align_up(0x12345, DUMP_CHUNK)))
            with self.assertRaisesRegex(ValueError, "looks empty or invalid"):
                verify(self_file, dumps, map_file, root / "out.elf", "campaign")

    def test_refuses_to_overwrite_self_or_existing_output(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self_file = root / "default.self"
            dumps = root / "campaign"
            dumps.mkdir()
            make_self(self_file)
            make_dumps(dumps, "campaign")
            with self.assertRaisesRegex(DumpFormatError, "must not overwrite"):
                reconstruct(self_file, dumps, self_file, mode="campaign")
            output = root / "existing.elf"
            output.write_bytes(b"keep")
            with self.assertRaisesRegex(FileExistsError, "already exists"):
                reconstruct(self_file, dumps, output, mode="campaign")
            self.assertEqual(output.read_bytes(), b"keep")

    def test_accepts_generic_seg_names_as_fallback(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            self_file = root / "default.self"
            dumps = root / "dumps"
            dumps.mkdir()
            make_self(self_file)
            make_dumps(dumps, "campaign")
            for index in (0, 1):
                (dumps / f"SEG{index}.bin").write_bytes(
                    (dumps / f"GHOSTS_TU116_CAMPAIGN_SEG{index}.bin").read_bytes()
                )
            report = reconstruct(self_file, dumps, root / "generic.elf", mode=None)
            self.assertEqual(report["program_header_count"], 2)

if __name__ == "__main__":
    unittest.main()
