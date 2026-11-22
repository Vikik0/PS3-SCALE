#!/usr/bin/env python3
"""Audit user-supplied decompiled GSC folders for split-screen and co-op evidence."""
from __future__ import annotations
import argparse
import json
import re
from collections import Counter
from pathlib import Path

MATCHERS = {
    "issplitscreen": re.compile(r"\bissplitscreen\s*\(", re.I),
    "issplitscreenplayer": re.compile(r"\bissplitscreenplayer\s*\(", re.I),
    "issplitscreenplayerprimary": re.compile(r"\bissplitscreenplayerprimary\s*\(", re.I),
    "primary_player_ref": re.compile(r"\blevel\.player\b", re.I),
    "playerpiggyback": re.compile(r"\blevel\.playerpiggyback\b", re.I),
    "playengineeffects": re.compile(r"\blevel\.playengineeffects\b", re.I),
    "getplayerfromclientnum": re.compile(r"\bgetplayerfromclientnum\s*\(", re.I),
    "clientid": re.compile(r"\bclientid\b", re.I),
    "coop_mode": re.compile(r"\bcoop\b|is_coop\s*\(|is_controlling_uav\s*\(|coop_game", re.I),
    "setplayertocamera": re.compile(r"\bsetplayertocamera\s*\(", re.I),
    "hideonclient": re.compile(r"\bhideonclient\s*\(", re.I),
    "player_entity_array": re.compile(r'getentarray\s*\(\s*"player"\s*,\s*"classname"\s*\)', re.I),
    "player2_marker": re.compile(r'["\']player2["\']|tag_player2', re.I),
    "player1_marker": re.compile(r'["\']player1["\']', re.I),
}
FUNCTION_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s*\(")

class GscAuditError(ValueError):
    """Invalid GSC audit input."""

def scan_group(name: str, root: Path, max_hits: int = 1000) -> dict:
    if not root.is_dir():
        raise GscAuditError(f"group {name!r} is not a directory: {root}")
    files = sorted(root.rglob("*.gsc"))
    if not files:
        raise GscAuditError(f"group {name!r} contains no .gsc files: {root}")
    counts = Counter()
    file_counts = Counter()
    hits = []
    total_hits = 0
    for path in files:
        try:
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError as exc:
            raise GscAuditError(f"cannot read {path}: {exc}") from exc
        current_function = None
        seen_in_file = set()
        for line_number, line in enumerate(lines, 1):
            declaration = FUNCTION_RE.match(line)
            if declaration:
                current_function = declaration.group(1)
            for key, matcher in MATCHERS.items():
                matches = list(matcher.finditer(line))
                if not matches:
                    continue
                counts[key] += len(matches)
                seen_in_file.add(key)
                total_hits += len(matches)
                if len(hits) < max_hits:
                    hits.append({
                        "keyword": key,
                        "file": str(path.relative_to(root)).replace("\\", "/"),
                        "function": current_function,
                        "line": line_number,
                        "occurrences": len(matches),
                        "context": line.strip()[:200],
                    })
        for key in seen_in_file:
            file_counts[key] += 1
    return {
        "name": name,
        "root": str(root),
        "file_count": len(files),
        "line_match_count": dict(sorted(counts.items())),
        "files_with_match": dict(sorted(file_counts.items())),
        "total_matches": total_hits,
        "hits_recorded": len(hits),
        "hits_truncated": total_hits > len(hits),
        "hits": hits,
    }

def parse_group(value: str) -> tuple[str, Path]:
    if "=" not in value:
        raise argparse.ArgumentTypeError("group must be NAME=PATH")
    name, path = value.split("=", 1)
    if not name.strip() or not path.strip():
        raise argparse.ArgumentTypeError("group must contain a non-empty name and path")
    return name.strip(), Path(path.strip())

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--group", action="append", type=parse_group, required=True, help="repeat as NAME=DECOMPILED_FOLDER")
    parser.add_argument("--output", type=Path, default=Path("gsc_splitscreen_audit.json"))
    parser.add_argument("--max-hits", type=int, default=1000)
    args = parser.parse_args(argv)
    if args.max_hits < 1:
        parser.error("--max-hits must be positive")
    try:
        groups = [scan_group(name, path, args.max_hits) for name, path in args.group]
    except (OSError, GscAuditError) as exc:
        parser.error(str(exc))
    result = {
        "groups": groups,
        "interpretation_limit": (
            "This is a static GSC reference audit. It identifies script-side handling of split-screen "
            "and multiple player entities, but does not prove that Campaign creates a second native client."
        ),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps([
        {
            "group": group["name"],
            "files": group["file_count"],
            "matches": group["line_match_count"],
            "files_with_match": group["files_with_match"],
        }
        for group in groups
    ], indent=2))
    print(f"Report: {args.output}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
