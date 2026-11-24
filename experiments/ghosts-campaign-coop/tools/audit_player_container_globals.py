#!/usr/bin/env python3
"""Find script-side initialization and mutation of Campaign player-container globals."""
from __future__ import annotations
import argparse, json, re, sys
from pathlib import Path
from typing import Any

try:
    from .gsc_callgraph_audit import clean_source_lines, function_ranges
    from .run_player_container_analysis import hash_gsc_tree, _atomic_write_json
except ImportError:
    from gsc_callgraph_audit import clean_source_lines, function_ranges
    from run_player_container_analysis import hash_gsc_tree, _atomic_write_json

PLAYER_GLOBAL = "level.playerpiggyback"
SECONDARY_CLIENT_GLOBAL = "level.playengineeffects"
GLOBAL_ASSIGNMENT = re.compile(r"\blevel\.playerpiggyback\s*(?:=(?!=)|\+=|-=)")
ARRAY_SLOT_ASSIGNMENT = re.compile(r"\blevel\.playerpiggyback\s*\[[^\]]+\]\s*(?:=(?!=)|\+=|-=)")
ENTITY_PROPERTY_WRITE = re.compile(r"\blevel\.playerpiggyback\s*\[[^\]]+\]\s*\.\s*[A-Za-z_]\w*\s*(?:=(?!=)|\+=|-=)")
ARRAY_MUTATOR = re.compile(r"\blevel\.playerpiggyback\s*\.\s*(?:push|append|remove|removeat|clear|splice|insert|add)\s*\(", re.I)
SECONDARY_GLOBAL_ASSIGNMENT = re.compile(r"\blevel\.playengineeffects\s*(?:=(?!=)|\+=|-=)")


class PlayerContainerGlobalAuditError(ValueError):
    pass


def _classify(line: str, variable: str) -> str:
    if variable == PLAYER_GLOBAL:
        if GLOBAL_ASSIGNMENT.search(line):
            return "GLOBAL_ASSIGNMENT"
        if ARRAY_SLOT_ASSIGNMENT.search(line):
            return "ARRAY_SLOT_ASSIGNMENT"
        if ENTITY_PROPERTY_WRITE.search(line):
            return "PLAYER_ENTITY_PROPERTY_WRITE"
        if ARRAY_MUTATOR.search(line):
            return "ARRAY_MUTATION"
        if re.search(r"\bforeach\s*\([^)]*\bin\s+level\.playerpiggyback\b", line):
            return "ITERATION_READ"
        if re.search(r"\blevel\.playerpiggyback\s*\[[^\]]+\]", line):
            return "INDEXED_READ"
        if re.search(r"\blevel\.playerpiggyback\s*\.\s*size\b", line):
            return "SIZE_READ"
        return "REFERENCE_READ"
    return "GLOBAL_ASSIGNMENT" if SECONDARY_GLOBAL_ASSIGNMENT.search(line) else "REFERENCE_READ"


def scan_file(path: Path, root: Path, mission: str) -> list[dict[str, Any]]:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    clean = clean_source_lines(lines)
    function_by_line: dict[int, str] = {}
    for name, start, end in function_ranges(clean):
        for line_number in range(start, end + 1):
            function_by_line[line_number] = name
    relative = path.relative_to(root).as_posix()
    findings = []
    for line_number, line in enumerate(clean, 1):
        for variable in (PLAYER_GLOBAL, SECONDARY_CLIENT_GLOBAL):
            if variable in line:
                findings.append({
                    "mission": mission, "file": relative, "line": line_number,
                    "function": function_by_line.get(line_number),
                    "variable": variable, "access": _classify(line, variable),
                })
    return findings


def audit_mission(mission: str, root: Path) -> dict[str, Any]:
    root = Path(root).expanduser().resolve()
    manifest = hash_gsc_tree(root)
    findings = []
    for entry in manifest["files"]:
        findings.extend(scan_file(root / entry["path"], root, mission))
    after = hash_gsc_tree(root)
    if manifest["sha256"] != after["sha256"] or manifest["files"] != after["files"]:
        raise PlayerContainerGlobalAuditError(f"{mission}: GSC source set changed during scan")

    def counts(variable: str) -> dict[str, int]:
        rows = [item for item in findings if item["variable"] == variable]
        result: dict[str, int] = {"reference_count": len(rows)}
        for row in rows:
            result[row["access"]] = result.get(row["access"], 0) + 1
        return dict(sorted(result.items()))

    player = counts(PLAYER_GLOBAL)
    secondary = counts(SECONDARY_CLIENT_GLOBAL)
    player_writes = sum(player.get(key, 0) for key in (
        "GLOBAL_ASSIGNMENT", "ARRAY_SLOT_ASSIGNMENT", "ARRAY_MUTATION"
    ))
    secondary_writes = secondary.get("GLOBAL_ASSIGNMENT", 0)
    return {
        "mission": mission,
        "source_manifest": manifest,
        "playerpiggyback": {
            **player,
            "initializer_or_array_mutation_count": player_writes,
            "initializer_status": "SCRIPT_INITIALIZER_FOUND" if player_writes else "NO_SCRIPT_INITIALIZER_IN_SCANNED_GSC",
        },
        "playengineeffects": {
            **secondary,
            "initializer_count": secondary_writes,
            "initializer_status": "SCRIPT_INITIALIZER_FOUND" if secondary_writes else "NO_SCRIPT_INITIALIZER_IN_SCANNED_GSC",
        },
        "occurrences": findings,
    }


def audit_paths(missions: dict[str, Path]) -> dict[str, Any]:
    if not missions:
        raise PlayerContainerGlobalAuditError("provide at least one mission directory")
    if any(not isinstance(name, str) or not name for name in missions):
        raise PlayerContainerGlobalAuditError("mission names must be non-empty strings")
    return {
        "schema_version": 1,
        "status": "PLAYER_CONTAINER_GLOBAL_REFERENCE_AUDIT_COMPLETED",
        "missions": [audit_mission(name, path) for name, path in sorted(missions.items())],
        "validation_boundary": (
            "This scanner identifies assignments and references in the supplied GSC source set only. "
            "A missing script assignment may be initialized by shared scripts or native engine code."
        ),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("missions", nargs="+", help="NAME=PATH pairs containing mission source trees")
    parser.add_argument("--output-json", required=True, type=Path)
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args(argv)
    missions = {}
    try:
        for item in args.missions:
            if "=" not in item:
                raise PlayerContainerGlobalAuditError(f"expected NAME=PATH, got {item!r}")
            name, raw_path = item.split("=", 1)
            if not name or not raw_path or name in missions:
                raise PlayerContainerGlobalAuditError(f"invalid or duplicate mission mapping: {item!r}")
            missions[name] = Path(raw_path)
        report = audit_paths(missions)
        protected = {
            str((Path(row["source_manifest"]["root"]) / entry["path"]).resolve()).casefold()
            for row in report["missions"] for entry in row["source_manifest"]["files"]
        }
        if str(args.output_json.expanduser().resolve()).casefold() in protected:
            raise PlayerContainerGlobalAuditError("report output cannot overwrite a GSC source file")
        _atomic_write_json(args.output_json, report, args.overwrite)
        print(json.dumps({"status": report["status"], "missions": {
            row["mission"]: {"playerpiggyback": row["playerpiggyback"], "playengineeffects": row["playengineeffects"]}
            for row in report["missions"]
        }}, indent=2))
        return 0
    except (OSError, PlayerContainerGlobalAuditError, ValueError) as exc:
        print(f"player-container global audit failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
