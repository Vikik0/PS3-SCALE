#!/usr/bin/env python3
"""Compare read-only cellPadGetData telemetry for Campaign and Multiplayer."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

EXPECTED_VERSION = 1
EXPECTED_SAMPLE_COUNT = 30
SAMPLE_RE = re.compile(
    r"^sample=(?P<sample>\d+)\s+result=(?P<result>OK|ERROR)(?P<rest>.*)$",
    re.I,
)
PORT_RE = re.compile(
    r"^sample=(?P<sample>\d+)\s+port=(?P<port>\d+)\s+(?P<rest>.*)$",
    re.I,
)
HEADER_RE = re.compile(r"^Ghosts DualShock Input Probe v(?P<version>\d+)\s*$", re.I)
MODE_RE = re.compile(r"^MODE=(CAMPAIGN|MULTIPLAYER)\s*$", re.I)
KV_RE = re.compile(r"([a-z][a-z0-9_]*)=([^\s]+)", re.I)


def _fields(text: str) -> dict[str, str]:
    return {key.lower(): value for key, value in KV_RE.findall(text)}


def _number(fields: dict[str, str], key: str, base: int = 10) -> int:
    value = fields.get(key)
    if value is None:
        raise ValueError(f"missing {key}")
    return int(value, base)



def _mapped_buttons(digital1: int, digital2: int) -> int:
    """Mirror the portable C decoder's exact DualShock digital-button mapping."""
    mapping = (
        (digital1, ((7, 2), (6, 1), (5, 3), (4, 0), (3, 4), (2, 7), (1, 6), (0, 5))),
        (digital2, ((7, 8), (6, 9), (5, 10), (4, 11), (3, 12), (2, 13), (1, 14), (0, 15))),
    )
    buttons = 0
    for raw, pairs in mapping:
        for raw_bit, mapped_bit in pairs:
            if raw & (1 << raw_bit):
                buttons |= 1 << mapped_bit
    return buttons


def _normalize_axis(raw: int, deadzone: int = 20) -> int:
    """Mirror ghc_normalize_axis() with the v1 probe's fixed default deadzone."""
    delta = raw - 128
    if delta > 0:
        if delta <= deadzone:
            return 0
        magnitude = delta - deadzone
        available = 127 - deadzone
        scaled = (magnitude * 127) // available
        return min(scaled, 127)
    magnitude = -delta
    if magnitude <= deadzone:
        return 0
    magnitude -= deadzone
    available = 128 - deadzone
    scaled = (magnitude * 127) // available
    return -min(scaled, 127)


