#!/usr/bin/env python3
"""Compare split-screen/client-related strings between Campaign and Multiplayer ELFs."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
try:
    from .scan_runtime_strings import DEFAULT_TERMS, scan
except ImportError:
    from scan_runtime_strings import DEFAULT_TERMS, scan

def compare(campaign_elf: Path, multiplayer_elf: Path, terms=DEFAULT_TERMS) -> dict:
    campaign = scan(campaign_elf, terms)
    multiplayer = scan(multiplayer_elf, terms)
    c_map = {item["string"]: item["locations"] for item in campaign["matches"]}
    m_map = {item["string"]: item["locations"] for item in multiplayer["matches"]}
    c_keys, m_keys = set(c_map), set(m_map)
    common = sorted(c_keys & m_keys, key=str.lower)
    campaign_only = sorted(c_keys - m_keys, key=str.lower)
    multiplayer_only = sorted(m_keys - c_keys, key=str.lower)
    return {
        "campaign_elf": str(campaign_elf),
        "multiplayer_elf": str(multiplayer_elf),
        "campaign_entry_point": campaign["entry_point"],
        "multiplayer_entry_point": multiplayer["entry_point"],
        "campaign_only": [{"string": key, "locations": c_map[key]} for key in campaign_only],
        "multiplayer_only": [{"string": key, "locations": m_map[key]} for key in multiplayer_only],
        "common": [
            {
                "string": key,
                "campaign_locations": c_map[key],
                "multiplayer_locations": m_map[key],
            }
            for key in common
        ],
        "counts": {
            "campaign_only": len(campaign_only),
            "multiplayer_only": len(multiplayer_only),
            "common": len(common),
        },
        "interpretation_limit": (
            "A string appearing in one executable but not the other is only a static navigation lead. "
            "It does not establish a call path, player-slot lifecycle, or safe hook."
        ),
    }

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign_elf", type=Path)
    parser.add_argument("multiplayer_elf", type=Path)
    parser.add_argument("--output", type=Path, default=Path("campaign_mp_runtime_string_diff.json"))
    args = parser.parse_args(argv)
    try:
        result = compare(args.campaign_elf, args.multiplayer_elf)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(result["counts"], indent=2))
    print(f"Report: {args.output}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
