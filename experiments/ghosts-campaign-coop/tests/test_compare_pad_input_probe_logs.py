import tempfile
import unittest
from pathlib import Path

from tools.compare_pad_input_probe_logs import compare, parse_log


def write_log(root: Path, filename: str, mode: str, *, distinct=True, active=True, connected=True, finished=True, axes=False):
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
    previous = {"0": 0, "1": 0}
    for sample in range(30):
        mask = 0x03 if connected else 0x01
        lines.append(
            f"sample={sample} result=OK connected_mask=0x{mask:08x} connected_count={2 if connected else 1} "
            f"read_error_mask=0x00000000 valid_mask=0x00000003 frame_connected_mask=0x{mask:08x}"
        )
        p0_buttons = 0x00000200 if active else 0
        p1_buttons = (0x00000400 if distinct else p0_buttons) if active else 0
        if sample < 4:
            p0_buttons = 0
            p1_buttons = 0
        if not connected:
            p1_buttons = 0
        p0_digital2 = 0x40 if p0_buttons & 0x00000200 else 0
        p1_digital2 = (
            0x20 if p1_buttons & 0x00000400
            else 0x40 if p1_buttons & 0x00000200
            else 0
        )
        p0_pressed = p0_buttons & ~previous["0"]
        p0_released = previous["0"] & ~p0_buttons
        p1_pressed = p1_buttons & ~previous["1"]
        p1_released = previous["1"] & ~p1_buttons
        previous["0"] = p0_buttons
        previous["1"] = p1_buttons
        p0_axis = (49, 0, 0, 0) if axes and sample >= 4 else (0, 0, 0, 0)
        p1_axis = (0, -50, 0, 0) if axes and sample >= 4 else (0, 0, 0, 0)
        p0_raw_axes = (190, 128, 128, 128) if axes and sample >= 4 else (128, 128, 128, 128)
        p1_raw_axes = (
            (128, 65, 128, 128) if axes and sample >= 4
            else (128, 128, 128, 128) if connected
            else (0, 0, 0, 0)
        )
        lines.append(
            f"  sample={sample} port=0 connected=1 read_error=0 "
            f"digital1=0x00000000 digital2=0x{p0_digital2:04x} buttons=0x{p0_buttons:08x} "
            f"pressed=0x{p0_pressed:08x} released=0x{p0_released:08x} left_x={p0_raw_axes[0]} left_y={p0_raw_axes[1]} right_x={p0_raw_axes[2]} right_y={p0_raw_axes[3]} "
            f"norm_left_x={p0_axis[0]} norm_left_y={p0_axis[1]} norm_right_x={p0_axis[2]} norm_right_y={p0_axis[3]} "
            "l2_pressure=0 r2_pressure=0"
        )
        lines.append(
            f"  sample={sample} port=1 connected={1 if connected else 0} read_error=0 "
            f"digital1=0x00000000 digital2=0x{p1_digital2:04x} buttons=0x{p1_buttons:08x} "
            f"pressed=0x{p1_pressed:08x} released=0x{p1_released:08x} left_x={p1_raw_axes[0]} left_y={p1_raw_axes[1]} right_x={p1_raw_axes[2]} right_y={p1_raw_axes[3]} "
            f"norm_left_x={p1_axis[0]} norm_left_y={p1_axis[1]} norm_right_x={p1_axis[2]} norm_right_y={p1_axis[3]} "
            "l2_pressure=0 r2_pressure=0"
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

    def test_raw_digital_buttons_must_match_the_mapped_button_mask(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign_path = write_log(root, "campaign.log", "CAMPAIGN")
            text = campaign_path.read_text(encoding="utf-8").replace(
                "digital2=0x0040 buttons=0x00000200",
                "digital2=0x0040 buttons=0x00000400",
                1,
            )
            campaign_path.write_text(text, encoding="utf-8")
            campaign = parse_log(campaign_path)
            self.assertFalse(campaign["capture_complete"])
            self.assertTrue(any("mapped buttons mask" in item["error"] for item in campaign["port_line_errors"]))

    def test_pressed_and_released_edges_must_match_successful_samples(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign_path = write_log(root, "campaign.log", "CAMPAIGN")
            text = campaign_path.read_text(encoding="utf-8").replace(
                "sample=4 port=0 connected=1 read_error=0 digital1=0x00000000 digital2=0x0040 buttons=0x00000200 pressed=0x00000200",
                "sample=4 port=0 connected=1 read_error=0 digital1=0x00000000 digital2=0x0040 buttons=0x00000200 pressed=0x00000000",
                1,
            )
            campaign_path.write_text(text, encoding="utf-8")
            campaign = parse_log(campaign_path)
            self.assertFalse(campaign["capture_complete"])
            self.assertTrue(any("pressed mask" in item["error"] for item in campaign["consistency_errors"]))

    def test_normalized_axes_must_match_raw_stick_values(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign_path = write_log(root, "campaign.log", "CAMPAIGN", axes=True)
            text = campaign_path.read_text(encoding="utf-8").replace(
                "norm_left_x=49",
                "norm_left_x=50",
                1,
            )
            campaign_path.write_text(text, encoding="utf-8")
            campaign = parse_log(campaign_path)
            self.assertFalse(campaign["capture_complete"])
            self.assertTrue(any("normalized axes do not match raw" in item["error"] for item in campaign["port_line_errors"]))

    def test_different_masks_without_exclusive_button_on_both_ports_do_not_pass(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign_path = write_log(root, "campaign.log", "CAMPAIGN")
            text = campaign_path.read_text(encoding="utf-8").replace(
                "digital2=0x0020 buttons=0x00000400",
                "digital2=0x0060 buttons=0x00000600",
            )
            text = text.replace(
                "sample=4 port=1 connected=1 read_error=0 digital1=0x00000000 digital2=0x0060 buttons=0x00000600 pressed=0x00000400",
                "sample=4 port=1 connected=1 read_error=0 digital1=0x00000000 digital2=0x0060 buttons=0x00000600 pressed=0x00000600",
                1,
            )
            campaign_path.write_text(text, encoding="utf-8")
            campaign = parse_log(campaign_path)
            multiplayer_path = write_log(root, "multiplayer.log", "MULTIPLAYER")
            multiplayer = parse_log(multiplayer_path)
            self.assertTrue(campaign["capture_complete"])
            self.assertFalse(campaign["independent_input_observed"])
            self.assertEqual(compare(campaign, multiplayer)["status"], "CAMPAIGN_INDEPENDENT_INPUT_NOT_PROVEN")

    def test_axis_independence_is_reported_separately_from_button_proof(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign = parse_log(write_log(root, "campaign.log", "CAMPAIGN", active=False, axes=True))
            multiplayer = parse_log(write_log(root, "multiplayer.log", "MULTIPLAYER", active=False, axes=True))
            self.assertEqual(campaign["independent_axis_samples"], list(range(4, 30)))
            self.assertFalse(campaign["independent_input_observed"])
            self.assertEqual(compare(campaign, multiplayer)["status"], "CAMPAIGN_BUTTON_ACTIVITY_MISSING_ON_ONE_OR_BOTH_PORTS")

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
