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

    def test_mode_parser_rejects_internal_whitespace_instead_of_truncating(self):
        source = (PROBE / "module.c").read_text(encoding="utf-8")
        self.assertIn("while (length > 0u)", source)
        self.assertIn("for (i = 0; i < length; ++i)", source)
        self.assertIn("return 0;", source)

    def test_log_marks_mode_as_operator_selected_not_auto_detected(self):
        source = (PROBE / "module.c").read_text(encoding="utf-8")
        self.assertIn("MODE_SOURCE=manual_mode_file", source)
        self.assertIn("MODE_LABEL_IS_OPERATOR_SELECTED=1", source)

    def test_campaign_and_multiplayer_logs_are_separate(self):
        source = (PROBE / "module.c").read_text(encoding="utf-8")
        self.assertIn("ghosts_campaign_pad_probe.log", source)
        self.assertIn("ghosts_multiplayer_pad_probe.log", source)
        self.assertIn("ghosts_probe_mode.txt", source)

    def test_build_checks_prx_and_links_required_read_only_imports(self):
        makefile = (PROBE / "Makefile").read_text(encoding="utf-8")
        self.assertIn("-lsys_fs_stub", makefile)
        self.assertIn("-lio_stub", makefile)
        self.assertIn("-llv2_stub", makefile)
        self.assertIn("$(PRX_GEN) check", makefile)

if __name__ == "__main__":
    unittest.main()
