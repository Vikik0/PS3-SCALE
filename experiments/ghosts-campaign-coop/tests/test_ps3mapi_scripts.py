import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "tools" / "ps3mapi"

class Ps3MapiScriptTests(unittest.TestCase):
    def test_campaign_dump_uses_exact_tu116_ranges_and_is_manual(self):
        script = (ROOT / "dump_campaign_tu116.bat").read_text(encoding="utf-8").lower()
        self.assertIn("manual, read-only", script)
        self.assertIn("addr=10000&len=20&dump=7c2428", script)
        self.assertIn("addr=7e0000&len=20&dump=1d1e0", script)
        self.assertIn("ghosts_tu116_campaign_seg0.bin", script)
        self.assertIn("ghosts_tu116_campaign_seg1.bin", script)
        self.assertNotIn("/tmp/wm_ingame/", script)

    def test_multiplayer_dump_uses_exact_tu116_ranges_and_is_manual(self):
        script = (ROOT / "dump_multiplayer_tu116.bat").read_text(encoding="utf-8").lower()
        self.assertIn("manual, read-only", script)
        self.assertIn("addr=10000&len=20&dump=a48c28", script)
        self.assertIn("addr=a60000&len=20&dump=e39d0", script)
        self.assertIn("ghosts_tu116_multiplayer_seg0.bin", script)
        self.assertIn("ghosts_tu116_multiplayer_seg1.bin", script)
        self.assertNotIn("/tmp/wm_ingame/", script)

    def test_memory_map_matches_the_phase7_tu116_profiles(self):
        profile = json.loads((ROOT / "SELF_MEMORY_MAP_TU116.json").read_text(encoding="utf-8"))
        self.assertEqual(profile["campaign"]["entry_point"], "0x7E9508")
        self.assertEqual(profile["campaign"]["pt_load_segments"][0]["file_size_hex"], "0x7c2428")
        self.assertEqual(profile["campaign"]["pt_load_segments"][1]["file_size_hex"], "0x1d1e0")
        self.assertEqual(profile["multiplayer"]["entry_point"], "0xA75E38")
        self.assertEqual(profile["multiplayer"]["pt_load_segments"][0]["file_size_hex"], "0xa48c28")
        self.assertEqual(profile["multiplayer"]["pt_load_segments"][1]["file_size_hex"], "0xe39d0")

    def test_each_dump_is_checked_after_rename_before_done_marker(self):
        for mode in ("campaign", "multiplayer"):
            prefix = f"ghosts_tu116_{mode}"
            script = (ROOT / f"dump_{mode}_tu116.bat").read_text(encoding="utf-8").lower()
            done_marker = f"/dev_hdd0/{prefix}_dump_done"
            self.assertIn(done_marker, script)
            completion = script.index(f"logfile {done_marker}")
            for index in (0, 1):
                target = f"/dev_hdd0/{prefix}_seg{index}.bin"
                rename = f"ren /dev_hdd0/mem_dump.bin={target}"
                check = f"abort if not exist {target}"
                self.assertIn(rename, script)
                self.assertIn(check, script)
                self.assertLess(script.index(rename), script.index(check))
                self.assertLess(script.index(check), completion)
            self.assertGreater(
                completion,
                max(script.index(f"abort if not exist /dev_hdd0/{prefix}_seg{i}.bin") for i in (0, 1)),
            )

    def test_scripts_do_not_write_to_game_memory(self):
        for name in ("dump_campaign_tu116.bat", "dump_multiplayer_tu116.bat"):
            script = (ROOT / name).read_text(encoding="utf-8").lower()
            self.assertNotIn("setmem", script)
            self.assertNotIn("/patch.ps3", script)
            self.assertIn("if titleid blus31270", script)

if __name__ == "__main__":
    unittest.main()
