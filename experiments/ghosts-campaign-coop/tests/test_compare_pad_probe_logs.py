import tempfile
import unittest
from pathlib import Path

from tools.compare_pad_probe_logs import compare, parse_log


CHECKPOINTS = (0, 5, 10, 15, 20, 25)


def write_log(
    root: Path,
    name: str,
    samples,
    mode=None,
    include_ports=True,
    *,
    checkpoint_indices=CHECKPOINTS,
    finished=True,
    probe_version=4,
    read_only=True,
    mode_source="manual_mode_file",
    operator_selected=True,
):
    path = root / name
    lines = [f"Ghosts Campaign Co-op Pad Probe v{probe_version}"]
    if read_only:
        lines.append("READ_ONLY=1")
    if mode:
        lines.extend([
            f"MODE={mode}",
            f"MODE_SOURCE={mode_source}",
            f"MODE_LABEL_IS_OPERATOR_SELECTED={1 if operator_selected else 0}",
        ])
    for position, sample_index in enumerate(checkpoint_indices):
        if not samples:
            continue
        mask = samples[min(position, len(samples) - 1)]
        lines.append(
            f"sample={sample_index} result=OK max_connect=7 now_connect={mask.bit_count()} "
            f"system_info=0x00000000 port_mask=0x{mask:02x}"
        )
    if include_ports:
        lines.extend([
            "  port0 status=0x00000001 capability=0x00000011 device_type=0 CONNECTED",
            "  port1 status=0x00000001 capability=0x00000011 device_type=0 CONNECTED",
        ])
    if finished:
        lines.append("Pad probe finished.")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


