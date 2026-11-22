#!/usr/bin/env python3
"""Find likely split-screen, local-player lifecycle and Campaign-flow code in IW6 GSC dumps.

This is a text-only triage tool for source files produced by GSC Tool. Matches
are leads for manual review, not proof that a function is called in Campaign.
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

SOURCE_SUFFIXES = {".gsc", ".csc", ".gscasm", ".txt"}
MAX_FINDINGS = 20000

PATTERNS: tuple[tuple[str, re.Pattern[str]], ...] = (
    ("split_screen", re.compile(
        r"isSplitScreen|isSplitscreenPlayer|isSplitscreenPlayerPrimary|level\s*\.\s*splitscreen|set_split_screen|split.?screen",
        re.IGNORECASE,
    )),
    ("player_lifecycle", re.compile(
        r"level\s*\.\s*players|player[_ ]?connect|player[_ ]?disconnect|clientid|local[_ ]?client|spawnplayer|spawn_player|waittill\s*\(\s*\"connected\"|waittill\s*\(\s*\"disconnect\"|getEntityNumber|removePlayerOnDisconnect",
        re.IGNORECASE,
    )),
    ("engine_player_api", re.compile(
        r"PlayerCmd_[A-Za-z0-9_]+|G_SpawnPlayer[A-Za-z0-9_]*|PlayerCMD_[A-Za-z0-9_]+",
        re.IGNORECASE,
    )),
    ("client_presentation", re.compile(
        r"SetClientOmnvar|SetClientDvars?|newClientHudElem|createFontString|cg_fovscale|viewport",
        re.IGNORECASE,
    )),
    ("solo_coop_branch", re.compile(
        r"isPlayingSolo|coop_gameShouldEnd|solo_gameShouldEnd|wait_to_be_revived|wait_for_self_revive|everyone_else_all_in_lastStand",
        re.IGNORECASE,
    )),
    ("down_revive", re.compile(
        r"last.?stand|reviv|bleed.?out|downed|reviver|dogtag",
        re.IGNORECASE,
    )),
    ("campaign_flow", re.compile(
        r"checkpoint|mission[_ ]?(start|end|complete|restart)|map_restart|restartmission|transition|scripted[_ ]?(scene|event)",
        re.IGNORECASE,
    )),
)


@dataclass(frozen=True)
class Finding:
    path: str
    line: int
    category: str
    terms: list[str]
    snippet: str


def _source_files(paths: list[Path]) -> list[Path]:
    files: set[Path] = set()
    for path in paths:
        if path.is_file():
            if path.suffix.lower() in SOURCE_SUFFIXES:
                files.add(path)
        elif path.is_dir():
            for candidate in path.rglob("*"):
                if candidate.is_file() and candidate.suffix.lower() in SOURCE_SUFFIXES:
                    files.add(candidate)
    return sorted(files, key=lambda item: str(item).lower())


def scan_file(path: Path, *, display_root: Path | None = None) -> list[Finding]:
    try:
        content = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []

    try:
        display_path = str(path.relative_to(display_root)) if display_root else str(path)
    except ValueError:
        display_path = str(path)

    findings: list[Finding] = []
    for line_number, line in enumerate(content.splitlines(), start=1):
        for category, pattern in PATTERNS:
            matches = sorted({match.group(0) for match in pattern.finditer(line)}, key=str.lower)
            if matches:
                snippet = line.strip()
                if len(snippet) > 240:
                    snippet = snippet[:237] + "..."
                findings.append(Finding(
                    path=display_path,
                    line=line_number,
                    category=category,
                    terms=matches,
                    snippet=snippet,
                ))
                if len(findings) >= MAX_FINDINGS:
                    return findings
    return findings


def audit_paths(paths: list[Path]) -> dict[str, Any]:
    files = _source_files(paths)
    root = paths[0] if len(paths) == 1 and paths[0].is_dir() else None
    findings: list[Finding] = []
    for path in files:
        findings.extend(scan_file(path, display_root=root))
        if len(findings) >= MAX_FINDINGS:
            findings = findings[:MAX_FINDINGS]
            break
    counts: dict[str, int] = {}
    for finding in findings:
        counts[finding.category] = counts.get(finding.category, 0) + 1
    return {
        "source_file_count": len(files),
        "finding_count": len(findings),
        "category_counts": dict(sorted(counts.items())),
        "findings": [asdict(finding) for finding in findings],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path, help="GSC/CSC source files or directories")
    parser.add_argument("--report-json", type=Path, help="write the audit as JSON")
    args = parser.parse_args(argv)

    report = audit_paths(args.paths)
    print(json.dumps({key: value for key, value in report.items() if key != "findings"}, indent=2))
    for finding in report["findings"]:
        print(f"{finding['path']}:{finding['line']} [{finding['category']}] {', '.join(finding['terms'])}")
        print(f"  {finding['snippet']}")
    if args.report_json:
        args.report_json.parent.mkdir(parents=True, exist_ok=True)
        args.report_json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
