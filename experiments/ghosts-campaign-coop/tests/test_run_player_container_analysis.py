import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import run_player_container_analysis as runner


class PlayerContainerAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.mission = self.root / "carrier"
        (self.mission / "maps").mkdir(parents=True)
        (self.mission / "maps" / "main.gsc").write_text(
            "magicreloadwhenreachenemy()\n{\n    thread container_scan();\n}\n"
            "container_scan()\n{\n"
            "    foreach (player in level.playerpiggyback)\n"
            "    {\n        player waittill(\"death\");\n    }\n"
            "}\n",
            encoding="utf-8",
        )

    def tearDown(self):
        self.temp.cleanup()

    def test_runs_ownership_and_exact_callgraph_from_same_sources(self):
        report = runner.run_analysis({"carrier": self.mission})
        self.assertEqual(report["status"], "PLAYER_CONTAINER_CALLGRAPH_COMPLETED")
        self.assertEqual(report["summary"]["source_file_count"], 1)
        self.assertEqual(report["summary"]["player_container_function_count"], 1)
        self.assertEqual(report["summary"]["exact_targets_found"], 1)
        self.assertEqual(report["summary"]["incoming_call_site_count"], 1)
        finding = report["callgraph_audit"]["missions"][0]["target_findings"][0]
        self.assertEqual(finding["definition"]["path"], "maps/main.gsc")
        self.assertEqual(finding["call_site_count"], 1)
        self.assertTrue(finding["reachable_from_script_roots_without_callbacks"])
        self.assertEqual(
            report["source_manifests"]["carrier"]["files"][0]["path"],
            "maps/main.gsc",
        )

    def test_duplicate_function_names_remain_separate_by_file(self):
        (self.mission / "helpers").mkdir()
        (self.mission / "helpers" / "helper.gsc").write_text(
            "container_scan()\n{\n    foreach (player in level.playerpiggyback) { }\n}\n",
            encoding="utf-8",
        )
        report = runner.run_analysis({"carrier": self.mission})
        findings = report["callgraph_audit"]["missions"][0]["target_findings"]
        self.assertEqual(len(findings), 2)
        by_file = {item["definition"]["path"]: item for item in findings}
        self.assertEqual(by_file["maps/main.gsc"]["call_site_count"], 1)
        self.assertEqual(by_file["helpers/helper.gsc"]["call_site_count"], 0)

    def test_detects_source_mutation_between_audits_and_writes_no_report(self):
        output = self.root / "combined.json"
        original = runner.audit_callgraph

        def mutate_after_audit(missions, target_definitions=None):
            result = original(missions, target_definitions=target_definitions)
            source = self.mission / "maps" / "main.gsc"
            source.write_text(source.read_text(encoding="utf-8") + "\n// changed\n", encoding="utf-8")
            return result

        with patch.object(runner, "audit_callgraph", side_effect=mutate_after_audit):
            with self.assertRaisesRegex(ValueError, "source set changed during analysis"):
                runner.run_analysis({"carrier": self.mission}, output)
        self.assertFalse(output.exists())

    def test_never_overwrites_source_gsc_even_with_overwrite_flag(self):
        source = self.mission / "maps" / "main.gsc"
        original = source.read_bytes()
        with self.assertRaisesRegex(ValueError, "cannot overwrite a GSC source file"):
            runner.run_analysis({"carrier": self.mission}, source, overwrite=True)
        self.assertEqual(source.read_bytes(), original)

    def test_rejects_existing_output_without_overwrite(self):
        output = self.root / "existing.json"
        output.write_text("keep", encoding="utf-8")
        with self.assertRaisesRegex(FileExistsError, "refusing to overwrite"):
            runner.run_analysis({"carrier": self.mission}, output)
        self.assertEqual(output.read_text(encoding="utf-8"), "keep")

    def test_hash_manifest_changes_when_a_source_file_is_added(self):
        before = runner.hash_gsc_tree(self.mission)
        (self.mission / "extra.gsc").write_text("helper()\n{\n return;\n}\n", encoding="utf-8")
        after = runner.hash_gsc_tree(self.mission)
        self.assertNotEqual(before["sha256"], after["sha256"])
        self.assertEqual(after["file_count"], 2)

    def test_atomic_report_writer_creates_json_and_refuses_second_write(self):
        output = self.root / "reports" / "analysis.json"
        report = runner.run_analysis({"carrier": self.mission}, output)
        self.assertTrue(output.is_file())
        import json
        saved = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(saved["summary"], report["summary"])
        self.assertEqual(saved["status"], "PLAYER_CONTAINER_CALLGRAPH_COMPLETED")
        self.assertEqual(report["status"], "PLAYER_CONTAINER_CALLGRAPH_COMPLETED")
        with self.assertRaises(FileExistsError):
            runner.run_analysis({"carrier": self.mission}, output)


    def test_rejects_symbolic_link_gsc_source(self):
        outside = self.root / "outside.gsc"
        outside.write_text("external_helper()\\n{\\n return;\\n}\\n", encoding="utf-8")
        link = self.mission / "linked.gsc"
        try:
            link.symlink_to(outside)
        except (OSError, NotImplementedError) as exc:
            self.skipTest(f"symbolic links unavailable: {exc}")
        with self.assertRaisesRegex(ValueError, "symbolic-link GSC inputs"):
            runner.hash_gsc_tree(self.mission)


if __name__ == "__main__":
    unittest.main()
