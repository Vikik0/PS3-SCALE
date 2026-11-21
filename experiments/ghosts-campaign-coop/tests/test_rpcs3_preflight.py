import struct
import tempfile
import unittest
from pathlib import Path

from tools.rpcs3_preflight import inspect_installation


def make_sfo(title_id="BLUS31270"):
    entries = [
        ("TITLE_ID", 0x0204, title_id),
        ("TITLE", 0x0204, "Call of Duty: Ghosts"),
        ("APP_VER", 0x0204, "01.00"),
    ]
    keys, values, records = bytearray(), bytearray(), []
    for key, fmt, value in entries:
        key_offset = len(keys)
        keys.extend(key.encode("ascii") + b"\0")
        raw = value.encode("utf-8") + b"\0"
        value_offset = len(values)
        values.extend(raw)
        records.append((key_offset, fmt, len(raw), len(raw), value_offset))
    key_start = 20 + len(records) * 16
    data_start = key_start + len(keys)
    header = struct.pack("<4sIIII", b"\0PSF", 0x00000101, key_start, data_start, len(records))
    index = b"".join(struct.pack("<HHIII", *record) for record in records)
    return header + index + bytes(keys) + bytes(values)


class Rpcs3PreflightTests(unittest.TestCase):
    def make_layout(self, root, title_id="BLUS31270", with_log=True):
        game = root / "game" / "PS3_GAME"
        (game / "USRDIR").mkdir(parents=True)
        (game / "PARAM.SFO").write_bytes(make_sfo(title_id))
        (game / "USRDIR" / "EBOOT.BIN").write_bytes(b"SCE\0" + b"test-eboot")
        emulator = root / "RPCS3"
        emulator.mkdir()
        (emulator / "rpcs3.exe").write_bytes(b"fake executable")
        if with_log:
            (emulator / "RPCS3.log").write_text(
                "SYS: Booting BLUS31270\n"
                "Pad 0: device='DualShock 3 Controller', handler=DualShock 3, VID=0x054c, PID=0x0268\n"
                "Pad 1: device='DualShock 3 Controller', handler=DualShock 3, VID=0x054c, PID=0x0268\n"
                "PPU executable hash: PPU-0123456789abcdef0123456789abcdef01234567\n",
                encoding="utf-8",
            )
        return game.parent, emulator

    def test_selected_profile_ready_for_baseline(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            game_root, emulator = self.make_layout(root)
            report = inspect_installation(game_root, rpcs3_root=emulator)
        self.assertEqual(report["overall"], "READY_FOR_BASELINE_BOOT")
        self.assertTrue(report["selected_profile"]["title_id_match"])
        self.assertEqual(report["game_files"]["eboot"]["container"], "SCE_CONTAINER")
        self.assertEqual(
            report["rpcs3"]["log"]["latest_ppu_hash"],
            "PPU-0123456789ABCDEF0123456789ABCDEF01234567",
        )
        self.assertEqual(report["checks"]["title_update"], "NOT_VERIFIED")
        self.assertEqual(report["checks"]["two_controller_ports"], "BOTH_PORTS_HAVE_NON_NULL_BINDINGS")
        self.assertFalse(report["rpcs3"]["log"]["controller_ports"]["distinct_device_labels"])
        self.assertFalse(report["patch_gate"]["ready"])

    def test_wrong_region_blocks_baseline(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            game_root, emulator = self.make_layout(root, title_id="BLES01945")
            report = inspect_installation(game_root, rpcs3_root=emulator)
        self.assertEqual(report["overall"], "BLOCKED")
        self.assertFalse(report["selected_profile"]["title_id_match"])

    def test_missing_log_waits_for_first_boot(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            game_root, emulator = self.make_layout(root, with_log=False)
            report = inspect_installation(game_root, rpcs3_root=emulator)
        self.assertEqual(report["overall"], "READY_FOR_BASELINE_BOOT")
        self.assertEqual(report["rpcs3"]["log"]["status"], "WAITING_FOR_FIRST_BOOT")

    def test_one_bound_pad_does_not_claim_two_ports(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            game_root, emulator = self.make_layout(root)
            (emulator / "RPCS3.log").write_text(
                "Pad 0: device='DualShock 3 Controller', handler=DualShock 3, VID=0x054c, PID=0x0268\n",
                encoding="utf-8",
            )
            report = inspect_installation(game_root, rpcs3_root=emulator)
        self.assertEqual(report["checks"]["two_controller_ports"], "INCOMPLETE_LOG_EVIDENCE")

    def test_expected_ppu_hash_mismatch_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            game_root, emulator = self.make_layout(root)
            report = inspect_installation(
                game_root, rpcs3_root=emulator,
                expected_ppu_hash="PPU-FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF",
            )
        self.assertEqual(report["checks"]["expected_ppu_hash_match"], "FAIL")

    def test_ps3_game_folder_can_be_passed_directly(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            game_root, emulator = self.make_layout(root)
            report = inspect_installation(game_root / "PS3_GAME", rpcs3_root=emulator)
        self.assertEqual(report["overall"], "READY_FOR_BASELINE_BOOT")

    def test_malformed_sfo_fails_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            game_root, emulator = self.make_layout(root)
            (game_root / "PS3_GAME" / "PARAM.SFO").write_bytes(b"not-a-param-sfo")
            with self.assertRaises(ValueError):
                inspect_installation(game_root, rpcs3_root=emulator)


if __name__ == "__main__":
    unittest.main()
