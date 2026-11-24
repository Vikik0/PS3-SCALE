#!/usr/bin/env python3
"""Build a conservative, file-aware call graph for extracted IW6 Campaign GSC.

The graph is static evidence only. Function-pointer registrations are kept
separate from direct/thread calls, and unresolved or ambiguous symbols are
reported rather than guessed.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict, deque
from pathlib import Path, PurePosixPath
from typing import Any

FUNCTION_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*\((.*)\)\s*$")
CONTROL_WORDS = {
    "if", "for", "foreach", "while", "switch", "catch", "wait", "waittill",
    "waittillmatch", "endon", "return", "sizeof", "isdefined",
}


class CallGraphError(ValueError):
    """Raised when the GSC input cannot be analyzed safely."""


def clean_source_lines(lines: list[str]) -> list[str]:
    """Blank comments and string contents while preserving line/column positions."""
    cleaned: list[str] = []
    in_block_comment = False
    for line in lines:
        out = list(line)
        quote: str | None = None
        escaped = False
        i = 0
        while i < len(line):
            if in_block_comment:
                out[i] = " "
                if line.startswith("*/", i):
                    if i + 1 < len(line):
                        out[i + 1] = " "
                    in_block_comment = False
                    i += 2
                else:
                    i += 1
                continue
            if quote is not None:
                out[i] = " "
                if escaped:
                    escaped = False
                elif line[i] == "\\":
                    escaped = True
                elif line[i] == quote:
                    quote = None
                i += 1
                continue
            if line.startswith("//", i):
                for j in range(i, len(line)):
                    out[j] = " "
                break
            if line.startswith("/*", i):
                out[i] = " "
                if i + 1 < len(line):
                    out[i + 1] = " "
                in_block_comment = True
                i += 2
                continue
            if line[i] in ('"', "'"):
                quote = line[i]
                out[i] = " "
            i += 1
        cleaned.append("".join(out))
    return cleaned


def function_ranges(clean_lines: list[str]) -> list[tuple[str, int, int]]:
    """Return top-level function definitions as (name, start_line, end_line)."""
    found: list[tuple[str, int, int]] = []
    index = 0
    while index < len(clean_lines):
        match = FUNCTION_RE.match(clean_lines[index])
        if not match or match.group(1).lower() in CONTROL_WORDS:
            index += 1
            continue
        brace_line = index if "{" in clean_lines[index] else index + 1
        while brace_line < len(clean_lines) and not clean_lines[brace_line].strip():
            brace_line += 1
        if brace_line >= len(clean_lines) or "{" not in clean_lines[brace_line]:
            index += 1
            continue
        depth = 0
        end_line: int | None = None
        for line_index in range(brace_line, len(clean_lines)):
            depth += clean_lines[line_index].count("{")
            depth -= clean_lines[line_index].count("}")
            if depth == 0:
                end_line = line_index
                break
            if depth < 0:
                break
        if end_line is None:
            index += 1
            continue
        found.append((match.group(1), index + 1, end_line + 1))
        index = end_line + 1
    return found


def _definition_id(path: str, name: str, line: int) -> str:
    return f"{path}::{name}@{line}"


def _namespace_path(namespace: str | None) -> str | None:
    if not namespace:
        return None
    return namespace[:-2].replace("\\", "/") + ".gsc"


def _resolve_symbol(
    name: str,
    namespace: str | None,
    caller_path: str,
    definitions_by_name: dict[str, list[dict[str, Any]]],
) -> tuple[dict[str, Any] | None, str]:
    candidates = definitions_by_name.get(name, [])
    if not candidates:
        return None, "external_or_missing"

    if namespace:
        expected = _namespace_path(namespace)
        exact = [item for item in candidates if item["path"].casefold() == (expected or "").casefold()]
        if len(exact) == 1:
            return exact[0], "namespace_path_match"
        if len(candidates) == 1:
            return candidates[0], "unique_symbol_fallback_namespace_unmapped"
        return None, "ambiguous_namespace_symbol"

    local = [item for item in candidates if item["path"] == caller_path]
    if len(local) == 1:
        return local[0], "same_file_symbol"
    if len(local) > 1:
        return None, "ambiguous_same_file_symbol"
    if len(candidates) == 1:
        return candidates[0], "unique_symbol_match"
    return None, "ambiguous_unqualified_symbol"


def _build_reference_patterns(names: list[str]) -> tuple[re.Pattern[str], re.Pattern[str]]:
    if not names:
        raise CallGraphError("no function definitions found")
    alternatives = "|".join(re.escape(name) for name in sorted(names, key=len, reverse=True))
    namespace = r"(?P<namespace>(?:[A-Za-z_][A-Za-z0-9_]*\\)*[A-Za-z_][A-Za-z0-9_]*::)?"
    call_re = re.compile(
        r"(?<![A-Za-z0-9_])" + namespace + r"(?P<name>" + alternatives + r")(?![A-Za-z0-9_])\s*\("
    )
    pointer_re = re.compile(
        r"::(?P<name>" + alternatives + r")(?![A-Za-z0-9_])(?!\s*\()"
    )
    return call_re, pointer_re


def audit_mission(
    name: str,
    root: Path,
    roots: list[str] | None = None,
    target_names_override: list[str] | None = None,
    target_definitions_override: list[dict[str, str]] | None = None,
) -> dict[str, Any]:
    root = Path(root).expanduser().resolve()
    if not root.is_dir():
        raise CallGraphError(f"mission directory does not exist: {root}")
    files = sorted(root.rglob("*.gsc"), key=lambda item: item.as_posix().casefold())
    if not files:
        raise CallGraphError(f"no GSC source files found under {root}")

    sources: dict[Path, tuple[list[str], list[str], list[tuple[str, int, int]]]] = {}
    definitions: list[dict[str, Any]] = []
    for path in files:
        original = path.read_text(encoding="utf-8", errors="replace").splitlines()
        clean = clean_source_lines(original)
        rel = path.relative_to(root).as_posix()
        ranges = function_ranges(clean)
        sources[path] = (original, clean, ranges)
        for function_name, start, end in ranges:
            definitions.append({
                "id": _definition_id(rel, function_name, start),
                "name": function_name,
                "path": rel,
                "start_line": start,
                "end_line": end,
            })

    definitions_by_name: dict[str, list[dict[str, Any]]] = defaultdict(list)
    definitions_by_id = {item["id"]: item for item in definitions}
    for definition in definitions:
        definitions_by_name[definition["name"]].append(definition)

    call_re, pointer_re = _build_reference_patterns(list(definitions_by_name))
    references: list[dict[str, Any]] = []
    for path, (original, clean, ranges) in sources.items():
        rel = path.relative_to(root).as_posix()
        for caller_name, start, end in ranges:
            caller_id = _definition_id(rel, caller_name, start)
            for line_number in range(start, end + 1):
                if line_number == start:
                    continue
                line = clean[line_number - 1]
                occupied: list[tuple[int, int]] = []
                for match in call_re.finditer(line):
                    symbol = match.group("name")
                    namespace = match.group("namespace")
                    prefix = line[:match.start()].rstrip()
                    style = "thread_call" if re.search(r"\bthread\s*$", prefix) else (
                        "namespaced_call" if namespace else "direct_call"
                    )
                    target, resolution = _resolve_symbol(
                        symbol, namespace, rel, definitions_by_name
                    )
                    references.append({
                        "mission": name,
                        "caller_id": caller_id,
                        "caller": caller_name,
                        "caller_file": rel,
                        "line": line_number,
                        "symbol": symbol,
                        "namespace": namespace[:-2] if namespace else None,
                        "kind": style,
                        "target_id": target["id"] if target else None,
                        "target_file": target["path"] if target else None,
                        "resolution": resolution,
                        "snippet": original[line_number - 1].strip()[:240],
                    })
                    occupied.append(match.span())

                for match in pointer_re.finditer(line):
                    if any(match.start() >= start_pos and match.end() <= end_pos for start_pos, end_pos in occupied):
                        continue
                    symbol = match.group("name")
                    namespace_prefix = line[:match.start()]
                    ns_match = re.search(r"((?:[A-Za-z_][A-Za-z0-9_]*\\)*[A-Za-z_][A-Za-z0-9_]*)::$", namespace_prefix)
                    namespace = ns_match.group(1) if ns_match else None
                    target, resolution = _resolve_symbol(
                        symbol, namespace, rel, definitions_by_name
                    )
                    references.append({
                        "mission": name,
                        "caller_id": caller_id,
                        "caller": caller_name,
                        "caller_file": rel,
                        "line": line_number,
                        "symbol": symbol,
                        "namespace": namespace,
                        "kind": "function_pointer",
                        "target_id": target["id"] if target else None,
                        "target_file": target["path"] if target else None,
                        "resolution": resolution,
                        "snippet": original[line_number - 1].strip()[:240],
                    })

    deduped: list[dict[str, Any]] = []
    seen: set[tuple[Any, ...]] = set()
    for item in references:
        key = (item["caller_id"], item["line"], item["symbol"], item["kind"])
        if key not in seen:
            seen.add(key)
            deduped.append(item)
    references = deduped

    direct_edges: dict[str, list[dict[str, Any]]] = defaultdict(list)
    callback_edges: dict[str, list[dict[str, Any]]] = defaultdict(list)
    incoming: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for reference in references:
        target_id = reference["target_id"]
        if not target_id:
            continue
        incoming[target_id].append(reference)
        if reference["kind"] == "function_pointer":
            callback_edges[reference["caller_id"]].append(reference)
        else:
            direct_edges[reference["caller_id"]].append(reference)

    root_names = roots or ["magicreloadwhenreachenemy"]
    root_defs = [item for item in definitions if item["name"] in root_names]

    def walk(include_callbacks: bool) -> tuple[set[str], dict[str, tuple[str, dict[str, Any]]]]:
        visited: set[str] = set()
        parent: dict[str, tuple[str, dict[str, Any]]] = {}
        queue = deque(item["id"] for item in root_defs)
        visited.update(queue)
        while queue:
            current = queue.popleft()
            edges = list(direct_edges.get(current, []))
            if include_callbacks:
                edges.extend(callback_edges.get(current, []))
            for edge in edges:
                target_id = edge["target_id"]
                if target_id and target_id not in visited:
                    visited.add(target_id)
                    parent[target_id] = (current, edge)
                    queue.append(target_id)
        return visited, parent

    direct_reachable, _direct_parent = walk(False)
    callback_reachable, callback_parent = walk(True)

    def path_to(target_id: str) -> list[dict[str, Any]] | None:
        if target_id not in callback_reachable:
            return None
        path: list[dict[str, Any]] = []
        current = target_id
        while current in callback_parent:
            previous, edge = callback_parent[current]
            path.append({
                "from": definitions_by_id[previous]["name"],
                "from_file": definitions_by_id[previous]["path"],
                "to": definitions_by_id[current]["name"],
                "to_file": definitions_by_id[current]["path"],
                "kind": edge["kind"],
                "line": edge["line"],
                "snippet": edge["snippet"],
            })
            current = previous
        path.reverse()
        return path

    default_target_names = {
        "black_ice": ["player_saw_kill", "player_scuba", "player_fail_finale_instakill", "ascend_logic", "event_player_turns_key"],
        "carrier": ["monitorscavengerpickup", "dog_monitor_goal_ent", "get_different_favoriteenemy", "playerview_spawn", "playerview_playmissanim", "handlemeleefinishattacknotetracks", "knockback_hesh", "cut_the_rope", "victory_choppers_land"],
        "cornered": ["courtyard_rorke", "bar_enemies", "enemy_threat_logic_dog", "enemy_threat_logic_dog_wait", "enemy_threat_logic_dog_wakeup_dist", "enemy_corpse_alert_level", "gun_down_trigger_internal", "fall_player", "upload_virus_enter"],
    }.get(name, [])
    target_findings = []

    def add_target_finding(target_name: str, definition: dict[str, Any] | None, selection_status: str, selection_mode: str, requested_file: str | None = None) -> None:
        if definition is None:
            target_findings.append({
                "function": target_name, "requested_file": requested_file,
                "definition": None, "selection_mode": selection_mode,
                "selection_status": selection_status, "call_site_count": 0,
                "function_pointer_count": 0, "call_sites": [],
                "reachable_from_script_roots_with_callbacks": False,
                "reachable_from_script_roots_without_callbacks": False,
                "shortest_root_path": None,
            })
            return
        refs = incoming.get(definition["id"], [])
        target_findings.append({
            "function": target_name,
            "definition": {key: definition[key] for key in ("id", "path", "start_line", "end_line")},
            "requested_file": requested_file,
            "selection_mode": selection_mode,
            "selection_status": selection_status,
            "call_site_count": sum(item["kind"] != "function_pointer" for item in refs),
            "function_pointer_count": sum(item["kind"] == "function_pointer" for item in refs),
            "call_sites": [
                {key: item[key] for key in ("caller", "caller_file", "line", "kind", "resolution", "snippet")}
                for item in refs
            ],
            "reachable_from_script_roots_with_callbacks": definition["id"] in callback_reachable,
            "reachable_from_script_roots_without_callbacks": definition["id"] in direct_reachable,
            "shortest_root_path": path_to(definition["id"]),
        })

    if target_definitions_override is not None:
        for requested in target_definitions_override:
            target_name, requested_file = requested["function"], requested["file"]
            matches = [item for item in definitions if item["name"] == target_name and item["path"].casefold() == requested_file.casefold()]
            if len(matches) == 1:
                add_target_finding(target_name, matches[0], "FOUND_EXACT_FILE_FUNCTION", "exact_file_function", requested_file)
            elif not matches:
                add_target_finding(target_name, None, "MISSING_EXACT_FILE_FUNCTION", "exact_file_function", requested_file)
            else:
                add_target_finding(target_name, None, "AMBIGUOUS_DUPLICATE_IN_FILE", "exact_file_function", requested_file)
    else:
        target_names = target_names_override if target_names_override is not None else default_target_names
        for target_name in target_names:
            for definition in definitions_by_name.get(target_name, []):
                add_target_finding(target_name, definition, "FOUND_NAME_MATCH", "name_only")

    kind_counts: dict[str, int] = defaultdict(int)
    resolution_counts: dict[str, int] = defaultdict(int)
    for item in references:
        kind_counts[item["kind"]] += 1
        resolution_counts[item["resolution"]] += 1

    return {
        "mission": name,
        "status": "STATIC_CALLGRAPH_COMPLETED",
        "source_file_count": len(files),
        "function_definition_count": len(definitions),
        "unique_function_name_count": len(definitions_by_name),
        "root_function_names": root_names,
        "root_definitions": [
            {key: item[key] for key in ("id", "path", "start_line", "end_line")}
            for item in root_defs
        ],
        "reference_count": len(references),
        "reference_kinds": dict(sorted(kind_counts.items())),
        "resolution_counts": dict(sorted(resolution_counts.items())),
        "direct_reachable_definition_count": len(direct_reachable),
        "callback_inclusive_reachable_definition_count": len(callback_reachable),
        "target_selection_mode": "exact_file_function" if target_definitions_override is not None else "name_only",
        "target_findings": target_findings,
        "unresolved_or_ambiguous_references": [
            {key: item[key] for key in ("caller", "caller_file", "line", "symbol", "namespace", "kind", "resolution", "snippet")}
            for item in references if not item["target_id"]
        ],
        "interpretation": (
            "A call graph is limited to the supplied decompiled GSC files. A missing internal call site "
            "does not prove that an engine callback or dynamically selected function is never used. "
            "Function-pointer edges show registration/reference, not guaranteed execution. Namespaced "
            "symbols resolved by unique-name fallback remain lower-confidence than exact path matches."
        ),
    }


def audit_paths(
    missions: dict[str, Path],
    roots: dict[str, list[str]] | None = None,
    target_definitions: dict[str, list[dict[str, str]]] | None = None,
) -> dict[str, Any]:
    if not missions:
        raise CallGraphError("provide at least one mission directory")
    if target_definitions is not None:
        missing_missions = sorted(set(missions) - set(target_definitions))
        extra_missions = sorted(set(target_definitions) - set(missions))
        if missing_missions or extra_missions:
            raise CallGraphError(
                "target manifest mission set must exactly match input missions; "
                f"missing={missing_missions}, extra={extra_missions}"
            )
    reports = [
        audit_mission(
            name, path, (roots or {}).get(name),
            target_definitions_override=(target_definitions or {}).get(name)
            if target_definitions is not None else None,
        )
        for name, path in sorted(missions.items())
    ]
    return {
        "schema_version": 1,
        "status": "CAMPAIGN_GSC_CALLGRAPH_COMPLETED",
        "mission_count": len(reports),
        "missions": reports,
        "interpretation": (
            "Static call-site and callback-reference evidence only. This report does not create or prove "
            "a native second Campaign local client, controller routing, independent camera, or HUD."
        ),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("missions", nargs="+", help="NAME=PATH pairs containing decompiled .gsc files")
    parser.add_argument("--output-json", required=True, type=Path)
    parser.add_argument("--target-manifest", type=Path, help="Phase 60 JSON; select exact file/function targets")
    args = parser.parse_args(argv)
    try:
        missions: dict[str, Path] = {}
        for item in args.missions:
            if "=" not in item:
                raise CallGraphError(f"expected NAME=PATH, got {item!r}")
            name, path = item.split("=", 1)
            if not name or not path or name in missions:
                raise CallGraphError(f"invalid or duplicate mission mapping: {item!r}")
            missions[name] = Path(path)
        target_definitions = load_target_manifest(args.target_manifest) if args.target_manifest else None
        report = audit_paths(missions, target_definitions=target_definitions)
        args.output_json.parent.mkdir(parents=True, exist_ok=True)
        args.output_json.write_text(
            json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        print(json.dumps({
            "status": report["status"],
            "mission_count": report["mission_count"],
            "missions": [{
                "mission": row["mission"],
                "source_file_count": row["source_file_count"],
                "function_definition_count": row["function_definition_count"],
                "reference_count": row["reference_count"],
                "reference_kinds": row["reference_kinds"],
                "target_findings": [
                    {
                        "function": item["function"],
                        "call_site_count": item["call_site_count"],
                        "function_pointer_count": item["function_pointer_count"],
                        "reachable_from_script_roots_with_callbacks": item["reachable_from_script_roots_with_callbacks"],
                    }
                    for item in row["target_findings"]
                ],
            } for row in report["missions"]],
        }, indent=2))
        return 0
    except (OSError, CallGraphError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


def load_target_manifest(path: Path) -> dict[str, list[dict[str, str]]]:
    """Load the Phase 60 file/function manifest without name-only fallback."""
    try:
        payload = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise CallGraphError(f"unable to read target manifest {path}: {exc}") from exc
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        raise CallGraphError("target manifest must be a schema_version 1 JSON object")
    mission_rows = payload.get("missions")
    if not isinstance(mission_rows, list):
        raise CallGraphError("target manifest missions must be a list")
    result: dict[str, list[dict[str, str]]] = {}
    for mission in mission_rows:
        if not isinstance(mission, dict):
            raise CallGraphError("target manifest mission entries must be objects")
        name, entries = mission.get("name"), mission.get("functions_by_file", [])
        if not isinstance(name, str) or not name or name in result:
            raise CallGraphError(f"invalid or duplicate mission name in target manifest: {name!r}")
        if not isinstance(entries, list):
            raise CallGraphError(f"{name}: functions_by_file must be a list")
        targets, seen = [], set()
        for entry in entries:
            if not isinstance(entry, dict):
                raise CallGraphError(f"{name}: target entries must be objects")
            file_name, function = entry.get("file"), entry.get("function")
            if (
                not isinstance(file_name, str) or not file_name
                or "\\" in file_name or ":" in file_name or "\x00" in file_name
            ):
                raise CallGraphError(f"{name}: target file must be a relative POSIX path")
            raw_parts = file_name.split("/")
            relative = PurePosixPath(file_name)
            if relative.is_absolute() or any(part in ("", ".", "..") for part in raw_parts):
                raise CallGraphError(f"{name}: unsafe target path: {file_name!r}")
            if not isinstance(function, str) or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", function):
                raise CallGraphError(f"{name}: invalid target function name: {function!r}")
            key = (relative.as_posix().casefold(), function)
            if key in seen:
                raise CallGraphError(f"{name}: duplicate target {file_name}::{function}")
            seen.add(key)
            targets.append({"file": relative.as_posix(), "function": function})
        result[name] = targets
    if not result:
        raise CallGraphError("target manifest contains no missions")
    return result


if __name__ == "__main__":
    raise SystemExit(main())