def parse_log(path: Path) -> dict:
    path = Path(path)
    data = {
        "path": str(path),
        "probe_version": None,
        "mode": None,
        "read_only": False,
        "mode_source": None,
        "mode_label_is_operator_selected": False,
        "calls": None,
        "no_cell_pad_init_end": False,
        "no_game_memory_writes": False,
        "no_hooks": False,
        "finished": False,
        "samples": {},
        "sample_errors": [],
        "port_line_errors": [],
    }

    for line_number, raw in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        stripped = raw.strip()
        header = HEADER_RE.match(stripped)
        if header:
            data["probe_version"] = int(header.group("version"))
            continue
        mode = MODE_RE.match(stripped)
        if mode:
            data["mode"] = mode.group(1).upper()
            continue
        if stripped == "READ_ONLY=1":
            data["read_only"] = True
            continue
        if stripped.startswith("MODE_SOURCE="):
            data["mode_source"] = stripped.split("=", 1)[1].strip().lower()
            continue
        if stripped == "MODE_LABEL_IS_OPERATOR_SELECTED=1":
            data["mode_label_is_operator_selected"] = True
            continue
        if stripped.startswith("CALLS="):
            data["calls"] = stripped.split("=", 1)[1].strip()
            continue
        if stripped == "NO_CELL_PAD_INIT_END=1":
            data["no_cell_pad_init_end"] = True
            continue
        if stripped == "NO_GAME_MEMORY_WRITES=1":
            data["no_game_memory_writes"] = True
            continue
        if stripped == "NO_HOOKS=1":
            data["no_hooks"] = True
            continue
        if stripped == "Pad input probe finished.":
            data["finished"] = True
            continue

        sample_match = SAMPLE_RE.match(stripped)
        if sample_match:
            index = int(sample_match.group("sample"))
            if index in data["samples"]:
                data["sample_errors"].append({
                    "line": line_number, "sample": index, "error": "duplicate sample header"
                })
                continue
            fields = _fields(sample_match.group("rest"))
            try:
                data["samples"][index] = {
                    "sample": index,
                    "result": sample_match.group("result").upper(),
                    "connected_mask": _number(fields, "connected_mask", 16),
                    "connected_count": _number(fields, "connected_count"),
                    "read_error_mask": _number(fields, "read_error_mask", 16),
                    "valid_mask": _number(fields, "valid_mask", 16),
                    "frame_connected_mask": _number(fields, "frame_connected_mask", 16),
                    "ports": {},
                }
                if data["samples"][index]["result"] == "ERROR":
                    data["sample_errors"].append({
                        "line": line_number, "sample": index, "error": "poll failed"
                    })
            except (ValueError, KeyError) as exc:
                data["sample_errors"].append({
                    "line": line_number, "sample": index, "error": str(exc)
                })
            continue

        port_match = PORT_RE.match(stripped)
        if port_match:
            sample_index = int(port_match.group("sample"))
            port = int(port_match.group("port"))
            if port not in (0, 1):
                data["port_line_errors"].append({
                    "line": line_number, "sample": sample_index, "port": port,
                    "error": "port outside 0/1 probe range",
                })
                continue
            if sample_index not in data["samples"]:
                data["port_line_errors"].append({
                    "line": line_number, "sample": sample_index, "port": port,
                    "error": "port line has no preceding sample header",
                })
                continue
            fields = _fields(port_match.group("rest"))
            try:
                port_data = {
                    "port": port,
                    "connected": _number(fields, "connected"),
                    "read_error": _number(fields, "read_error"),
                    "digital1": _number(fields, "digital1", 16),
                    "digital2": _number(fields, "digital2", 16),
                    "buttons": _number(fields, "buttons", 16),
                    "pressed": _number(fields, "pressed", 16),
                    "released": _number(fields, "released", 16),
                    "left_x": _number(fields, "left_x"),
                    "left_y": _number(fields, "left_y"),
                    "right_x": _number(fields, "right_x"),
                    "right_y": _number(fields, "right_y"),
                    "norm_left_x": _number(fields, "norm_left_x"),
                    "norm_left_y": _number(fields, "norm_left_y"),
                    "norm_right_x": _number(fields, "norm_right_x"),
                    "norm_right_y": _number(fields, "norm_right_y"),
                    "l2_pressure": _number(fields, "l2_pressure"),
                    "r2_pressure": _number(fields, "r2_pressure"),
                }
            except ValueError as exc:
                data["port_line_errors"].append({
                    "line": line_number, "sample": sample_index, "port": port,
                    "error": str(exc),
                })
                continue

            bounded_fields = {
                "connected": (0, 1),
                "read_error": (0, 1),
                "digital1": (0, 0xFFFF),
                "digital2": (0, 0xFFFF),
                "buttons": (0, 0xFFFF),
                "pressed": (0, 0xFFFF),
                "released": (0, 0xFFFF),
                "left_x": (0, 255),
                "left_y": (0, 255),
                "right_x": (0, 255),
                "right_y": (0, 255),
                "norm_left_x": (-127, 127),
                "norm_left_y": (-127, 127),
                "norm_right_x": (-127, 127),
                "norm_right_y": (-127, 127),
                "l2_pressure": (0, 255),
                "r2_pressure": (0, 255),
            }
            invalid = [
                key for key, (minimum, maximum) in bounded_fields.items()
                if not minimum <= port_data[key] <= maximum
            ]
            if invalid:
                data["port_line_errors"].append({
                    "line": line_number, "sample": sample_index, "port": port,
                    "error": "out-of-range fields: " + ", ".join(invalid),
                })
                continue
            if port_data["connected"] and port_data["read_error"]:
                data["port_line_errors"].append({
                    "line": line_number, "sample": sample_index, "port": port,
                    "error": "port cannot be connected and in read-error state simultaneously",
                })
                continue
            if port_data["pressed"] & port_data["released"]:
                data["port_line_errors"].append({
                    "line": line_number, "sample": sample_index, "port": port,
                    "error": "pressed and released masks overlap",
                })
                continue
            if port_data["connected"]:
                expected_buttons = _mapped_buttons(
                    port_data["digital1"], port_data["digital2"]
                )
                if port_data["buttons"] != expected_buttons:
                    data["port_line_errors"].append({
                        "line": line_number, "sample": sample_index, "port": port,
                        "error": (
                            "mapped buttons mask does not match raw digital1/digital2 "
                            f"(expected 0x{expected_buttons:04x})"
                        ),
                    })
                    continue
                raw_axes = {
                    "norm_left_x": port_data["left_x"],
                    "norm_left_y": port_data["left_y"],
                    "norm_right_x": port_data["right_x"],
                    "norm_right_y": port_data["right_y"],
                }
                axis_mismatches = [
                    key for key, raw_value in raw_axes.items()
                    if port_data[key] != _normalize_axis(raw_value)
                ]
                if axis_mismatches:
                    data["port_line_errors"].append({
                        "line": line_number, "sample": sample_index, "port": port,
                        "error": "normalized axes do not match raw stick values: " + ", ".join(axis_mismatches),
                    })
                    continue
            elif not port_data["read_error"] and any(port_data[key] for key in (
                "digital1", "digital2", "buttons", "left_x", "left_y",
                "right_x", "right_y", "norm_left_x", "norm_left_y",
                "norm_right_x", "norm_right_y", "l2_pressure", "r2_pressure",
            )):
                data["port_line_errors"].append({
                    "line": line_number, "sample": sample_index, "port": port,
                    "error": "disconnected or unreadable port contains raw input data",
                })
                continue

            if str(port) in data["samples"][sample_index]["ports"]:
                data["port_line_errors"].append({
                    "line": line_number, "sample": sample_index, "port": port,
                    "error": "duplicate port line",
                })
                continue
            data["samples"][sample_index]["ports"][str(port)] = port_data

    indices = sorted(data["samples"])
    expected_indices = list(range(EXPECTED_SAMPLE_COUNT))
    missing_samples = sorted(set(expected_indices) - set(indices))
    out_of_range = sorted(index for index in indices if index < 0 or index >= EXPECTED_SAMPLE_COUNT)
    missing_port_samples = sorted(
        index for index, sample in data["samples"].items()
        if set(sample["ports"]) != {"0", "1"}
    )
    port_read_errors = [
        {"sample": index, "port": port, "read_error": details["read_error"]}
        for index, sample in data["samples"].items()
        for port, details in sample["ports"].items()
        if details["read_error"] != 0
    ]
    consistency_errors = []
    previous_buttons = {"0": 0, "1": 0}
    for index in sorted(data["samples"]):
        sample = data["samples"][index]
        expected_mask = 0
        observed_error_mask = 0
        for port in (0, 1):
            details = sample["ports"].get(str(port))
            if details is None:
                continue
            if details["connected"] == 1 and details["read_error"] == 0:
                expected_mask |= 1 << port
            if details["read_error"] != 0:
                observed_error_mask |= 1 << port
        if (sample["connected_mask"] & 0x03) != expected_mask:
            consistency_errors.append({
                "sample": index, "error": "connected_mask does not match per-port lines"
            })
        if (sample["read_error_mask"] & 0x03) != observed_error_mask:
            consistency_errors.append({
                "sample": index, "error": "read_error_mask does not match per-port lines"
            })
        if sample["connected_count"] != expected_mask.bit_count():
            consistency_errors.append({
                "sample": index, "error": "connected_count does not match ports 0/1"
            })
        if sample["result"] == "OK":
            if sample["valid_mask"] != (0x03 & ~observed_error_mask):
                consistency_errors.append({
                    "sample": index, "error": "valid_mask does not match per-port read status"
                })
            if observed_error_mask == 0 and sample["frame_connected_mask"] != expected_mask:
                consistency_errors.append({
                    "sample": index, "error": "frame_connected_mask does not match current ports"
                })
            for port in (0, 1):
                details = sample["ports"].get(str(port))
                if details is None or details["read_error"]:
                    continue
                current = details["buttons"]
                expected_pressed = current & ~previous_buttons[str(port)]
                expected_released = previous_buttons[str(port)] & ~current
                if details["pressed"] != expected_pressed:
                    consistency_errors.append({
                        "sample": index, "port": port,
                        "error": "pressed mask does not match the previous successful sample",
                    })
                if details["released"] != expected_released:
                    consistency_errors.append({
                        "sample": index, "port": port,
                        "error": "released mask does not match the previous successful sample",
                    })
                previous_buttons[str(port)] = current
    data["sample_indices"] = indices
    data["missing_samples"] = missing_samples
    data["out_of_range_samples"] = out_of_range
    data["missing_port_samples"] = missing_port_samples
    data["port_read_errors"] = port_read_errors
    data["consistency_errors"] = consistency_errors

    simultaneous_samples = []
    port_activity = {"0": False, "1": False}
    axis_activity = {"0": False, "1": False}
    trigger_activity = {"0": False, "1": False}
    independent_samples = []
    independent_button_evidence = []
    independent_axis_samples = []
    independent_trigger_samples = []
    for index in sorted(data["samples"]):
        sample = data["samples"][index]
        port0 = sample["ports"].get("0")
        port1 = sample["ports"].get("1")
        if not port0 or not port1:
            continue
        both_connected = (
            sample["result"] == "OK"
            and (sample["connected_mask"] & 0x03) == 0x03
            and port0["connected"] == 1
            and port1["connected"] == 1
            and port0["read_error"] == 0
            and port1["read_error"] == 0
        )
        if both_connected:
            simultaneous_samples.append(index)
            if port0["buttons"] != 0:
                port_activity["0"] = True
            if port1["buttons"] != 0:
                port_activity["1"] = True
            exclusive0 = port0["buttons"] & ~port1["buttons"]
            exclusive1 = port1["buttons"] & ~port0["buttons"]
            if exclusive0 and exclusive1:
                independent_samples.append(index)
                independent_button_evidence.append({
                    "sample": index,
                    "port0_only_buttons": f"0x{exclusive0:04x}",
                    "port1_only_buttons": f"0x{exclusive1:04x}",
                })

            axes0 = (
                port0["norm_left_x"], port0["norm_left_y"],
                port0["norm_right_x"], port0["norm_right_y"],
            )
            axes1 = (
                port1["norm_left_x"], port1["norm_left_y"],
                port1["norm_right_x"], port1["norm_right_y"],
            )
            active0 = any(abs(value) > 0 for value in axes0)
            active1 = any(abs(value) > 0 for value in axes1)
            if active0:
                axis_activity["0"] = True
            if active1:
                axis_activity["1"] = True
            if active0 and active1 and axes0 != axes1:
                independent_axis_samples.append(index)

            triggers0 = (port0["l2_pressure"], port0["r2_pressure"])
            triggers1 = (port1["l2_pressure"], port1["r2_pressure"])
            trigger0 = any(value > 0 for value in triggers0)
            trigger1 = any(value > 0 for value in triggers1)
            if trigger0:
                trigger_activity["0"] = True
            if trigger1:
                trigger_activity["1"] = True
            if trigger0 and trigger1 and triggers0 != triggers1:
                independent_trigger_samples.append(index)

    data["simultaneous_samples"] = simultaneous_samples
    data["both_ports_simultaneously_connected"] = bool(simultaneous_samples)
    data["port_button_activity"] = port_activity
    data["port_axis_activity"] = axis_activity
    data["port_trigger_activity"] = trigger_activity
    data["independent_button_samples"] = independent_samples
    data["independent_button_evidence"] = independent_button_evidence
    data["independent_axis_samples"] = independent_axis_samples
    data["independent_trigger_samples"] = independent_trigger_samples
    data["independent_input_observed"] = bool(independent_samples)
    data["capture_complete"] = (
        data["probe_version"] == EXPECTED_VERSION
        and data["read_only"] is True
        and data["mode"] in ("CAMPAIGN", "MULTIPLAYER")
        and data["mode_source"] == "manual_mode_file"
        and data["mode_label_is_operator_selected"] is True
        and data["calls"] == "cellPadGetInfo2,cellPadGetData"
        and data["no_cell_pad_init_end"] is True
        and data["no_game_memory_writes"] is True
        and data["no_hooks"] is True
        and data["finished"] is True
        and not missing_samples
        and not out_of_range
        and not missing_port_samples
        and not data["sample_errors"]
        and not data["port_line_errors"]
        and not port_read_errors
        and not consistency_errors
        and all(sample["result"] == "OK" and sample["read_error_mask"] == 0 for sample in data["samples"].values())
        and len(indices) == EXPECTED_SAMPLE_COUNT
    )
    return data