class PadProbeComparatorTests(unittest.TestCase):
    def test_both_modes_report_both_ports_in_same_sample(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", [0x01, 0x03], "CAMPAIGN"))
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x03], "MULTIPLAYER"))
            result = compare(campaign, multiplayer)
            self.assertEqual(result["status"], "BOTH_CONTROLLERS_SIMULTANEOUSLY_VISIBLE_IN_BOTH")
            self.assertEqual(campaign["simultaneous_samples"], [5, 10, 15, 20, 25])
            self.assertTrue(result["capture_complete_in_both_modes"])
            self.assertTrue(result["capture_provenance_valid_in_both_modes"])

    def test_campaign_port_enumeration_gap(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", [0x01], "CAMPAIGN"))
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x03], "MULTIPLAYER"))
            self.assertEqual(compare(campaign, multiplayer)["status"], "CAMPAIGN_PAD_ENUMERATION_DIFFERS")

    def test_port_one_seen_at_different_time_is_not_simultaneous(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", [0x01, 0x02], "CAMPAIGN"))
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x01, 0x02], "MULTIPLAYER"))
            result = compare(campaign, multiplayer)
            self.assertTrue(campaign["port0_seen"])
            self.assertTrue(campaign["port1_seen"])
            self.assertFalse(campaign["both_controllers_simultaneously_seen"])
            self.assertEqual(result["status"], "PORT1_SEEN_BUT_NOT_SIMULTANEOUS_WITH_PORT0")

    def test_mode_provenance_is_explicitly_operator_selected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", [0x03], "CAMPAIGN"))
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x03], "MULTIPLAYER"))
            result = compare(campaign, multiplayer)
            self.assertEqual(campaign["mode_source"], "manual_mode_file")
            self.assertTrue(campaign["mode_label_is_operator_selected"])
            self.assertTrue(result["mode_labels_are_operator_selected"])
            self.assertTrue(result["mode_label_provenance"]["campaign"]["operator_selected"])
            self.assertIn("not auto-detected", result["interpretation_limit"])

    def test_manual_mode_provenance_requires_both_logs(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", [0x03], "CAMPAIGN"))
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x03], "MULTIPLAYER"))
            multiplayer["mode_label_is_operator_selected"] = None
            result = compare(campaign, multiplayer)
            self.assertFalse(result["mode_labels_are_operator_selected"])
            self.assertTrue(result["mode_label_provenance"]["campaign"]["operator_selected"])
            self.assertIsNone(result["mode_label_provenance"]["multiplayer"]["operator_selected"])
            self.assertEqual(result["status"], "INCONCLUSIVE_UNVERIFIED_CAPTURE_METADATA")

    def test_mislabeled_modes_are_inconclusive(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", [0x03], "MULTIPLAYER"))
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x03], "CAMPAIGN"))
            self.assertEqual(compare(campaign, multiplayer)["status"], "INCONCLUSIVE_MODE_LABEL_MISMATCH")

    def test_missing_valid_samples_are_inconclusive(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign_path = root / "campaign.log"
            campaign_path.write_text(
                "Ghosts Campaign Co-op Pad Probe v4\n"
                "READ_ONLY=1\n"
                "MODE=CAMPAIGN\n"
                "MODE_SOURCE=manual_mode_file\n"
                "MODE_LABEL_IS_OPERATOR_SELECTED=1\n"
                "sample=0 result=ERROR rc=0x80000001\n"
                "Pad probe finished.\n",
                encoding="utf-8",
            )
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x03], "MULTIPLAYER"))
            self.assertEqual(
                compare(parse_log(campaign_path), multiplayer)["status"],
                "INCONCLUSIVE_MISSING_VALID_SAMPLES",
            )

    def test_legacy_unlabelled_logs_do_not_claim_verified_mode_provenance(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", [0x03]))
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x03]))
            result = compare(campaign, multiplayer)
            self.assertEqual(result["status"], "INCONCLUSIVE_UNVERIFIED_CAPTURE_METADATA")
            self.assertTrue(result["capture_complete_in_both_modes"])
            self.assertFalse(result["capture_provenance_valid_in_both_modes"])

    def test_truncated_capture_does_not_pass_on_one_simultaneous_sample(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(
                root, "campaign.log", [0x03], "CAMPAIGN", checkpoint_indices=(0,)
            ))
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x03], "MULTIPLAYER"))
            result = compare(campaign, multiplayer)
            self.assertTrue(campaign["both_controllers_simultaneously_seen"])
            self.assertEqual(campaign["missing_valid_checkpoints"], [5, 10, 15, 20, 25])
            self.assertFalse(campaign["capture_complete"])
            self.assertEqual(result["status"], "INCONCLUSIVE_INCOMPLETE_CAPTURE")

    def test_missing_completion_marker_is_inconclusive(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(
                root, "campaign.log", [0x03], "CAMPAIGN", finished=False
            ))
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x03], "MULTIPLAYER"))
            self.assertFalse(campaign["capture_complete"])
            self.assertEqual(compare(campaign, multiplayer)["status"], "INCONCLUSIVE_INCOMPLETE_CAPTURE")

    def test_duplicate_sample_index_invalidates_capture(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign_path = write_log(root, "campaign.log", [0x03], "CAMPAIGN")
            with campaign_path.open("a", encoding="utf-8") as stream:
                stream.write(
                    "sample=0 result=OK max_connect=7 now_connect=2 "
                    "system_info=0x00000000 port_mask=0x03\n"
                )
            campaign = parse_log(campaign_path)
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x03], "MULTIPLAYER"))
            self.assertEqual(campaign["duplicate_sample_indices"], [0])
            self.assertEqual(compare(campaign, multiplayer)["status"], "INCONCLUSIVE_INCOMPLETE_CAPTURE")

    def test_failed_periodic_checkpoint_invalidates_capture(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign_path = write_log(root, "campaign.log", [0x03], "CAMPAIGN")
            original = campaign_path.read_text(encoding="utf-8")
            original = original.replace(
                "sample=10 result=OK max_connect=7 now_connect=2 "
                "system_info=0x00000000 port_mask=0x03",
                "sample=10 result=ERROR rc=0x80000001",
            )
            campaign_path.write_text(original, encoding="utf-8")
            campaign = parse_log(campaign_path)
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x03], "MULTIPLAYER"))
            self.assertIn(10, campaign["missing_valid_checkpoints"])
            self.assertEqual(compare(campaign, multiplayer)["status"], "INCONCLUSIVE_INCOMPLETE_CAPTURE")

    def test_wrong_probe_version_or_missing_read_only_header_invalidates_capture(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            old_version = parse_log(write_log(
                root, "old-version.log", [0x03], "CAMPAIGN", probe_version=3
            ))
            no_read_only = parse_log(write_log(
                root, "no-read-only.log", [0x03], "CAMPAIGN", read_only=False
            ))
            self.assertFalse(old_version["capture_complete"])
            self.assertFalse(no_read_only["capture_complete"])


if __name__ == "__main__":
    unittest.main()
