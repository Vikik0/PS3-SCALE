import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import run_verified_mission_gsc_analysis as runner
from verify_ghosts_mission_fastfiles import EXPECTED_FASTFILES


class VerifiedMissionGscAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.fastfiles = {}
        self.expectations = {}
        for role in ("black_ice", "carrier", "cornered"):
            payload = b"IWff0100" + bytes.fromhex("0000022E") + role.encode("ascii")
            path = self.root / f"{role}.ff"
            path.write_bytes(payload)
            self.fastfiles[role] = path
            self.expectations[role] = {
                "file_name": path.name,
                "size_bytes": len(payload),
                "sha256": hashlib.sha256(payload).hexdigest(),
                "magic": "IWff0100",
                "version": "0x0000022E",
            }
        self.missions = {}
        for role in ("black_ice", "carrier", "cornered"):
            root = self.root / role
            root.mkdir()
            (root / "main.gsc").write_text(
                "magicreloadwhenreachenemy()\n{\n    return;\n}\n"
                "container_scan()\n{\n    foreach (player in level.playerpiggyback) { }\n}\n",
                encoding="utf-8",
            )
            self.missions[role] = root

    def tearDown(self):
        self.temp.cleanup()

    def test_runs_only_when_all_exact_fastfiles_match_and_rechecks_after(self):
        output = self.root / "verified-analysis.json"
        with patch.dict(runner.EXPECTED_FASTFILES, self.expectations, clear=True):
            report = runner.run_verified_analysis(self.fastfiles, self.missions, output)
        self.assertEqual(report["status"], "VERIFIED_MISSION_GSC_ANALYSIS_COMPLETED")
        self.assertEqual(
            report["fastfile_verification"]["before"]["status"],
            "MATCHED_MISSION_FASTFILES",
        )
        self.assertEqual(
            report["fastfile_verification"]["after"]["status"],
            "MATCHED_MISSION_FASTFILES",
        )
        self.assertEqual(
            report["mission_analysis"]["summary"]["player_container_function_count"],
            3,
        )
        saved = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(saved["status"], report["status"])

    def test_fastfile_mismatch_blocks_gsc_analysis(self):
        changed = self.fastfiles["carrier"]
        changed.write_bytes(changed.read_bytes() + b"changed")
        with patch.dict(runner.EXPECTED_FASTFILES, self.expectations, clear=True), patch.object(
            runner, "run_analysis"
        ) as analyze:
            report = runner.run_verified_analysis(
                self.fastfiles, self.missions, self.root / "blocked.json"
            )
        self.assertEqual(report["status"], "BLOCKED_FASTFILE_IDENTITY_MISMATCH")
        self.assertIsNone(report["mission_analysis"])
        analyze.assert_not_called()

    def test_fastfile_change_during_analysis_blocks_final_status(self):
        matched = {
            "status": "MATCHED_MISSION_FASTFILES",
            "errors": [],
            "files": {
                role: {
                    "status": "MATCHED", "size_bytes": 100,
                    "sha256": role, "magic": "IWff0100", "version": "0x0000022E",
                }
                for role in self.fastfiles
            },
        }
        changed = {
            "status": "MISMATCH_OR_INCOMPLETE",
            "errors": ["carrier: FastFile changed"],
            "files": {
                **matched["files"],
                "carrier": {**matched["files"]["carrier"], "status": "MISMATCH", "sha256": "changed"},
            },
        }
        mission_report = {
            "status": "PLAYER_CONTAINER_CALLGRAPH_COMPLETED",
            "summary": {"source_tree_sha256": {"carrier": "abc"}},
        }
        with patch.object(runner, "verify_fastfiles", side_effect=[matched, changed]), patch.object(
            runner, "run_analysis", return_value=mission_report
        ):
            report = runner.run_verified_analysis(self.fastfiles, self.missions)
        self.assertEqual(report["status"], "BLOCKED_INPUTS_CHANGED_OR_ANALYSIS_PARTIAL")
        self.assertIn("changed", report["errors"][0])

    def test_never_overwrites_fastfile_input(self):
        output = self.fastfiles["black_ice"]
        with patch.dict(runner.EXPECTED_FASTFILES, self.expectations, clear=True):
            with self.assertRaisesRegex(ValueError, "cannot overwrite the black_ice FastFile"):
                runner.run_verified_analysis(
                    self.fastfiles, self.missions, output, overwrite=True
                )

    def test_never_overwrites_gsc_source_file(self):
        output = self.missions["carrier"] / "main.gsc"
        with patch.dict(runner.EXPECTED_FASTFILES, self.expectations, clear=True):
            with self.assertRaisesRegex(ValueError, "cannot overwrite a carrier GSC source"):
                runner.run_verified_analysis(
                    self.fastfiles, self.missions, output, overwrite=True
                )

    def test_rejects_missing_or_extra_mission_gsc_roles(self):
        with self.assertRaisesRegex(ValueError, "exactly these mission GSC roles"):
            runner.run_verified_analysis(
                self.fastfiles,
                {"carrier": self.missions["carrier"]},
            )

    def test_rejects_missing_or_extra_fastfile_roles(self):
        with self.assertRaisesRegex(ValueError, "provide exactly these FastFile roles"):
            runner.run_verified_analysis(
                {"black_ice": self.fastfiles["black_ice"]},
                self.missions,
            )


if __name__ == "__main__":
    unittest.main()
