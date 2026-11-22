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

    def test_scripts_do_not_write_to_game_memory(self):
        for name in ("dump_campaign_tu116.bat", "dump_multiplayer_tu116.bat"):
            script = (ROOT / name).read_text(encoding="utf-8").lower()
            self.assertNotIn("setmem", script)
            self.assertNotIn("/patch.ps3", script)
            self.assertIn("if titleid blus31270", script)

if __name__ == "__main__":
    unittest.main()
