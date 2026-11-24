import hashlib
import struct
import tempfile
import unittest
from pathlib import Path

from tools.verify_tu116_input_set import verify_input_set


def make_sfo(entries):
    key_table, data_table, index = bytearray(), bytearray(), []
    for key, fmt, value in entries:
        key_offset = len(key_table)
        key_table.extend(key.encode("ascii") + b"\x00")
        if fmt == 0x0204:
            raw = value.encode("utf-8") + b"\x00"
            max_len = len(raw)
        else:
            raw = struct.pack("<I", value)
            max_len = 4
        value_offset = len(data_table)
        data_table.extend(raw)
        index.append((key_offset, fmt, len(raw), max_len, value_offset))
    key_start = 20 + 16 * len(index)
    data_start = key_start + len(key_table)
    header = struct.pack("<4sIIII", b"\x00PSF", 0x00000101, key_start, data_start, len(index))
    return header + b"".join(struct.pack("<HHIII", *x) for x in index) + key_table + data_table


def make_self(entry_point, segments):
    data = bytearray(0x1400)
    from tools import ps3_self_inspect as mod
    mod.SCE_HEADER.pack_into(data, 0, b"SCE\x00", 2, 28, 1, 0x4A0, 0xA80, 0x20000)
    mod.EXT_HEADER.pack_into(data, 0x20, 3, 0x70, 0x90, 0xD0, 0, 0x290, 0x390, 0x3C0, 0x70, 0)
    mod.APP_INFO.pack_into(data, 0x70, 0x1010000001000003, 0x01000002, 8, 0x0001000000000000, 0)
    ident = b"\x7fELF" + bytes([2, 2, 1, 0x66]) + bytes(8)
    mod.ELF_HEADER.pack_into(data, 0x90, ident, 2, 21, 1, entry_point, 64, 0, 0, 64, 56, 8, 64, 0, 0)
    for i in range(8):
        if i < len(segments):
            vaddr, filesz, memsz, align = segments[i]
            mod.ELF64_PHDR.pack_into(data, 0xD0 + i * 56, 1, 5, 0, vaddr, vaddr, filesz, memsz, align)
            data_offset = 0xA80 + i * 0x100
            mod.SEGMENT_INFO.pack_into(data, 0x290 + i * 32, data_offset, 0x80, 2, 1)
        else:
            mod.ELF64_PHDR.pack_into(data, 0xD0 + i * 56, 0x60000001, 0, 0, 0, 0, 0, 0, 8)
            mod.SEGMENT_INFO.pack_into(data, 0x290 + i * 32, 0, 0, 0, 0)
    return bytes(data)


