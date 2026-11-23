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
from pathlib import Path
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

    target_names = target_names_override or {
        "black_ice": ["player_saw_kill", "player_scuba", "player_fail_finale_instakill", "ascend_logic", "event_player_turns_key"],
        "carrier": ["monitorscavengerpickup", "dog_monitor_goal_ent", "get_different_favoriteenemy", "playerview_spawn", "playerview_playmissanim", "handlemeleefinishattacknotetracks", "knockback_hesh", "cut_the_rope", "victory_choppers_land"],
        "cornered": ["courtyard_rorke", "bar_enemies", "enemy_threat_logic_dog", "enemy_threat_logic_dog_wait", "enemy_threat_logic_dog_wakeup_dist", "enemy_corpse_alert_level", "gun_down_trigger_internal", "fall_player", "upload_virus_enter"],
    }.get(name, [])
    target_findings = []
    for target_name in target_names:
        for definition in definitions_by_name.get(target_name, []):
            refs = incoming.get(definition["id"], [])
            target_findings.append({
                "function": target_name,
                "definition": {key: definition[key] for key in ("id", "path", "start_line", "end_line")},
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


def audit_paths(missions: dict[str, Path], roots: dict[str, list[str]] | None = None) -> dict[str, Any]:
    if not missions:
        raise CallGraphError("provide at least one mission directory")
    reports = [audit_mission(name, path, (roots or {}).get(name)) for name, path in sorted(missions.items())]
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
        report = audit_paths(missions)
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


if __name__ == "__main__":
    raise SystemExit(main())
