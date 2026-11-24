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
    for index, sample in data["samples"].items():
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
        if sample["valid_mask"] != (0x03 & ~observed_error_mask):
            consistency_errors.append({
                "sample": index, "error": "valid_mask does not match per-port read status"
            })
        if sample["frame_connected_mask"] != expected_mask:
            consistency_errors.append({
                "sample": index, "error": "frame_connected_mask does not match current ports"
            })
    data["sample_indices"] = indices
    data["missing_samples"] = missing_samples
    data["out_of_range_samples"] = out_of_range
    data["missing_port_samples"] = missing_port_samples
    data["port_read_errors"] = port_read_errors
    data["consistency_errors"] = consistency_errors

    simultaneous_samples = []
    port_activity = {"0": False, "1": False}
    independent_samples = []
    for index, sample in data["samples"].items():
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
            if port0["buttons"] != 0 and port1["buttons"] != 0 and port0["buttons"] != port1["buttons"]:
                independent_samples.append(index)

    data["simultaneous_samples"] = simultaneous_samples
    data["both_ports_simultaneously_connected"] = bool(simultaneous_samples)
    data["port_button_activity"] = port_activity
    data["independent_button_samples"] = independent_samples
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
