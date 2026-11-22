#!/usr/bin/env python3
"""Compare read-only cellPadGetInfo2 logs from Campaign and Multiplayer."""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

OK_RE = re.compile(
    r"sample=(?P<sample>\d+)\s+result=OK\s+max_connect=(?P<max>\d+)"
    r"\s+now_connect=(?P<now>\d+)"
    r"(?:\s+system_info=0x(?P<system>[0-9a-fA-F]+))?"
    r"\s+port_mask=0x(?P<mask>[0-9a-fA-F]+)", re.I
)
ERR_RE = re.compile(r"sample=(?P<sample>\d+)\s+result=ERROR\s+rc=0x(?P<rc>[0-9a-fA-F]+)", re.I)
MODE_RE = re.compile(r"^MODE=(CAMPAIGN|MULTIPLAYER|UNSPECIFIED)\s*$", re.I)
MODE_SOURCE_RE = re.compile(r"^MODE_SOURCE=(manual_mode_file|auto_detected|unspecified)\s*$", re.I)
MODE_OPERATOR_RE = re.compile(r"^MODE_LABEL_IS_OPERATOR_SELECTED=(0|1)\s*$", re.I)
PORT_RE = re.compile(
    r"port(?P<port>\d+)\s+status=0x(?P<status>[0-9a-fA-F]+)"
    r"\s+capability=0x(?P<cap>[0-9a-fA-F]+)\s+device_type=(?P<type>\d+)", re.I
)

def parse_log(path: Path) -> dict:
    data = {"path": str(path), "mode": None, "mode_source": None, "mode_label_is_operator_selected": None, "samples": [], "errors": [], "ports": {}}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        mode_match = MODE_RE.match(line.strip())
        if mode_match:
            data["mode"] = mode_match.group(1).upper()
            continue
        source_match = MODE_SOURCE_RE.match(line.strip())
        if source_match:
            data["mode_source"] = source_match.group(1).lower()
            continue
        operator_match = MODE_OPERATOR_RE.match(line.strip())
        if operator_match:
            data["mode_label_is_operator_selected"] = operator_match.group(1) == "1"
            continue
        match = OK_RE.search(line)
        if match:
            data["samples"].append({
                "sample": int(match.group("sample")),
                "max_connect": int(match.group("max")),
                "now_connect": int(match.group("now")),
                "system_info": int(match.group("system"), 16) if match.group("system") else None,
                "port_mask": int(match.group("mask"), 16),
            })
            continue
        match = ERR_RE.search(line)
        if match:
            data["errors"].append({
                "sample": int(match.group("sample")),
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
    data["unique_masks"] = sorted(set(masks))
    data["port0_seen"] = any(mask & 0x01 for mask in masks)
    data["port1_seen"] = any(mask & 0x02 for mask in masks)
    data["both_controllers_simultaneously_seen"] = any((mask & 0x03) == 0x03 for mask in masks)
    data["simultaneous_samples"] = [
        sample["sample"] for sample in data["samples"] if (sample["port_mask"] & 0x03) == 0x03
    ]
    data["max_now_connect"] = max((sample["now_connect"] for sample in data["samples"]), default=None)
    return data

def compare(campaign: dict, multiplayer: dict) -> dict:
    mode_mismatch = (
        campaign["mode"] not in (None, "CAMPAIGN")
        or multiplayer["mode"] not in (None, "MULTIPLAYER")
    )
    if mode_mismatch:
        status = "INCONCLUSIVE_MODE_LABEL_MISMATCH"
    elif not campaign["samples"] or not multiplayer["samples"]:
        status = "INCONCLUSIVE_MISSING_VALID_SAMPLES"
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
        "campaign": campaign,
        "multiplayer": multiplayer,
        "interpretation_limit": (
            "MODE is an operator-selected label when mode_label_is_operator_selected is true; it is not auto-detected. "
            "A simultaneous 0x03 port mask proves only that the pad subsystem reported ports 0 and 1 "
            "connected in the same sample. It does not prove that the game created a second local client, "
            "player entity, camera, HUD, or independent command stream."
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
            f"simultaneous={data['both_controllers_simultaneously_seen']}"
        )
    print(result["interpretation_limit"])
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