def compare(campaign: dict, multiplayer: dict) -> dict:
    mode_mismatch = (
        campaign.get("mode") != "CAMPAIGN"
        or multiplayer.get("mode") != "MULTIPLAYER"
    )
    both_complete = (
        campaign.get("capture_complete") is True
        and multiplayer.get("capture_complete") is True
    )

    if mode_mismatch:
        status = "INCONCLUSIVE_MODE_LABEL_MISMATCH"
    elif not both_complete:
        status = "INCONCLUSIVE_INCOMPLETE_OR_UNVERIFIED_INPUT_CAPTURE"
    elif not campaign["both_ports_simultaneously_connected"] or not multiplayer["both_ports_simultaneously_connected"]:
        status = "BOTH_PORTS_NOT_SIMULTANEOUSLY_CONNECTED_IN_BOTH_MODES"
    elif not campaign["port_button_activity"]["0"] or not campaign["port_button_activity"]["1"]:
        status = "CAMPAIGN_BUTTON_ACTIVITY_MISSING_ON_ONE_OR_BOTH_PORTS"
    elif not multiplayer["port_button_activity"]["0"] or not multiplayer["port_button_activity"]["1"]:
        status = "MULTIPLAYER_BUTTON_ACTIVITY_MISSING_ON_ONE_OR_BOTH_PORTS"
    elif campaign["independent_input_observed"] and multiplayer["independent_input_observed"]:
        status = "INDEPENDENT_PORT_INPUT_OBSERVED_IN_BOTH_MODES"
    elif not campaign["independent_input_observed"] and not multiplayer["independent_input_observed"]:
        status = "INDEPENDENT_INPUT_NOT_PROVEN_SIMULTANEOUSLY"
    elif not campaign["independent_input_observed"]:
        status = "CAMPAIGN_INDEPENDENT_INPUT_NOT_PROVEN"
    else:
        status = "MULTIPLAYER_INDEPENDENT_INPUT_NOT_PROVEN"

    return {
        "status": status,
        "mode_labels_match_expected": not mode_mismatch,
        "capture_complete_in_both_modes": both_complete,
        "campaign": campaign,
        "multiplayer": multiplayer,
        "interpretation_limit": (
            "A passing result proves that cellPadGetData returned distinct button states for port 0 and port 1 "
            "during simultaneous connection samples in both selected modes. It does not prove that Ghosts "
            "created a second native Local Client, player entity, independent camera, HUD, or command stream. "
            "Mode labels come from the operator-selected mode file, not automatic game-mode detection."
        ),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign_log", type=Path)
    parser.add_argument("multiplayer_log", type=Path)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    for path in (args.campaign_log, args.multiplayer_log):
        if not path.is_file():
            parser.error(f"log file not found: {path}")
    result = compare(parse_log(args.campaign_log), parse_log(args.multiplayer_log))
    rendered = json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered + "\n", encoding="utf-8")
    if args.json:
        print(rendered)
    else:
        print(f"STATUS: {result['status']}")
        for label in ("campaign", "multiplayer"):
            data = result[label]
            print(
                f"{label}: complete={data['capture_complete']}, "
                f"simultaneous={data['both_ports_simultaneously_connected']}, "
                f"port0_activity={data['port_button_activity']['0']}, "
                f"port1_activity={data['port_button_activity']['1']}, "
                f"independent_samples={data['independent_button_samples']}"
            )
        print(result["interpretation_limit"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
