import tempfile
import unittest
from pathlib import Path

from tools.ps3mapi.verify_capture_metadata import verify_capture_metadata


class CaptureMetadataVerificationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.campaign_dir = self.root / "campaign"
        self.multiplayer_dir = self.root / "multiplayer"
        self.campaign_dir.mkdir()
        self.multiplayer_dir.mkdir()
        self.campaign_log = self.root / "ghosts_campaign_pad_probe.log"
        self.multiplayer_log = self.root / "ghosts_multiplayer_pad_probe.log"
        self.campaign_log.write_text("campaign probe fixture\n", encoding="utf-8")
        self.multiplayer_log.write_text("multiplayer probe fixture\n", encoding="utf-8")
        (self.campaign_dir / "GHOSTS_TU116_CAMPAIGN_SEG0.bin").write_bytes(b"campaign0")
        (self.campaign_dir / "GHOSTS_TU116_CAMPAIGN_SEG1.bin").write_bytes(b"campaign1")
        (self.multiplayer_dir / "GHOSTS_TU116_MULTIPLAYER_SEG0.bin").write_bytes(b"multiplayer0")
        (self.multiplayer_dir / "GHOSTS_TU116_MULTIPLAYER_SEG1.bin").write_bytes(b"multiplayer1")
        self.campaign_meta = self.root / "campaign_capture_metadata.txt"
        self.multiplayer_meta = self.root / "multiplayer_capture_metadata.txt"
        self.write_metadata(self.campaign_meta, "CAMPAIGN", 1234, "default.self")
        self.write_metadata(self.multiplayer_meta, "MULTIPLAYER", 1234, "default_mp.self")

    def tearDown(self):
        self.temp.cleanup()

    def write_metadata(self, path, mode, pid, process_name):
        prefix = mode
        lines = [
            "# metadata fixture",
            "profile_title_id=BLUS31270",
            "profile_app_ver=01.16",
            f"operator_selected_mode={mode}",
            "capture_date_local=2026-11-25 00:30:00",
            f"ps3mapi_current_pid={pid}",
            f"ps3mapi_process_name_verbatim={process_name}",
            f"pad_probe_log_filename=ghosts_{mode.lower()}_pad_probe.log",
            f"segment0_filename=GHOSTS_TU116_{prefix}_SEG0.bin",
            f"segment1_filename=GHOSTS_TU116_{prefix}_SEG1.bin",
            "notes=process name is diagnostic only",
        ]
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    def test_valid_campaign_metadata_matches_real_input_filenames(self):
        result = verify_capture_metadata(
            self.campaign_meta, "campaign", self.campaign_dir, pad_log=self.campaign_log
        )
        self.assertEqual(result["status"], "PASS_CAPTURE_METADATA")
        self.assertEqual(result["ps3mapi_pid"], 1234)
        self.assertEqual(result["ps3mapi_process_name_verbatim"], "default.self")
        self.assertTrue(result["process_name_is_diagnostic_only"])
        self.assertEqual(result["segment_files"]["0"]["size_bytes"], len(b"campaign0"))
        self.assertEqual(len(result["metadata_sha256"]), 64)

    def test_valid_multiplayer_metadata_accepts_same_pid_and_process_name(self):
        self.write_metadata(
            self.multiplayer_meta, "MULTIPLAYER", 1234, "default.self"
        )
        result = verify_capture_metadata(
            self.multiplayer_meta, "multiplayer", self.multiplayer_dir,
            pad_log=self.multiplayer_log,
        )
        self.assertEqual(result["status"], "PASS_CAPTURE_METADATA")
        self.assertEqual(result["ps3mapi_pid"], 1234)
        self.assertEqual(result["ps3mapi_process_name_verbatim"], "default.self")

    def test_wrong_mode_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "mode mismatch"):
            verify_capture_metadata(
                self.campaign_meta, "multiplayer", self.multiplayer_dir
            )

    def test_wrong_title_id_is_rejected(self):
        text = self.campaign_meta.read_text(encoding="utf-8").replace(
            "profile_title_id=BLUS31270", "profile_title_id=BLES00000"
        )
        self.campaign_meta.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "profile_title_id mismatch"):
            verify_capture_metadata(self.campaign_meta, "campaign", self.campaign_dir)

    def test_wrong_update_is_rejected(self):
        text = self.campaign_meta.read_text(encoding="utf-8").replace(
            "profile_app_ver=01.16", "profile_app_ver=01.00"
        )
        self.campaign_meta.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "profile_app_ver mismatch"):
            verify_capture_metadata(self.campaign_meta, "campaign", self.campaign_dir)

    def test_invalid_pid_values_are_rejected(self):
        for value in ("", "0", "-1", "abc", "4294967296"):
            with self.subTest(value=value):
                text = self.campaign_meta.read_text(encoding="utf-8")
                lines = [
                    "ps3mapi_current_pid=" + value if line.startswith("ps3mapi_current_pid=") else line
                    for line in text.splitlines()
                ]
                self.campaign_meta.write_text("\n".join(lines) + "\n", encoding="utf-8")
                with self.assertRaises(ValueError):
                    verify_capture_metadata(self.campaign_meta, "campaign", self.campaign_dir)
                self.write_metadata(self.campaign_meta, "CAMPAIGN", 1234, "default.self")

    def test_empty_process_name_is_rejected(self):
        text = self.campaign_meta.read_text(encoding="utf-8").replace(
            "ps3mapi_process_name_verbatim=default.self",
            "ps3mapi_process_name_verbatim="
        )
        self.campaign_meta.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "missing required metadata"):
            verify_capture_metadata(self.campaign_meta, "campaign", self.campaign_dir)

    def test_invalid_capture_timestamp_is_rejected(self):
        text = self.campaign_meta.read_text(encoding="utf-8").replace(
            "capture_date_local=2026-11-25 00:30:00",
            "capture_date_local=tomorrow"
        )
        self.campaign_meta.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "YYYY-MM-DD HH:MM:SS"):
            verify_capture_metadata(self.campaign_meta, "campaign", self.campaign_dir)

    def test_duplicate_and_unknown_keys_are_rejected(self):
        with self.subTest("duplicate"):
            text = self.campaign_meta.read_text(encoding="utf-8") + "profile_app_ver=01.16\n"
            self.campaign_meta.write_text(text, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate metadata key"):
                verify_capture_metadata(self.campaign_meta, "campaign", self.campaign_dir)
        with self.subTest("unknown"):
            self.write_metadata(self.campaign_meta, "CAMPAIGN", 1234, "default.self")
            text = self.campaign_meta.read_text(encoding="utf-8") + "pid=1234\n"
            self.campaign_meta.write_text(text, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "unknown metadata key"):
                verify_capture_metadata(self.campaign_meta, "campaign", self.campaign_dir)

    def test_wrong_segment_filename_is_rejected(self):
        text = self.campaign_meta.read_text(encoding="utf-8").replace(
            "segment0_filename=GHOSTS_TU116_CAMPAIGN_SEG0.bin",
            "segment0_filename=GHOSTS_TU116_MULTIPLAYER_SEG0.bin"
        )
        self.campaign_meta.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "segment0 filename mismatch"):
            verify_capture_metadata(self.campaign_meta, "campaign", self.campaign_dir)

    def test_missing_segment_file_is_rejected(self):
        (self.campaign_dir / "GHOSTS_TU116_CAMPAIGN_SEG1.bin").unlink()
        with self.assertRaisesRegex(FileNotFoundError, "SEG1.bin"):
            verify_capture_metadata(self.campaign_meta, "campaign", self.campaign_dir)

    def test_wrong_pad_log_filename_is_rejected(self):
        wrong_log = self.root / "campaign.log"
        wrong_log.write_text("fixture", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Pad Probe log filename mismatch"):
            verify_capture_metadata(
                self.campaign_meta, "campaign", self.campaign_dir, pad_log=wrong_log
            )

    def test_metadata_parser_rejects_malformed_lines(self):
        text = self.campaign_meta.read_text(encoding="utf-8") + "not a key value line\n"
        self.campaign_meta.write_text(text, encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "expected key=value"):
            verify_capture_metadata(self.campaign_meta, "campaign", self.campaign_dir)

    def test_missing_metadata_file_is_rejected(self):
        with self.assertRaises(FileNotFoundError):
            verify_capture_metadata(
                self.root / "missing.txt", "campaign", self.campaign_dir
            )


if __name__ == "__main__":
    unittest.main()
