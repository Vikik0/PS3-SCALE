#!/usr/bin/env python3
"""Function-level audit of extracted IW6 Campaign GSC mission directories."""
from __future__ import annotations
import argparse, json, re
from collections import Counter
from pathlib import Path

PATTERNS = {
    "level_player": re.compile(r"\blevel\.player\b"),
    "playerpiggyback": re.compile(r"\blevel\.playerpiggyback\b"),
    "playengineeffects": re.compile(r"\blevel\.playengineeffects\b"),
    "issplitscreen": re.compile(r"\bissplitscreen\s*\(", re.I),
    "issplitscreenplayer": re.compile(r"\bissplitscreenplayer\s*\(", re.I),
    "issplitscreenplayerprimary": re.compile(r"\bissplitscreenplayerprimary\s*\(", re.I),
    "coop": re.compile(r"\bcoop\b", re.I),
    "get_other_player": re.compile(r"\bget_other_player\s*\(", re.I),
    "getplayerfromclientnum": re.compile(r"\bgetplayerfromclientnum\s*\(", re.I),
    "clientid": re.compile(r"\bclientid\b", re.I),
    "callback_playerconnect": re.compile(r"\bcallback_playerconnect\s*\(", re.I),
    "callback_playerdisconnect": re.compile(r"\bcallback_playerdisconnect\s*\(", re.I),
    "getentarray_player": re.compile(r'getentarray\s*\(\s*"player"\s*,\s*"classname"\s*\)', re.I),
}
FUNCTION_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s*\((.*)\)\s*$")
PLAYER_METHOD_RE = re.compile(r"\blevel\.player\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(")


class AuditError(ValueError):
    pass


def strip_comments_and_strings(line):
    output = list(line)
    quote = None
    escaped = False
    index = 0
    while index < len(line):
        char = line[index]
        if quote:
            output[index] = " "
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
            index += 1
            continue
        if char in ('"', "'"):
            quote = char
            output[index] = " "
            index += 1
            continue
        if char == "/" and index + 1 < len(line) and line[index + 1] == "/":
            for cursor in range(index, len(line)):
                output[cursor] = " "
            break
        index += 1
    return "".join(output)


def function_ranges(lines):
    ranges = []
    index = 0
    while index < len(lines):
        clean = strip_comments_and_strings(lines[index]).strip()
        match = FUNCTION_RE.match(clean)
        if not match:
            index += 1
            continue
        brace_line = index if "{" in strip_comments_and_strings(lines[index]) else index + 1
        while brace_line < len(lines) and not strip_comments_and_strings(lines[brace_line]).strip():
            brace_line += 1
        if brace_line >= len(lines) or "{" not in strip_comments_and_strings(lines[brace_line]):
            index += 1
            continue
        depth = 0
        started = False
        end = brace_line
        for line_index in range(brace_line, len(lines)):
            for char in strip_comments_and_strings(lines[line_index]):
                if char == "{":
                    depth += 1
                    started = True
                elif char == "}":
                    depth -= 1
                    if started and depth == 0:
                        end = line_index
                        break
            if started and depth == 0:
                break
        if started and depth == 0:
            ranges.append((match.group(1), index + 1, end + 1))
            index = end + 1
        else:
            index += 1
    return ranges


def audit_file(path, root):
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    clean_lines = [strip_comments_and_strings(line) for line in lines]
    body = "\n".join(clean_lines)
    totals = {name: len(pattern.findall(body)) for name, pattern in PATTERNS.items()}
    ranges = function_ranges(lines)
    player_functions = []
    methods = Counter()
    for name, start, end in ranges:
        function_body = "\n".join(clean_lines[start - 1:end])
        player_refs = len(PATTERNS["level_player"].findall(function_body))
        if player_refs:
            function_methods = Counter(method.lower() for method in PLAYER_METHOD_RE.findall(function_body))
            methods.update(function_methods)
            player_functions.append({
                "function": name, "start_line": start, "end_line": end,
                "level_player_refs": player_refs, "methods": dict(function_methods),
            })
    return {
        "path": path.relative_to(root).as_posix(),
        "line_count": len(lines),
        "function_count": len(ranges),
        "functions_with_level_player": len(player_functions),
        **totals,
        "player_methods": dict(methods),
        "player_functions": player_functions,
    }


def audit_mission(name, root):
    root = Path(root).resolve()
    if not root.is_dir():
        raise AuditError(f"mission directory does not exist: {root}")
    files = sorted(root.rglob("*.gsc"), key=lambda item: item.as_posix().lower())
    if not files:
        raise AuditError(f"no GSC source files found: {root}")
    rows = [audit_file(path, root) for path in files]
    map_rows = [row for row in rows if row["path"].startswith("maps/")]

    def summarize(selected):
        return {
            "file_count": len(selected),
            "function_count": sum(row["function_count"] for row in selected),
            "functions_with_level_player": sum(row["functions_with_level_player"] for row in selected),
            "totals": {key: sum(row[key] for row in selected) for key in PATTERNS},
        }

    methods = Counter()
    functions = []
    for row in rows:
        methods.update(row["player_methods"])
        functions.extend({"file": row["path"], **item} for item in row["player_functions"])
    return {
        "mission": name,
        "all_scripts": summarize(rows),
        "map_scripts": summarize(map_rows),
        "top_player_methods": methods.most_common(40),
        "top_player_functions": sorted(
            functions, key=lambda row: (-row["level_player_refs"], row["file"], row["start_line"])
        )[:100],
        "files": rows,
    }


def audit_paths(missions):
    if not missions:
        raise AuditError("provide at least one mission directory")
    reports = [audit_mission(name, path) for name, path in sorted(missions.items())]
    return {
        "schema_version": 1,
        "status": "STATIC_GSC_AUDIT_COMPLETED",
        "mission_count": len(reports),
        "all_scripts": {
            "file_count": sum(row["all_scripts"]["file_count"] for row in reports),
            "function_count": sum(row["all_scripts"]["function_count"] for row in reports),
            "functions_with_level_player": sum(row["all_scripts"]["functions_with_level_player"] for row in reports),
            "level_player_refs": sum(row["all_scripts"]["totals"]["level_player"] for row in reports),
        },
        "map_scripts": {
            "file_count": sum(row["map_scripts"]["file_count"] for row in reports),
            "function_count": sum(row["map_scripts"]["function_count"] for row in reports),
            "functions_with_level_player": sum(row["map_scripts"]["functions_with_level_player"] for row in reports),
            "level_player_refs": sum(row["map_scripts"]["totals"]["level_player"] for row in reports),
        },
        "missions": reports,
        "interpretation": (
            "Static source counts only. Existing player-array loops and split-screen predicates "
            "are not proof that Campaign creates or runs a second native local client."
        ),
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("missions", nargs="+", help="NAME=PATH pairs")
    parser.add_argument("--output-json", required=True, type=Path)
    args = parser.parse_args(argv)
    missions = {}
    try:
        for item in args.missions:
            if "=" not in item:
                raise AuditError(f"expected NAME=PATH, got {item!r}")
            name, raw_path = item.split("=", 1)
            if not name or not raw_path or name in missions:
                raise AuditError(f"invalid or duplicate mission mapping: {item!r}")
            missions[name] = Path(raw_path)
        report = audit_paths(missions)
        args.output_json.parent.mkdir(parents=True, exist_ok=True)
        args.output_json.write_text(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(json.dumps({"status": report["status"], "all_scripts": report["all_scripts"], "map_scripts": report["map_scripts"]}, indent=2))
        return 0
    except (OSError, AuditError, ValueError) as exc:
        parser.exit(1, f"error: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