class VerifyTu116InputSetTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.sfo_path = self.root / "PARAM.SFO"
        self.eboot_path = self.root / "EBOOT.BIN"
        self.campaign_path = self.root / "default.self"
        self.multiplayer_path = self.root / "default_mp.self"

        sfo = make_sfo([
            ("TITLE_ID", 0x0204, "BLUS31270"),
            ("APP_VER", 0x0204, "01.16"),
            ("PS3_SYSTEM_VER", 0x0204, "04.4000"),
            ("TITLE", 0x0204, "Call of Duty® Ghosts"),
        ])
        eboot = make_self(0x20080, [(0x10000, 60288, 60288, 65536), (0x20000, 2704, 137056, 65536)])
        campaign = make_self(0x7E9508, [(0x10000, 8135720, 8135720, 65536), (0x7E0000, 119264, 31551768, 65536)])
        multiplayer = make_self(0xA75E38, [(0x10000, 10783784, 10783784, 65536), (0xA60000, 932304, 31434904, 65536)])
        self.sfo_path.write_bytes(sfo)
        self.eboot_path.write_bytes(eboot)
        self.campaign_path.write_bytes(campaign)
        self.multiplayer_path.write_bytes(multiplayer)

        def item(path, entry, segments=None):
            result = {
                "file": path.name,
                "size_bytes": path.stat().st_size,
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "key_revision": 28,
                "entry_point": f"0x{entry:X}",
            }
            if segments is not None:
                result["pt_load_segments"] = [
                    {
                        "segment_index": i,
                        "virtual_address": f"0x{v:X}",
                        "file_size": fs,
                        "memory_size": ms,
                        "alignment": al,
                    }
                    for i, (v, fs, ms, al) in enumerate(segments)
                ]
            return result

        c_segments = [(0x10000, 8135720, 8135720, 65536), (0x7E0000, 119264, 31551768, 65536)]
        m_segments = [(0x10000, 10783784, 10783784, 65536), (0xA60000, 932304, 31434904, 65536)]
        self.manifest = {
            "target": {
                "title_id": "BLUS31270",
                "app_ver": "01.16",
                "ps3_system_ver": "04.4000",
                "title": "Call of Duty® Ghosts",
            },
            "sfo": {"sha256": hashlib.sha256(sfo).hexdigest()},
            "executables": {
                "eboot": item(self.eboot_path, 0x20080),
                "campaign": item(self.campaign_path, 0x7E9508, c_segments),
                "multiplayer": item(self.multiplayer_path, 0xA75E38, m_segments),
            },
        }
        self.memory_map = {
            "campaign": {
                "entry_point": "0x7E9508",
                "pt_load_segments": self.manifest["executables"]["campaign"]["pt_load_segments"],
            },
            "multiplayer": {
                "entry_point": "0xA75E38",
                "pt_load_segments": self.manifest["executables"]["multiplayer"]["pt_load_segments"],
            },
        }

    def tearDown(self):
        self.tmp.cleanup()

    def verify(self, **overrides):
        args = {
            "sfo_path": self.sfo_path,
            "eboot_path": self.eboot_path,
            "campaign_self_path": self.campaign_path,
            "multiplayer_self_path": self.multiplayer_path,
            "manifest": self.manifest,
            "memory_map": self.memory_map,
        }
        args.update(overrides)
        return verify_input_set(**args)

    def test_exact_input_set_passes_without_claiming_runtime_capture(self):
        report = self.verify()
        self.assertTrue(report["ok"])
        self.assertEqual(report["status"], "PASS_STATIC_INPUTS")
        self.assertEqual(report["target"]["app_ver"], "01.16")
        self.assertEqual(report["runtime_capture"]["campaign_pt_load_dumps"], "NOT_PROVIDED")
        self.assertEqual(report["runtime_capture"]["playable_campaign_player_two"], "NOT_PROVEN")

    def test_wrong_title_update_is_rejected_even_with_matching_sfo_hash(self):
        wrong = make_sfo([
            ("TITLE_ID", 0x0204, "BLUS31270"),
            ("APP_VER", 0x0204, "01.00"),
            ("PS3_SYSTEM_VER", 0x0204, "04.4000"),
            ("TITLE", 0x0204, "Call of Duty® Ghosts"),
        ])
        self.sfo_path.write_bytes(wrong)
        self.manifest["sfo"]["sha256"] = hashlib.sha256(wrong).hexdigest()
        report = self.verify()
        self.assertFalse(report["ok"])
        self.assertTrue(any("APP_VER" in error for error in report["errors"]))

    def test_campaign_hash_mismatch_is_rejected(self):
        data = bytearray(self.campaign_path.read_bytes())
        data[-1] ^= 0x01
        self.campaign_path.write_bytes(data)
        report = self.verify()
        self.assertFalse(report["ok"])
        self.assertTrue(any("Campaign SELF: SHA-256 mismatch" in error for error in report["errors"]))

    def test_campaign_segment_map_mismatch_is_rejected(self):
        self.memory_map["campaign"]["pt_load_segments"][0]["file_size"] += 4
        report = self.verify()
        self.assertFalse(report["ok"])
        self.assertTrue(any("PT_LOAD segment map mismatch" in error for error in report["errors"]))

    def test_campaign_and_multiplayer_must_be_separate_inputs(self):
        report = self.verify(multiplayer_self_path=self.campaign_path)
        self.assertFalse(report["ok"])
        self.assertTrue(any("must be different files" in error for error in report["errors"]))


if __name__ == "__main__":
    unittest.main()
