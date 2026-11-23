#!/usr/bin/env python3
"""Compare read-only cellPadGetInfo2 logs from Campaign and Multiplayer."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

EXPECTED_PROBE_VERSION = 4
EXPECTED_SAMPLE_COUNT = 30
CHECKPOINT_INDICES = tuple(range(0, EXPECTED_SAMPLE_COUNT, 5))

HEADER_RE = re.compile(r"^Ghosts Campaign Co-op Pad Probe v(?P<version>\d+)\s*$", re.I)
OK_RE = re.compile(
    r"sample=(?P<sample>\d+)\s+result=OK\s+max_connect=(?P<max>\d+)"
    r"\s+now_connect=(?P<now>\d+)"
    r"(?:\s+system_info=0x(?P<system>[0-9a-fA-F]+))?"
    r"\s+port_mask=0x(?P<mask>[0-9a-fA-F]+)", re.I
)
ERR_RE = re.compile(
    r"sample=(?P<sample>\d+)\s+result=ERROR\s+rc=0x(?P<rc>[0-9a-fA-F]+)", re.I
)
MODE_RE = re.compile(r"^MODE=(CAMPAIGN|MULTIPLAYER|UNSPECIFIED)\s*$", re.I)
MODE_SOURCE_RE = re.compile(
    r"^MODE_SOURCE=(manual_mode_file|auto_detected|unspecified)\s*$", re.I
)
MODE_OPERATOR_RE = re.compile(r"^MODE_LABEL_IS_OPERATOR_SELECTED=(0|1)\s*$", re.I)
PORT_RE = re.compile(
    r"port(?P<port>\d+)\s+status=0x(?P<status>[0-9a-fA-F]+)"
    r"\s+capability=0x(?P<cap>[0-9a-fA-F]+)\s+device_type=(?P<type>\d+)", re.I
)


def parse_log(path: Path) -> dict:
    data = {
        "path": str(path),
        "probe_version": None,
        "read_only": None,
        "finished": False,
        "mode": None,
        "mode_source": None,
        "mode_label_is_operator_selected": None,
        "samples": [],
        "errors": [],
        "ports": {},
    }
    sample_events: list[int] = []

    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        stripped = line.strip()
        header_match = HEADER_RE.match(stripped)
        if header_match:
            data["probe_version"] = int(header_match.group("version"))
            continue
        if stripped == "READ_ONLY=1":
            data["read_only"] = True
            continue
        if stripped == "Pad probe finished.":
            data["finished"] = True
            continue

        mode_match = MODE_RE.match(stripped)
        if mode_match:
            data["mode"] = mode_match.group(1).upper()
            continue
        source_match = MODE_SOURCE_RE.match(stripped)
        if source_match:
            data["mode_source"] = source_match.group(1).lower()
            continue
        operator_match = MODE_OPERATOR_RE.match(stripped)
        if operator_match:
            data["mode_label_is_operator_selected"] = operator_match.group(1) == "1"
            continue

        match = OK_RE.search(line)
        if match:
            sample_index = int(match.group("sample"))
            sample_events.append(sample_index)
            data["samples"].append({
                "sample": sample_index,
                "max_connect": int(match.group("max")),
                "now_connect": int(match.group("now")),
                "system_info": int(match.group("system"), 16) if match.group("system") else None,
                "port_mask": int(match.group("mask"), 16),
            })
            continue

        match = ERR_RE.search(line)
        if match:
            sample_index = int(match.group("sample"))
            sample_events.append(sample_index)
            data["errors"].append({
                "sample": sample_index,
                "rc": int(match.group("rc"), 16),
            })
            continue

        match = PORT_RE.search(line)
        if match:
            port = int(match.group("port"))
            data["ports"][str(port)] = {
                "status": int(match.group("status"), 16),
                "capability": int(match.group("cap"), 16),
                "device_type": int(match.group("type")),
            }

    masks = [sample["port_mask"] for sample in data["samples"]]
    sample_counts: dict[int, int] = {}
    for sample_index in sample_events:
        sample_counts[sample_index] = sample_counts.get(sample_index, 0) + 1

    valid_sample_indices = {sample["sample"] for sample in data["samples"]}
    missing_checkpoints = [
        index for index in CHECKPOINT_INDICES if index not in valid_sample_indices
    ]
    duplicate_indices = sorted(
        index for index, count in sample_counts.items() if count > 1
    )
    out_of_range_indices = sorted(
        index for index in sample_counts if index < 0 or index >= EXPECTED_SAMPLE_COUNT
    )

    data["unique_masks"] = sorted(set(masks))
    data["port0_seen"] = any(mask & 0x01 for mask in masks)
    data["port1_seen"] = any(mask & 0x02 for mask in masks)
    data["both_controllers_simultaneously_seen"] = any(
        (mask & 0x03) == 0x03 for mask in masks
    )
    data["simultaneous_samples"] = [
        sample["sample"] for sample in data["samples"]
        if (sample["port_mask"] & 0x03) == 0x03
    ]
    data["max_now_connect"] = max(
        (sample["now_connect"] for sample in data["samples"]), default=None
    )
    data["sample_event_indices"] = sample_events
    data["missing_valid_checkpoints"] = missing_checkpoints
    data["duplicate_sample_indices"] = duplicate_indices
    data["out_of_range_sample_indices"] = out_of_range_indices
    data["capture_complete"] = (
        data["probe_version"] == EXPECTED_PROBE_VERSION
        and data["read_only"] is True
        and data["finished"] is True
        and not missing_checkpoints
        and not duplicate_indices
        and not out_of_range_indices
    )
    return data


def _mode_mismatch(campaign: dict, multiplayer: dict) -> bool:
    return (
        campaign.get("mode") not in (None, "CAMPAIGN")
        or multiplayer.get("mode") not in (None, "MULTIPLAYER")
    )


def _provenance_valid(capture: dict, expected_mode: str) -> bool:
    return (
        capture.get("probe_version") == EXPECTED_PROBE_VERSION
        and capture.get("read_only") is True
        and capture.get("mode") == expected_mode
        and capture.get("mode_source") == "manual_mode_file"
        and capture.get("mode_label_is_operator_selected") is True
    )


def compare(campaign: dict, multiplayer: dict) -> dict:
    mode_mismatch = _mode_mismatch(campaign, multiplayer)
    capture_complete = (
        campaign.get("capture_complete") is True
        and multiplayer.get("capture_complete") is True
    )
    provenance_valid = (
        _provenance_valid(campaign, "CAMPAIGN")
        and _provenance_valid(multiplayer, "MULTIPLAYER")
    )

    if mode_mismatch:
        status = "INCONCLUSIVE_MODE_LABEL_MISMATCH"
    elif not campaign["samples"] or not multiplayer["samples"]:
        status = "INCONCLUSIVE_MISSING_VALID_SAMPLES"
    elif not capture_complete:
        status = "INCONCLUSIVE_INCOMPLETE_CAPTURE"
    elif not provenance_valid:
        status = "INCONCLUSIVE_UNVERIFIED_CAPTURE_METADATA"
    elif campaign["both_controllers_simultaneously_seen"] and multiplayer["both_controllers_simultaneously_seen"]:
        status = "BOTH_CONTROLLERS_SIMULTANEOUSLY_VISIBLE_IN_BOTH"
    elif not campaign["both_controllers_simultaneously_seen"] and multiplayer["both_controllers_simultaneously_seen"]:
        status = "CAMPAIGN_PAD_ENUMERATION_DIFFERS"
    elif campaign["both_controllers_simultaneously_seen"] and not multiplayer["both_controllers_simultaneously_seen"]:
        status = "MULTIPLAYER_PAD_ENUMERATION_UNEXPECTEDLY_MISSING"
    elif campaign["port1_seen"] or multiplayer["port1_seen"]:
        status = "PORT1_SEEN_BUT_NOT_SIMULTANEOUS_WITH_PORT0"
    else:
        status = "SECOND_CONTROLLER_NOT_VISIBLE"

    return {
        "status": status,
        "mode_labels_match": not mode_mismatch,
        "mode_label_values_match_expected": not mode_mismatch,
        "mode_labels_are_operator_selected": (
            campaign.get("mode_label_is_operator_selected") is True
            and multiplayer.get("mode_label_is_operator_selected") is True
        ),
        "capture_complete_in_both_modes": capture_complete,
        "capture_provenance_valid_in_both_modes": provenance_valid,
        "mode_label_provenance": {
            "campaign": {
                "source": campaign.get("mode_source"),
                "operator_selected": campaign.get("mode_label_is_operator_selected"),
            },
            "multiplayer": {
                "source": multiplayer.get("mode_source"),
                "operator_selected": multiplayer.get("mode_label_is_operator_selected"),
            },
        },
        "capture_integrity": {
            "campaign": {
                "probe_version": campaign.get("probe_version"),
                "read_only": campaign.get("read_only"),
                "finished": campaign.get("finished"),
                "missing_valid_checkpoints": campaign.get("missing_valid_checkpoints", []),
                "duplicate_sample_indices": campaign.get("duplicate_sample_indices", []),
                "out_of_range_sample_indices": campaign.get("out_of_range_sample_indices", []),
                "errors": len(campaign.get("errors", [])),
            },
            "multiplayer": {
                "probe_version": multiplayer.get("probe_version"),
                "read_only": multiplayer.get("read_only"),
                "finished": multiplayer.get("finished"),
                "missing_valid_checkpoints": multiplayer.get("missing_valid_checkpoints", []),
                "duplicate_sample_indices": multiplayer.get("duplicate_sample_indices", []),
                "out_of_range_sample_indices": multiplayer.get("out_of_range_sample_indices", []),
                "errors": len(multiplayer.get("errors", [])),
            },
        },
        "campaign": campaign,
        "multiplayer": multiplayer,
        "interpretation_limit": (
            "A complete capture requires the v4 read-only header, the completion marker, "
            "and successful samples at indices 0, 5, 10, 15, 20, and 25 with no duplicate "
            "or out-of-range sample indices. MODE is an operator-selected label from the "
            "manual mode file; it is not auto-detected. A simultaneous 0x03 port mask proves "
            "only that the pad subsystem reported ports 0 and 1 connected in the same sample. "
            "It does not prove that the game created a second local client, player entity, "
            "camera, HUD, or independent command stream."
        ),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign_log", type=Path)
    parser.add_argument("multiplayer_log", type=Path)
    parser.add_argument("--json", action="store_true", help="print JSON instead of a short report")
    parser.add_argument("--output", type=Path, help="optional JSON report path")
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
        return 0
    print(f"STATUS: {result['status']}")
    for label in ("campaign", "multiplayer"):
        data = result[label]
        mode_label = data["mode"] or "unlabelled"
        if data.get("mode_label_is_operator_selected") is True:
            mode_label += " (operator-selected)"
        print(
            f"{label}: mode={mode_label}, valid_samples={len(data['samples'])}, "
            f"errors={len(data['errors'])}, masks={['0x%02x' % mask for mask in data['unique_masks']]}, "
            f"port0={data['port0_seen']}, port1={data['port1_seen']}, "
            f"simultaneous={data['both_controllers_simultaneously_seen']}, "
            f"complete={data['capture_complete']}"
        )
    print(result["interpretation_limit"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
