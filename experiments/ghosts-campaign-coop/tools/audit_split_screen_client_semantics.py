#!/usr/bin/env python3
"""Trace client-scoped rendering and FOV selectors in the uploaded mission GSC sources."""
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

VISIBILITY_CALL = re.compile(
    r"\b(?P<receiver>[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*)\s+"
    r"(?P<api>showonclient|hideonclient)\s*\(\s*(?P<argument>[^)]*)\)",
    re.IGNORECASE,
)
FOV_BINDING = re.compile(
    r"\blevel\.playerpiggyback\s*\[\s*(?P<index>\d+)\s*\]\.sniper_degy\s*=\s*['\"](?P<dvar>cg_playerFovScale[01])['\"]",
    re.IGNORECASE,
)
SNIPER_DVAR_SET = re.compile(r"\bsetsaveddvar\s*\(\s*self\.sniper_degy\s*,", re.IGNORECASE)
FAVORITE_SECONDARY_COMPARE = re.compile(
    r"\bself\.favoriteenemy\s*!=\s*level\.playengineeffects\b", re.IGNORECASE
)
CLIENT_FONT_CALL = re.compile(r"\bcreateclientfontstring\s*\(", re.IGNORECASE)


class ClientSemanticsAuditError(ValueError):
    pass


def _client_role(argument: str) -> str:
    token = argument.strip().lower()
    if "level.playengineeffects" in token:
        return "SECONDARY_CLIENT_CANDIDATE"
    if "level.player" in token:
        return "PRIMARY_PLAYER_CANDIDATE"
    return "UNRESOLVED_CLIENT_ARGUMENT"


def scan_mission(mission: str, root: Path) -> dict[str, Any]:
    root = Path(root).expanduser().resolve()
    manifest = hash_gsc_tree(root)
    visibility, fov_bindings, dvar_setters, favorite_compares, font_calls = [], [], [], [], []
    for entry in manifest["files"]:
        path = root / entry["path"]
        raw_lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        clean = clean_source_lines(raw_lines)
        function_by_line: dict[int, str] = {}
        for name, start, end in function_ranges(clean):
            for line_number in range(start, end + 1):
                function_by_line[line_number] = name
        for line_number, line in enumerate(clean, 1):
            if not line.strip():
                continue
            function = function_by_line.get(line_number)
            raw = raw_lines[line_number - 1] if line_number <= len(raw_lines) else line
            for match in VISIBILITY_CALL.finditer(line):
                argument = match.group("argument").strip()
                visibility.append({
                    "mission": mission, "file": entry["path"], "line": line_number,
                    "function": function, "receiver": match.group("receiver"),
                    "api": match.group("api").lower(), "client_argument": argument,
                    "client_role": _client_role(argument),
                })
            fov = FOV_BINDING.search(raw)
            if fov and ("level.playerpiggyback" in line):
                fov_bindings.append({
                    "mission": mission, "file": entry["path"], "line": line_number,
                    "function": function, "array_index": int(fov.group("index")),
                    "dvar": fov.group("dvar"),
                })
            if SNIPER_DVAR_SET.search(line):
                dvar_setters.append({
                    "mission": mission, "file": entry["path"], "line": line_number,
                    "function": function,
                })
            if FAVORITE_SECONDARY_COMPARE.search(line):
                favorite_compares.append({
                    "mission": mission, "file": entry["path"], "line": line_number,
                    "function": function,
                })
            if CLIENT_FONT_CALL.search(line):
                font_calls.append({
                    "mission": mission, "file": entry["path"], "line": line_number,
                    "function": function,
                })
    after = hash_gsc_tree(root)
    if manifest["sha256"] != after["sha256"] or manifest["files"] != after["files"]:
        raise ClientSemanticsAuditError(f"{mission}: GSC source changed during client-semantics scan")
    return {
        "mission": mission,
        "source_manifest": manifest,
        "client_visibility_calls": visibility,
        "player_fov_bindings": fov_bindings,
        "sniper_dvar_setters": dvar_setters,
        "favoriteenemy_secondary_client_comparisons": favorite_compares,
        "client_fontstring_calls": font_calls,
        "summary": {
            "visibility_call_count": len(visibility),
            "hideonclient_count": sum(item["api"] == "hideonclient" for item in visibility),
            "showonclient_count": sum(item["api"] == "showonclient" for item in visibility),
            "secondary_client_selector_count": sum(item["client_role"] == "SECONDARY_CLIENT_CANDIDATE" for item in visibility),
            "primary_player_selector_count": sum(item["client_role"] == "PRIMARY_PLAYER_CANDIDATE" for item in visibility),
            "player_fov_binding_count": len(fov_bindings),
            "sniper_dvar_setter_count": len(dvar_setters),
            "favoriteenemy_secondary_compare_count": len(favorite_compares),
            "client_fontstring_call_count": len(font_calls),
        },
    }


def audit_paths(missions: dict[str, Path]) -> dict[str, Any]:
    if not missions:
        raise ClientSemanticsAuditError("provide at least one mission directory")
    if any(not isinstance(name, str) or not name for name in missions):
        raise ClientSemanticsAuditError("mission names must be non-empty strings")
    reports = [scan_mission(name, path) for name, path in sorted(missions.items())]
    return {
        "schema_version": 1,
        "status": "CLIENT_SCOPED_GSC_AUDIT_COMPLETED",
        "missions": reports,
        "validation_boundary": (
            "Client selectors and per-client DVAR names are static GSC clues. "
            "They do not prove native local-player initialization or runtime split-screen."
        ),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("missions", nargs="+", help="NAME=PATH pairs containing mission GSC trees")
    parser.add_argument("--output-json", required=True, type=Path)
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args(argv)
    missions = {}
    try:
        for item in args.missions:
            if "=" not in item:
                raise ClientSemanticsAuditError(f"expected NAME=PATH, got {item!r}")
            name, raw_path = item.split("=", 1)
            if not name or not raw_path or name in missions:
                raise ClientSemanticsAuditError(f"invalid or duplicate mission mapping: {item!r}")
            missions[name] = Path(raw_path)
        report = audit_paths(missions)
        protected = {
            str((Path(row["source_manifest"]["root"]) / entry["path"]).resolve()).casefold()
            for row in report["missions"] for entry in row["source_manifest"]["files"]
        }
        if str(args.output_json.expanduser().resolve()).casefold() in protected:
            raise ClientSemanticsAuditError("report output cannot overwrite a GSC source file")
        _atomic_write_json(args.output_json, report, args.overwrite)
        print(json.dumps({
            row["mission"]: row["summary"] for row in report["missions"]
        }, indent=2))
        return 0
    except (OSError, ClientSemanticsAuditError, ValueError) as exc:
        print(f"client-semantics audit failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
