#!/usr/bin/env python3
"""Function-level dependency audit for decompiled Ghosts IW6 Campaign GSC.

Read-only text analysis. Does not compile, modify, or rebuild game assets.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path

FUNCTION_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s*\((.*)\)\s*$")
PLAYER_RE = re.compile(r"\blevel\.player\b")
PIGGYBACK_RE = re.compile(r"\blevel\.playerpiggyback\b")
SPLIT_RE = re.compile(r"\b(?:issplitscreen|issplitscreenplayer|issplitscreenplayerprimary)\s*\(", re.I)
COOP_RE = re.compile(r"\bcoop\b", re.I)
METHOD_RE = re.compile(r"\blevel\.player\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(")

GROUPS = {
    "input": {"buttonpressed", "attackbuttonpressed", "adsbuttonpressed", "usebuttonpressed", "meleebuttonpressed", "jumpbuttonpressed", "reloadbuttonpressed"},
    "weapons": {"giveweapon", "takeweapon", "takeallweapons", "switchtoweapon", "switchtoweaponimmediate", "getcurrentweapon", "setweaponammoclip", "setweaponammostock", "getweaponammoclip", "setoffhandprimaryclass", "setoffhandsecondaryclass", "givemaxammo", "disableweaponswitch", "enableweaponswitch", "enableweapons", "disableweapons"},
    "movement_transform": {"setorigin", "setplayerangles", "setvelocity", "playerlinktoblend", "playerlinktodelta", "playerlinktoabsolute", "playerlinkto", "linkto", "unlink", "freezecontrols", "allowcrouch", "allowprone", "allowlean", "allowjump", "allowsprint", "setstance"},
    "camera_view": {"setviewmodel", "hideviewmodel", "showviewmodel", "getplayerangles", "geteye", "setclientdvar", "setviewkickscale", "setthirdperson"},
    "hud_interaction": {"setactionslot", "newpip", "createclienticon", "createclientfontstring", "sethintstring", "sethinttext", "iprintln", "iprintlnbold"},
    "health_lifecycle": {"isalive", "kill", "suicide", "sethealth", "gethealth", "damage", "endon", "waittill", "notify", "reviveplayer", "laststandrevive"},
}


def strip_comments_and_strings(line: str) -> str:
    out = list(line)
    quote = None
    escaped = False
    i = 0
    while i < len(line):
        ch = line[i]
        if quote:
            out[i] = " "
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                quote = None
            i += 1
            continue
        if ch in ('"', "'"):
            quote = ch
            out[i] = " "
            i += 1
            continue
        if ch == "/" and i + 1 < len(line) and line[i + 1] == "/":
            for j in range(i, len(line)):
                out[j] = " "
            break
        i += 1
    return "".join(out)


def function_ranges(lines: list[str]) -> list[tuple[str, int, int]]:
    ranges = []
    i = 0
    while i < len(lines):
        clean = strip_comments_and_strings(lines[i]).strip()
        match = FUNCTION_RE.match(clean)
        if not match:
            i += 1
            continue
        brace_line = i if "{" in strip_comments_and_strings(lines[i]) else i + 1
        while brace_line < len(lines) and not strip_comments_and_strings(lines[brace_line]).strip():
            brace_line += 1
        if brace_line >= len(lines) or "{" not in strip_comments_and_strings(lines[brace_line]):
            i += 1
            continue
        depth = 0
        started = False
        end = brace_line
        for j in range(brace_line, len(lines)):
            for ch in strip_comments_and_strings(lines[j]):
                if ch == "{":
                    depth += 1
                    started = True
                elif ch == "}":
                    depth -= 1
                    if started and depth == 0:
                        end = j
                        break
            if started and depth == 0:
                break
        if started and depth == 0:
            ranges.append((match.group(1), i + 1, end + 1))
            i = end + 1
        else:
            i += 1
    return ranges


def categories(methods: set[str]) -> list[str]:
    result = [name for name, members in GROUPS.items() if methods & members]
    return result or ["other_player_api"]


@dataclass
class FunctionFinding:
    file: str
    function: str
    start_line: int
    end_line: int
    level_player_refs: int
    playerpiggyback_refs: int
    split_screen_refs: int
    coop_refs: int
    player_methods: str
    categories: str


def audit_file(path: Path, root: Path):
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    rel = path.relative_to(root).as_posix()
    findings = []
    line_hits = []
    covered = set()
    for name, start, end in function_ranges(lines):
        body_lines = lines[start - 1:end]
        clean_lines = [strip_comments_and_strings(line) for line in body_lines]
        body = "\n".join(clean_lines)
        player_count = len(PLAYER_RE.findall(body))
        piggy_count = len(PIGGYBACK_RE.findall(body))
        split_count = len(SPLIT_RE.findall(body))
        coop_count = len(COOP_RE.findall(body))
        methods = sorted({m.lower() for m in METHOD_RE.findall(body)})
        cats = categories(set(methods)) if player_count else []
        if player_count or piggy_count or split_count or coop_count:
            findings.append(FunctionFinding(rel, name, start, end, player_count, piggy_count, split_count, coop_count, ";".join(methods), ";".join(cats)))
        for line_no, (raw, clean) in enumerate(zip(body_lines, clean_lines), start=start):
            lp = len(PLAYER_RE.findall(clean))
            pb = len(PIGGYBACK_RE.findall(clean))
            ss = len(SPLIT_RE.findall(clean))
            cp = len(COOP_RE.findall(clean))
            if not (lp or pb or ss or cp):
                continue
            line_methods = sorted({m.lower() for m in METHOD_RE.findall(clean)})
            line_hits.append({
                "file": rel, "function": name, "line": line_no,
                "level_player_refs": lp, "playerpiggyback_refs": pb,
                "split_screen_refs": ss, "coop_refs": cp,
                "methods": ";".join(line_methods),
                "categories": ";".join(categories(set(line_methods)) if lp else []),
            })
            covered.add(line_no)
    all_clean = "\n".join(strip_comments_and_strings(line) for line in lines)
    summary = {
        "file": rel, "line_count": len(lines), "function_count": len(function_ranges(lines)),
        "level_player_refs": len(PLAYER_RE.findall(all_clean)),
        "playerpiggyback_refs": len(PIGGYBACK_RE.findall(all_clean)),
        "split_screen_refs": len(SPLIT_RE.findall(all_clean)),
        "coop_refs": len(COOP_RE.findall(all_clean)),
        "unscoped_player_ref_lines": sum(1 for n, line in enumerate(lines, 1) if n not in covered and PLAYER_RE.search(strip_comments_and_strings(line))),
    }
    return findings, line_hits, summary


def audit_paths(inputs: list[Path], all_files: bool = False):
    files = []
    for item in inputs:
        if item.is_dir():
            files.extend(item.rglob("*.gsc"))
        elif item.is_file() and item.suffix.lower() == ".gsc":
            files.append(item)
    files = sorted(set(files))
    if not all_files:
        files = [p for p in files if p.name.lower().startswith("maps__clockwork")]
    if not files:
        raise ValueError("no matching decompiled GSC files found")
    root = Path(__import__("os").path.commonpath([str(p.parent) for p in files]))
    functions, line_hits, file_rows = [], [], []
    for path in files:
        f, l, s = audit_file(path, root)
        functions.extend(f)
        line_hits.extend(l)
        file_rows.append(s)
    category_counts = Counter()
    method_line_counts = Counter()
    for row in functions:
        if row.level_player_refs:
            for cat in row.categories.split(";"):
                if cat:
                    category_counts[cat] += row.level_player_refs
    for row in line_hits:
        for method in row["methods"].split(";"):
            if method:
                method_line_counts[method] += 1
    hotspots = sorted((row for row in functions if row.level_player_refs), key=lambda x: (-x.level_player_refs, x.file, x.start_line))
    return {
        "scope": "all supplied GSC" if all_files else "maps__clockwork*.gsc",
        "file_count": len(file_rows),
        "function_count": sum(row["function_count"] for row in file_rows),
        "functions_with_player_dependencies": sum(1 for row in functions if row.level_player_refs),
        "level_player_refs": sum(row["level_player_refs"] for row in file_rows),
        "playerpiggyback_refs": sum(row["playerpiggyback_refs"] for row in file_rows),
        "split_screen_refs": sum(row["split_screen_refs"] for row in file_rows),
        "coop_refs": sum(row["coop_refs"] for row in file_rows),
        "non_exclusive_player_ref_counts_by_category": dict(sorted(category_counts.items())),
        "player_method_line_counts": dict(method_line_counts.most_common()),
        "files": file_rows,
        "functions": [asdict(row) for row in functions],
        "hotspots": [asdict(row) for row in hotspots[:100]],
        "line_hits": line_hits,
    }


def write_csv(path: Path, rows):
    if not rows:
        path.write_text("", encoding="utf-8")
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="+", type=Path)
    parser.add_argument("--all-files", action="store_true")
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        report = audit_paths(args.inputs, args.all_files)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    out = args.output_dir
    out.mkdir(parents=True, exist_ok=True)
    (out / "campaign_player_dependency_audit.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    write_csv(out / "campaign_file_summary.csv", report["files"])
    write_csv(out / "campaign_function_hotspots.csv", report["functions"])
    write_csv(out / "campaign_player_reference_lines.csv", report["line_hits"])
    summary = {key: value for key, value in report.items() if key not in {"files", "functions", "hotspots", "line_hits"}}
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
