import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "client" / "ps3" / "probes" / "pad_probe"

class PadProbeSourceSafetyTests(unittest.TestCase):
    def test_probe_only_queries_pad_port_info(self):
        source = (PROBE / "module.c").read_text(encoding="utf-8")
        self.assertIn("cellPadGetInfo2", source)
        for forbidden in ("cellPadInit(", "cellPadEnd(", "cellPadGetData(", "cellPadSet"):
            self.assertNotIn(forbidden, source)
        self.assertNotIn("setmem.ps3mapi", source)
        self.assertNotIn("memcpy((void *)", source)

    def test_mode_selection_fails_closed(self):
        source = (PROBE / "module.c").read_text(encoding="utf-8")
        self.assertIn("MODE_FILE_MUST_CONTAIN_EXACTLY_CAMPAIGN_OR_MULTIPLAYER", source)
        self.assertIn("if (!select_mode())", source)
        self.assertIn("return SYS_PRX_RESIDENT;", source)

    def test_campaign_and_multiplayer_logs_are_separate(self):
        source = (PROBE / "module.c").read_text(encoding="utf-8")
        self.assertIn("ghosts_campaign_pad_probe.log", source)
        self.assertIn("ghosts_multiplayer_pad_probe.log", source)
        self.assertIn("ghosts_probe_mode.txt", source)

    def test_build_checks_prx_and_links_required_read_only_imports(self):
        makefile = (PROBE / "Makefile").read_text(encoding="utf-8")
        self.assertIn("-lfs_stub", makefile)
        self.assertIn("-lio_stub", makefile)
        self.assertIn("-llv2_stub", makefile)
        self.assertIn("prx-gen check", makefile)

if __name__ == "__main__":
    unittest.main()
