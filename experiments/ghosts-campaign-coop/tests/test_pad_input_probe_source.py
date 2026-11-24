import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / "client" / "ps3" / "probes" / "pad_input_probe"


class PadInputProbeSourceTests(unittest.TestCase):
    def test_probe_uses_existing_two_port_ps3_adapter(self):
        source = (PROBE / "module.c").read_text(encoding="utf-8")
        self.assertIn("ghc_local_input_poll_ps3(&snapshot)", source)
        self.assertIn("GhcLocalInputSnapshot", source)
        self.assertIn("GhcLocalInputFrame", source)
        self.assertIn("ports 0 and 1 only", source)

    def test_probe_does_not_initialize_end_or_write_pad_state(self):
        source = (PROBE / "module.c").read_text(encoding="utf-8")
        for forbidden in (
            "cellPadInit(",
            "cellPadEnd(",
            "cellPadSetPortSetting(",
            "cellPadSetPressMode(",
            "cellPadSetSensorMode(",
            "cellPadSetActDirect(",
            "setmem.ps3mapi",
            "memcpy((void *)",
        ):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, source)

    def test_mode_is_manual_and_campaign_multiplayer_logs_are_separate(self):
        source = (PROBE / "module.c").read_text(encoding="utf-8")
        self.assertIn("MODE_SOURCE=manual_mode_file", source)
        self.assertIn("MODE_LABEL_IS_OPERATOR_SELECTED=1", source)
        self.assertIn("ghosts_campaign_pad_input_probe.log", source)
        self.assertIn("ghosts_multiplayer_pad_input_probe.log", source)
        self.assertIn("MODE_FILE_MUST_CONTAIN_EXACTLY_CAMPAIGN_OR_MULTIPLAYER", source)

    def test_probe_logs_independent_button_and_stick_fields(self):
        source = (PROBE / "module.c").read_text(encoding="utf-8")
        for field in (
            "digital1=",
            "digital2=",
            "buttons=0x",
            "pressed=0x",
            "released=0x",
            "left_x=",
            "right_x=",
            "l2_pressure=",
            "r2_pressure=",
        ):
            self.assertIn(field, source)

    def test_probe_is_bounded_and_joins_worker_before_unload(self):
        source = (PROBE / "module.c").read_text(encoding="utf-8")
        self.assertIn("#define PROBE_SAMPLES 30u", source)
        self.assertIn("#define PROBE_INTERVAL_US 1000000u", source)
        self.assertIn("int join_status = thread_join(g_thread, &thread_result);", source)
        self.assertIn("if (join_status != 0)", source)
        self.assertIn("return SYS_PRX_STOP_FAIL;", source)

    def test_build_links_the_shared_decoder_adapter_and_cell_io_imports(self):
        makefile = (PROBE / "Makefile").read_text(encoding="utf-8")
        self.assertIn("../../local_input.c", makefile)
        self.assertIn("../../local_input_ps3.c", makefile)
        self.assertIn("-lio_stub", makefile)
        self.assertIn("$(PRX_GEN) check", makefile)
        self.assertIn("ghosts_pad_input_probe.fake.sprx", makefile)

    def test_export_library_and_ping_nid_are_stable(self):
        exports = (PROBE / "exports.S").read_text(encoding="utf-8")
        self.assertIn('PRX_LIBRARY_BEGIN(ghosts_pad_input_probe, "ghosts_pad_input_probe")', exports)
        self.assertIn("ghosts_input_probe_ping, 0x47504950", exports)


if __name__ == "__main__":
    unittest.main()
