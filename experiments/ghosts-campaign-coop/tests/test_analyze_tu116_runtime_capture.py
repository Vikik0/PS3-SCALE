import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.ps3mapi import analyze_tu116_runtime_capture as analyzer


class RuntimeCaptureAnalyzerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.campaign_self = self.root / "default.self"
        self.multiplayer_self = self.root / "default_mp.self"
        self.campaign_self.write_bytes(b"campaign-self-fixture")
        self.multiplayer_self.write_bytes(b"multiplayer-self-fixture")
        self.campaign_dumps = self.root / "campaign-dumps"
        self.multiplayer_dumps = self.root / "multiplayer-dumps"
        self.campaign_dumps.mkdir()
        self.multiplayer_dumps.mkdir()
        self.map_file = self.root / "memory-map.json"
        self.map_file.write_text('{"campaign": {}, "multiplayer": {}}', encoding="utf-8")
        self.output_dir = self.root / "analysis"

    def tearDown(self):
        self.temp.cleanup()

    def fake_verify(self, self_file, dump_dir, map_file, output_elf, mode, *, overwrite=False):
        Path(output_elf).write_bytes((mode + "-analysis-elf").encode("ascii"))
        return {
            "status": "PASS_METADATA",
            "mode": mode,
            "self_sha256": analyzer.sha256_file(Path(self_file)),
            "reconstructed_elf": {"output_elf": str(output_elf)},
        }

    @staticmethod
    def fake_scan(path):
        return {
            "file": str(path),
            "entry_point": "0x10000",
            "match_count": 0,
            "unique_strings": 0,
            "matches": [],
        }

    @staticmethod
    def fake_string_compare(campaign_elf, multiplayer_elf):
        return {"counts": {"campaign_only": 0, "multiplayer_only": 0, "common": 0}}

    @staticmethod
    def fake_binary_compare(campaign_elf, multiplayer_elf):
        return {"totals": {"blocks_compared": 1, "identical_bytes": 0, "different_bytes": 1}}

    def write_log(self, name, mode):
        path = self.root / name
        lines = [
            "Ghosts Campaign Co-op Pad Probe v4",
            "READ_ONLY=1",
            f"MODE={mode}",
            "MODE_SOURCE=manual_mode_file",
            "MODE_LABEL_IS_OPERATOR_SELECTED=1",
        ]
        for sample_index in (0, 5, 10, 15, 20, 25):
            lines.append(
                f"sample={sample_index} result=OK max_connect=7 now_connect=2 "
                "system_info=0x00000000 port_mask=0x03"
            )
        lines.append("Pad probe finished.")
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")
        return path

    def run_analyzer(self, **kwargs):
        defaults = {
            "campaign_self": self.campaign_self,
            "campaign_dumps": self.campaign_dumps,
            "multiplayer_self": self.multiplayer_self,
            "multiplayer_dumps": self.multiplayer_dumps,
            "output_dir": self.output_dir,
            "map_file": self.map_file,
        }
        defaults.update(kwargs)
        with (
            patch.object(analyzer, "verify_dump", side_effect=self.fake_verify),
            patch.object(analyzer, "scan_runtime_strings", side_effect=self.fake_scan),
            patch.object(analyzer, "compare_runtime_strings", side_effect=self.fake_string_compare),
            patch.object(analyzer, "compare_runtime_elfs", side_effect=self.fake_binary_compare),
        ):
            return analyzer.analyze(**defaults)

    def test_complete_capture_pair_is_ready_for_engine_review_not_player_two(self):
        campaign_log = self.write_log("campaign.log", "CAMPAIGN")
        multiplayer_log = self.write_log("multiplayer.log", "MULTIPLAYER")
        result = self.run_analyzer(campaign_log=campaign_log, multiplayer_log=multiplayer_log)

        self.assertEqual(result["status"], "CAPTURE_READY_FOR_ENGINE_REVIEW")
        self.assertTrue(result["dump_verification"]["both_passed"])
        self.assertTrue(result["pad_probe"]["both_ports_visible_simultaneously_in_both_modes"])
        self.assertTrue(result["pad_probe"]["capture_complete_in_both_modes"])
        self.assertTrue(result["pad_probe"]["capture_provenance_valid_in_both_modes"])
        self.assertEqual(result["player_two_gate"]["status"], "NOT_PROVEN")
        self.assertEqual(result["ppu_analysis"]["status"], "NOT_RUN")
        self.assertTrue((self.output_dir / "campaign_tu116_runtime.elf").is_file())
        self.assertTrue((self.output_dir / "multiplayer_tu116_runtime.elf").is_file())
        self.assertTrue((self.output_dir / "capture_summary.json").is_file())

    def test_optional_ghidra_analysis_adds_six_outputs_and_summary_evidence(self):
        fake_manifest = {
            "campaign": {"function_records": 100, "builtin_xref_rows": 4},
            "multiplayer": {"function_records": 120, "builtin_xref_rows": 8},
        }
        fake_comparison = {"multiplayer_only_leads": [{"builtin": "isSplitScreen"}]}

        def fake_generate(headless, campaign_elf, multiplayer_elf, output_dir, **kwargs):
            output_dir.mkdir(parents=True, exist_ok=True)
            for name in analyzer.GHIDRA_OUTPUT_MAP:
                path = output_dir / name
                if name == "ppu_inventory_manifest.json":
                    path.write_text(__import__("json").dumps(fake_manifest), encoding="utf-8")
                elif name == "splitscreen_builtin_xrefs.json":
                    path.write_text(__import__("json").dumps(fake_comparison), encoding="utf-8")
                else:
                    path.write_text("synthetic csv", encoding="utf-8")
            return fake_manifest

        headless = self.root / "analyzeHeadless"
        headless.write_text("fake", encoding="utf-8")
        with patch.object(analyzer, "generate_inventories", side_effect=fake_generate) as generate:
            result = self.run_analyzer(analyze_headless=headless)

        self.assertEqual(result["ppu_analysis"]["status"], "GHIDRA_PPU_ANALYSIS_COMPLETED")
        self.assertEqual(result["ppu_analysis"]["multiplayer_only_builtin_lead_count"], 1)
        self.assertEqual(result["ppu_analysis"]["campaign_function_records"], 100)
        self.assertEqual(result["player_two_gate"]["status"], "NOT_PROVEN")
        for name in analyzer.GHIDRA_OUTPUT_NAMES:
            self.assertTrue((self.output_dir / name).is_file(), name)
        generate.assert_called_once()
        self.assertIn("ppu_splitscreen_builtin_xrefs.json", result["outputs"])

    def test_ghidra_failure_does_not_promote_any_capture_outputs(self):
        headless = self.root / "analyzeHeadless"
        headless.write_text("fake", encoding="utf-8")
        with patch.object(analyzer, "generate_inventories", side_effect=RuntimeError("Ghidra failed")):
            with self.assertRaisesRegex(RuntimeError, "Ghidra failed"):
                self.run_analyzer(analyze_headless=headless)
        self.assertFalse(any((self.output_dir / name).exists() for name in analyzer.OUTPUT_NAMES))
        self.assertEqual(list(self.output_dir.glob(".ghosts-capture-stage-*")), [])

    def test_ghidra_options_require_analyze_headless(self):
        with self.assertRaisesRegex(ValueError, "require --analyze-headless"):
            self.run_analyzer(ghidra_script_dir=self.root)

    def test_truncated_pad_capture_cannot_pass_engine_review_gate(self):
        campaign_log = self.write_log("campaign.log", "CAMPAIGN")
        multiplayer_log = self.write_log("multiplayer.log", "MULTIPLAYER")
        lines = [
            line for line in campaign_log.read_text(encoding="utf-8").splitlines()
            if line != "Pad probe finished."
        ]
        campaign_log.write_text("\n".join(lines) + "\n", encoding="utf-8")

        result = self.run_analyzer(campaign_log=campaign_log, multiplayer_log=multiplayer_log)

        self.assertEqual(result["pad_probe"]["status"], "INCONCLUSIVE_INCOMPLETE_CAPTURE")
        self.assertEqual(result["status"], "ELF_ANALYSIS_READY_PAD_GATE_NOT_PASSED")
        self.assertFalse(result["pad_probe"]["both_ports_visible_simultaneously_in_both_modes"])
        self.assertFalse(result["pad_probe"]["capture_complete_in_both_modes"])
        self.assertTrue(result["pad_probe"]["capture_provenance_valid_in_both_modes"])
        self.assertEqual(result["player_two_gate"]["status"], "NOT_PROVEN")

    def test_missing_pad_logs_are_reported_without_claiming_controller_visibility(self):
        result = self.run_analyzer()
        self.assertEqual(result["status"], "ELF_ANALYSIS_READY_PAD_EVIDENCE_MISSING")
        self.assertEqual(result["pad_probe"]["status"], "NOT_PROVIDED")
        self.assertFalse(result["pad_probe"]["both_ports_visible_simultaneously_in_both_modes"])
        self.assertEqual(result["player_two_gate"]["status"], "NOT_PROVEN")

    def test_only_one_pad_log_is_rejected(self):
        campaign_log = self.write_log("campaign.log", "CAMPAIGN")
        with self.assertRaisesRegex(ValueError, "both pad-probe logs or neither"):
            self.run_analyzer(campaign_log=campaign_log)

    def test_wrong_manual_mode_labels_do_not_pass_pad_gate(self):
        campaign_log = self.write_log("campaign.log", "MULTIPLAYER")
        multiplayer_log = self.write_log("multiplayer.log", "CAMPAIGN")
        result = self.run_analyzer(campaign_log=campaign_log, multiplayer_log=multiplayer_log)
        self.assertEqual(result["pad_probe"]["status"], "INCONCLUSIVE_MODE_LABEL_MISMATCH")
        self.assertEqual(result["status"], "ELF_ANALYSIS_READY_PAD_GATE_NOT_PASSED")

    def test_existing_outputs_are_protected_without_overwrite(self):
        self.output_dir.mkdir()
        existing = self.output_dir / "capture_summary.json"
        existing.write_text("keep", encoding="utf-8")
        with patch.object(analyzer, "verify_dump") as verify:
            with self.assertRaises(FileExistsError):
                self.run_analyzer()
            verify.assert_not_called()
        self.assertEqual(existing.read_text(encoding="utf-8"), "keep")

    def test_overwrite_must_be_explicit_and_replaces_only_analysis_outputs(self):
        self.output_dir.mkdir()
        existing = self.output_dir / "capture_summary.json"
        existing.write_text("old", encoding="utf-8")
        unrelated = self.output_dir / "operator-notes.txt"
        unrelated.write_text("keep", encoding="utf-8")
        result = self.run_analyzer(overwrite=True)
        self.assertEqual(result["status"], "ELF_ANALYSIS_READY_PAD_EVIDENCE_MISSING")
        self.assertNotEqual(existing.read_text(encoding="utf-8"), "old")
        self.assertEqual(unrelated.read_text(encoding="utf-8"), "keep")

    def test_second_mode_verification_failure_leaves_no_partial_outputs(self):
        calls = []

        def fail_second(self_file, dump_dir, map_file, output_elf, mode, *, overwrite=False):
            calls.append(mode)
            if mode == "campaign":
                Path(output_elf).write_bytes(b"staged-campaign-elf")
                return {"status": "PASS_METADATA", "mode": mode}
            raise ValueError("multiplayer dump is invalid")

        with patch.object(analyzer, "verify_dump", side_effect=fail_second):
            with self.assertRaisesRegex(ValueError, "multiplayer dump is invalid"):
                analyzer.analyze(
                    self.campaign_self,
                    self.campaign_dumps,
                    self.multiplayer_self,
                    self.multiplayer_dumps,
                    self.output_dir,
                    map_file=self.map_file,
                )

        self.assertEqual(calls, ["campaign", "multiplayer"])
        self.assertFalse(any((self.output_dir / name).exists() for name in analyzer.OUTPUT_NAMES))
        self.assertEqual(list(self.output_dir.glob(".ghosts-capture-stage-*")), [])

    def test_failed_overwrite_preserves_previous_outputs_and_unrelated_files(self):
        self.output_dir.mkdir()
        previous = {
            "campaign_tu116_runtime.elf": b"old-campaign",
            "capture_summary.json": b"old-summary",
        }
        for name, content in previous.items():
            (self.output_dir / name).write_bytes(content)
        unrelated = self.output_dir / "operator-notes.txt"
        unrelated.write_text("keep", encoding="utf-8")

        def fail_second(self_file, dump_dir, map_file, output_elf, mode, *, overwrite=False):
            if mode == "campaign":
                Path(output_elf).write_bytes(b"new-staged-campaign")
                return {"status": "PASS_METADATA", "mode": mode}
            raise ValueError("late verification failure")

        with patch.object(analyzer, "verify_dump", side_effect=fail_second):
            with self.assertRaisesRegex(ValueError, "late verification failure"):
                analyzer.analyze(
                    self.campaign_self,
                    self.campaign_dumps,
                    self.multiplayer_self,
                    self.multiplayer_dumps,
                    self.output_dir,
                    map_file=self.map_file,
                    overwrite=True,
                )

        for name, content in previous.items():
            self.assertEqual((self.output_dir / name).read_bytes(), content)
        self.assertEqual(unrelated.read_text(encoding="utf-8"), "keep")
        self.assertEqual(list(self.output_dir.glob(".ghosts-capture-stage-*")), [])
        self.assertEqual(list(self.output_dir.glob(".ghosts-capture-backup-*")), [])

    def test_output_promotion_rolls_back_if_a_rename_fails(self):
        self.output_dir.mkdir()
        stage_dir = self.root / "staged"
        stage_dir.mkdir()
        staged = {name: stage_dir / name for name in analyzer.OUTPUT_NAMES}
        final = {name: self.output_dir / name for name in analyzer.OUTPUT_NAMES}
        for name, path in staged.items():
            path.write_bytes(("new-" + name).encode("utf-8"))

        previous = {
            "campaign_tu116_runtime.elf": b"old-campaign",
            "multiplayer_tu116_runtime.elf": b"old-multiplayer",
        }
        for name, value in previous.items():
            final[name].write_bytes(value)
        unrelated = self.output_dir / "operator-notes.txt"
        unrelated.write_text("keep", encoding="utf-8")

        real_replace = analyzer.os.replace
        calls = 0

        def fail_fourth_replace(source, destination):
            nonlocal calls
            calls += 1
            if calls == 4:
                raise OSError("simulated promotion failure")
            return real_replace(source, destination)

        with patch.object(analyzer.os, "replace", side_effect=fail_fourth_replace):
            with self.assertRaisesRegex(OSError, "simulated promotion failure"):
                analyzer._commit_staged_outputs(staged, final, overwrite=True)

        for name, value in previous.items():
            self.assertEqual(final[name].read_bytes(), value)
        self.assertEqual(unrelated.read_text(encoding="utf-8"), "keep")
        self.assertEqual(list(self.output_dir.glob(".ghosts-capture-backup-*")), [])

    def test_reports_use_final_paths_not_temporary_stage_paths(self):
        self.run_analyzer()
        summary = __import__("json").loads(
            (self.output_dir / "capture_summary.json").read_text(encoding="utf-8")
        )
        self.assertEqual(
            summary["outputs"]["campaign_tu116_runtime.elf"],
            str(self.output_dir / "campaign_tu116_runtime.elf"),
        )
        self.assertNotIn(".ghosts-capture-stage-", str(summary))
        verification = __import__("json").loads(
            (self.output_dir / "campaign_dump_verification.json").read_text(encoding="utf-8")
        )
        self.assertEqual(
            verification["reconstructed_elf"]["output_elf"],
            str(self.output_dir / "campaign_tu116_runtime.elf"),
        )

    def test_invalid_capture_inputs_fail_before_analysis(self):
        with patch.object(analyzer, "verify_dump") as verify:
            with self.assertRaises(FileNotFoundError):
                self.run_analyzer(campaign_dumps=self.root / "missing")
            verify.assert_not_called()

    def test_verification_failure_does_not_write_a_success_summary(self):
        with patch.object(analyzer, "verify_dump", side_effect=ValueError("bad TU profile")):
            with self.assertRaisesRegex(ValueError, "bad TU profile"):
                analyzer.analyze(
                    self.campaign_self,
                    self.campaign_dumps,
                    self.multiplayer_self,
                    self.multiplayer_dumps,
                    self.output_dir,
                    map_file=self.map_file,
                )
        self.assertFalse((self.output_dir / "capture_summary.json").exists())


if __name__ == "__main__":
    unittest.main()
