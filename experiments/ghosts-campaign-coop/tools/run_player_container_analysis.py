#!/usr/bin/env python3
"""Run the player-container ownership and exact call-graph audits as one stable read-only job."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import tempfile
from pathlib import Path
from typing import Any

try:
    from .gsc_player_state_ownership_audit import audit_paths as audit_ownership
    from .gsc_callgraph_audit import audit_paths as audit_callgraph
except ImportError:
    from gsc_player_state_ownership_audit import audit_paths as audit_ownership
    from gsc_callgraph_audit import audit_paths as audit_callgraph


class PlayerContainerAnalysisError(ValueError):
    """Raised when source provenance or combined analysis is invalid."""


def _stat_signature(info: os.stat_result) -> tuple[int, int, int, int, int]:
    return (
        info.st_dev,
        info.st_ino,
        info.st_size,
        info.st_mtime_ns,
        info.st_ctime_ns,
    )


def hash_gsc_tree(root: Path) -> dict[str, Any]:
    """Return a stable, content-addressed manifest for every GSC file in a mission."""
    root = Path(root).expanduser().resolve()
    if not root.is_dir():
        raise PlayerContainerAnalysisError(f"mission directory does not exist: {root}")
    files = sorted(
        (path for path in root.rglob("*.gsc") if path.is_file()),
        key=lambda item: item.relative_to(root).as_posix().casefold(),
    )
    if not files:
        raise PlayerContainerAnalysisError(f"no GSC source files found under {root}")

    entries: list[dict[str, Any]] = []
    tree_digest = hashlib.sha256()
    total_bytes = 0
    for path in files:
        if path.is_symlink():
            raise PlayerContainerAnalysisError(f"symbolic-link GSC inputs are not accepted: {path}")
        file_digest = hashlib.sha256()
        bytes_read = 0
        with path.open("rb") as stream:
            before = os.fstat(stream.fileno())
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                file_digest.update(chunk)
                bytes_read += len(chunk)
            after = os.fstat(stream.fileno())
        current_path = path.stat()
        if (
            bytes_read != before.st_size
            or _stat_signature(before) != _stat_signature(after)
            or _stat_signature(after) != _stat_signature(current_path)
        ):
            raise PlayerContainerAnalysisError(
                f"GSC source changed while being hashed; retry the analysis: {path}"
            )

        relative = path.relative_to(root).as_posix()
        digest = file_digest.hexdigest()
        entries.append({
            "path": relative,
            "size_bytes": bytes_read,
            "sha256": digest,
        })
        tree_digest.update(relative.encode("utf-8"))
        tree_digest.update(b"\0")
        tree_digest.update(str(bytes_read).encode("ascii"))
        tree_digest.update(b"\0")
        tree_digest.update(bytes.fromhex(digest))
        total_bytes += bytes_read

    return {
        "root": str(root),
        "file_count": len(entries),
        "total_bytes": total_bytes,
        "sha256": tree_digest.hexdigest(),
        "files": entries,
    }


def _atomic_write_json(path: Path, payload: dict[str, Any], overwrite: bool) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and not overwrite:
        raise FileExistsError(f"refusing to overwrite existing report: {path}")
    rendered = json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            newline="\n",
            prefix=path.name + ".",
            suffix=".tmp",
            dir=path.parent,
            delete=False,
        ) as stream:
            temporary_path = Path(stream.name)
            stream.write(rendered)
            stream.flush()
            os.fsync(stream.fileno())
        if overwrite:
            os.replace(temporary_path, path)
        else:
            # A hard-link creation is atomic and fails if another process created
            # the destination after the initial existence check.
            os.link(temporary_path, path)
            temporary_path.unlink()
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()


def run_analysis(
    missions: dict[str, Path],
    output_json: Path | None = None,
    overwrite: bool = False,
) -> dict[str, Any]:
    if not missions:
        raise PlayerContainerAnalysisError("provide at least one mission directory")
    if len(missions) != len(set(missions)):
        raise PlayerContainerAnalysisError("mission names must be unique")
    if output_json is not None and Path(output_json).exists() and not overwrite:
        raise FileExistsError(f"refusing to overwrite existing report: {output_json}")

    before = {
        name: hash_gsc_tree(path)
        for name, path in sorted(missions.items())
    }
    ownership = audit_ownership(missions)
    target_definitions: dict[str, list[dict[str, str]]] = {}
    for mission in ownership["missions"]:
        target_definitions[mission["mission"]] = [
            {"file": item["file"], "function": item["function"]}
            for item in mission["player_container_functions"]
        ]

    callgraph = audit_callgraph(missions, target_definitions=target_definitions)
    after = {
        name: hash_gsc_tree(path)
        for name, path in sorted(missions.items())
    }
    changed = [
        name for name in sorted(before)
        if before[name]["sha256"] != after[name]["sha256"]
        or before[name]["files"] != after[name]["files"]
    ]
    if changed:
        raise PlayerContainerAnalysisError(
            "GSC source set changed during analysis; no combined report was written: "
            + ", ".join(changed)
        )

    target_rows = [
        item
        for mission in callgraph["missions"]
        for item in mission["target_findings"]
    ]
    summary = {
        "mission_count": len(missions),
        "source_file_count": sum(item["file_count"] for item in before.values()),
        "source_total_bytes": sum(item["total_bytes"] for item in before.values()),
        "source_tree_sha256": {
            name: item["sha256"] for name, item in sorted(before.items())
        },
        "player_container_function_count": len(target_rows),
        "exact_targets_found": sum(
            item["selection_status"] == "FOUND_EXACT_FILE_FUNCTION"
            for item in target_rows
        ),
        "exact_targets_missing_or_ambiguous": sum(
            item["selection_status"] != "FOUND_EXACT_FILE_FUNCTION"
            for item in target_rows
        ),
        "incoming_call_site_count": sum(item["call_site_count"] for item in target_rows),
        "function_pointer_count": sum(item["function_pointer_count"] for item in target_rows),
        "directly_reachable_target_count": sum(
            item["reachable_from_script_roots_without_callbacks"]
            for item in target_rows
        ),
        "callback_inclusive_reachable_target_count": sum(
            item["reachable_from_script_roots_with_callbacks"]
            for item in target_rows
        ),
    }
    report = {
        "schema_version": 1,
        "tool": "run_player_container_analysis",
        "status": (
            "PLAYER_CONTAINER_CALLGRAPH_COMPLETED"
            if summary["exact_targets_missing_or_ambiguous"] == 0
            else "PLAYER_CONTAINER_CALLGRAPH_PARTIAL"
        ),
        "summary": summary,
        "source_manifests": before,
        "ownership_audit": ownership,
        "callgraph_audit": callgraph,
        "validation_boundary": (
            "Stable, read-only static GSC source analysis. Direct call sites and callback references "
            "do not prove runtime execution or a native Campaign second local player."
        ),
    }
    if output_json is not None:
        _atomic_write_json(Path(output_json), report, overwrite)
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("missions", nargs="+", help="NAME=PATH pairs containing decompiled .gsc files")
    parser.add_argument("--output-json", required=True, type=Path)
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args(argv)
    missions: dict[str, Path] = {}
    try:
        for item in args.missions:
            if "=" not in item:
                raise PlayerContainerAnalysisError(f"expected NAME=PATH, got {item!r}")
            name, raw_path = item.split("=", 1)
            if not name or not raw_path or name in missions:
                raise PlayerContainerAnalysisError(f"invalid or duplicate mission mapping: {item!r}")
            missions[name] = Path(raw_path)
        report = run_analysis(missions, args.output_json, args.overwrite)
        print(json.dumps({"status": report["status"], "summary": report["summary"]}, indent=2))
        return 0 if report["status"] == "PLAYER_CONTAINER_CALLGRAPH_COMPLETED" else 1
    except (OSError, PlayerContainerAnalysisError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
