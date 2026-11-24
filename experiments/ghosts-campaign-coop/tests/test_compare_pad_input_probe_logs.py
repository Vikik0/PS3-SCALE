import tempfile
import unittest
from pathlib import Path

from tools.compare_pad_input_probe_logs import compare, parse_log


def write_log(root: Path, filename: str, mode: str, *, distinct=True, active=True, connected=True, finished=True):
    path = root / filename
    lines = [
        "Ghosts DualShock Input Probe v1",
        f"MODE={mode}",
        "READ_ONLY=1",
        "MODE_SOURCE=manual_mode_file",
        "MODE_LABEL_IS_OPERATOR_SELECTED=1",
        "CALLS=cellPadGetInfo2,cellPadGetData",
        "NO_CELL_PAD_INIT_END=1",
        "NO_GAME_MEMORY_WRITES=1",
        "NO_HOOKS=1",
    ]
    for sample in range(30):
        mask = 0x03 if connected else 0x01
        lines.append(
            f"sample={sample} result=OK connected_mask=0x{mask:08x} connected_count={2 if connected else 1} "
            "read_error_mask=0x00000000 valid_mask=0x00000003 frame_connected_mask=0x00000003"
        )
        p0_buttons = 0x00000200 if active else 0
        p1_buttons = (0x00000400 if distinct else p0_buttons) if active else 0
        if sample < 4:
            p0_buttons = 0
            p1_buttons = 0
        lines.append(
            f"  sample={sample} port=0 connected={1 if connected else 1} read_error=0 "
            f"digital1=0x00000000 digital2=0x00000040 buttons=0x{p0_buttons:08x} "
            f"pressed=0x{p0_buttons:08x} released=0x00000000 left_x=128 left_y=128 right_x=128 right_y=128 "
            "norm_left_x=0 norm_left_y=0 norm_right_x=0 norm_right_y=0 l2_pressure=0 r2_pressure=0"
        )
        lines.append(
            f"  sample={sample} port=1 connected={1 if connected else 0} read_error=0 "
            f"digital1=0x00000000 digital2=0x00000020 buttons=0x{p1_buttons:08x} "
            f"pressed=0x{p1_buttons:08x} released=0x00000000 left_x=128 left_y=128 right_x=128 right_y=128 "
            "norm_left_x=0 norm_left_y=0 norm_right_x=0 norm_right_y=0 l2_pressure=0 r2_pressure=0"
        )
    if finished:
        lines.append("Pad input probe finished.")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


class PadInputProbeComparatorTests(unittest.TestCase):
    def test_independent_input_is_confirmed_in_both_modes(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", "CAMPAIGN"))
            multiplayer = parse_log(write_log(root, "multiplayer.log", "MULTIPLAYER"))
            result = compare(campaign, multiplayer)
            self.assertEqual(result["status"], "INDEPENDENT_PORT_INPUT_OBSERVED_IN_BOTH_MODES")
            self.assertTrue(campaign["both_ports_simultaneously_connected"])
            self.assertTrue(campaign["independent_input_observed"])
            self.assertEqual(campaign["independent_button_samples"], list(range(4, 30)))

    def test_same_button_state_on_both_ports_is_not_independence_evidence(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", "CAMPAIGN", distinct=False))
            multiplayer = parse_log(write_log(root, "multiplayer.log", "MULTIPLAYER", distinct=False))
            self.assertFalse(campaign["independent_input_observed"])
            self.assertEqual(compare(campaign, multiplayer)["status"], "INDEPENDENT_INPUT_NOT_PROVEN_SIMULTANEOUSLY")

    def test_one_port_without_button_activity_is_inconclusive(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", "CAMPAIGN", active=False))
            multiplayer = parse_log(write_log(root, "multiplayer.log", "MULTIPLAYER", active=False))
            self.assertFalse(campaign["port_button_activity"]["0"])
            self.assertEqual(compare(campaign, multiplayer)["status"], "CAMPAIGN_BUTTON_ACTIVITY_MISSING_ON_ONE_OR_BOTH_PORTS")

    def test_non_simultaneous_ports_do_not_pass(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", "CAMPAIGN", connected=False))
            multiplayer = parse_log(write_log(root, "multiplayer.log", "MULTIPLAYER", connected=False))
            self.assertFalse(campaign["both_ports_simultaneously_connected"])
            self.assertEqual(compare(campaign, multiplayer)["status"], "BOTH_PORTS_NOT_SIMULTANEOUSLY_CONNECTED_IN_BOTH_MODES")

    def test_wrong_mode_label_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", "MULTIPLAYER"))
            multiplayer = parse_log(write_log(root, "multiplayer.log", "CAMPAIGN"))
            self.assertEqual(compare(campaign, multiplayer)["status"], "INCONCLUSIVE_MODE_LABEL_MISMATCH")

    def test_incomplete_capture_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", "CAMPAIGN", finished=False))
            multiplayer = parse_log(write_log(root, "multiplayer.log", "MULTIPLAYER"))
            self.assertFalse(campaign["capture_complete"])
            self.assertEqual(compare(campaign, multiplayer)["status"], "INCONCLUSIVE_INCOMPLETE_OR_UNVERIFIED_INPUT_CAPTURE")

    def test_duplicate_sample_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign_path = write_log(root, "campaign.log", "CAMPAIGN")
            with campaign_path.open("a", encoding="utf-8") as stream:
                stream.write(
                    "sample=0 result=OK connected_mask=0x00000003 connected_count=2 "
                    "read_error_mask=0x00000000 valid_mask=0x00000003 frame_connected_mask=0x00000003\n"
                )
            campaign = parse_log(campaign_path)
            multiplayer = parse_log(write_log(root, "multiplayer.log", "MULTIPLAYER"))
            self.assertFalse(campaign["capture_complete"])
            self.assertEqual(compare(campaign, multiplayer)["status"], "INCONCLUSIVE_INCOMPLETE_OR_UNVERIFIED_INPUT_CAPTURE")

    def test_read_error_invalidates_capture(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign_path = write_log(root, "campaign.log", "CAMPAIGN")
            text = campaign_path.read_text(encoding="utf-8").replace(
                "port=1 connected=1 read_error=0",
                "port=1 connected=0 read_error=1",
                1,
            )
            campaign_path.write_text(text, encoding="utf-8")
            campaign = parse_log(campaign_path)
            self.assertFalse(campaign["capture_complete"])

    def test_missing_port_line_invalidates_capture(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign_path = write_log(root, "campaign.log", "CAMPAIGN")
            lines = campaign_path.read_text(encoding="utf-8").splitlines()
            campaign_path.write_text("\n".join(line for line in lines if "sample=5 port=1 " not in line) + "\n", encoding="utf-8")
            campaign = parse_log(campaign_path)
            self.assertFalse(campaign["capture_complete"])


if __name__ == "__main__":
    unittest.main()
