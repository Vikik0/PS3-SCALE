import tempfile
import unittest
from pathlib import Path

from tools.compare_pad_probe_logs import compare, parse_log

def write_log(root: Path, name: str, samples, mode=None, include_ports=True):
    path = root / name
    lines = ["Ghosts Campaign Co-op Pad Probe v4", "READ_ONLY=1"]
    if mode:
        lines.extend([
            f"MODE={mode}",
            "MODE_SOURCE=manual_mode_file",
            "MODE_LABEL_IS_OPERATOR_SELECTED=1",
        ])
    for index, mask in enumerate(samples):
        lines.append(
            f"sample={index} result=OK max_connect=7 now_connect={mask.bit_count()} "
            f"system_info=0x00000000 port_mask=0x{mask:02x}"
        )
    if include_ports:
        lines.extend([
            "  port0 status=0x00000001 capability=0x00000011 device_type=0 CONNECTED",
            "  port1 status=0x00000001 capability=0x00000011 device_type=0 CONNECTED",
        ])
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
            self.assertEqual(campaign["simultaneous_samples"], [1])

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
            campaign_path.write_text("MODE=CAMPAIGN\nsample=0 result=ERROR rc=0x80000001\n")
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x03], "MULTIPLAYER"))
            self.assertEqual(compare(parse_log(campaign_path), multiplayer)["status"], "INCONCLUSIVE_MISSING_VALID_SAMPLES")

    def test_legacy_unlabelled_logs_remain_supported(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", [0x03]))
            multiplayer = parse_log(write_log(root, "multiplayer.log", [0x03]))
            self.assertEqual(compare(campaign, multiplayer)["status"], "BOTH_CONTROLLERS_SIMULTANEOUSLY_VISIBLE_IN_BOTH")

if __name__ == "__main__":
    unittest.main()
