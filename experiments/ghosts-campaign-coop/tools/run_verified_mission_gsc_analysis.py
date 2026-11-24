#!/usr/bin/env python3
"""Gate the player-container GSC analysis on exact, stable mission FastFile identities."""
from __future__ import annotations

import argparse
import json
import os
import sys
import tempfile
from pathlib import Path
from typing import Any

try:
    from .verify_ghosts_mission_fastfiles import EXPECTED_FASTFILES, verify_fastfiles
    from .run_player_container_analysis import hash_gsc_tree, run_analysis
except ImportError:
    from verify_ghosts_mission_fastfiles import EXPECTED_FASTFILES, verify_fastfiles
    from run_player_container_analysis import hash_gsc_tree, run_analysis


class VerifiedMissionAnalysisError(ValueError):
    """Raised when inputs or output paths violate the verified-analysis contract."""


def _path_key(path: Path | str) -> str:
    return os.path.normcase(str(Path(path).expanduser().resolve())).casefold()


def _identity_signature(report: dict[str, Any]) -> dict[str, tuple[Any, ...]]:
    return {
        role: (
            item.get("status"),
            item.get("size_bytes"),
            item.get("sha256"),
            item.get("magic"),
            item.get("version"),
        )
        for role, item in sorted(report.get("files", {}).items())
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
            os.link(temporary_path, path)
            temporary_path.unlink()
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()


def _reject_output_aliases(
    output_json: Path | None,
    fastfiles: dict[str, Path | str],
    missions: dict[str, Path],
) -> None:
    if output_json is None:
        return
    output_key = _path_key(output_json)
    for role, path in fastfiles.items():
        if _path_key(path) == output_key:
            raise VerifiedMissionAnalysisError(
                f"report output cannot overwrite the {role} FastFile input: {path}"
            )
    for mission, root in missions.items():
        manifest = hash_gsc_tree(root)
        for entry in manifest["files"]:
            source = Path(manifest["root"]) / entry["path"]
            if _path_key(source) == output_key:
                raise VerifiedMissionAnalysisError(
                    f"report output cannot overwrite a {mission} GSC source file: {source}"
                )


def run_verified_analysis(
    fastfiles: dict[str, Path | str],
    missions: dict[str, Path],
    output_json: Path | None = None,
    overwrite: bool = False,
) -> dict[str, Any]:
    if set(fastfiles) != set(EXPECTED_FASTFILES):
        raise VerifiedMissionAnalysisError(
            "provide exactly these FastFile roles: " + ", ".join(sorted(EXPECTED_FASTFILES))
        )
    if set(missions) != set(EXPECTED_FASTFILES):
        raise VerifiedMissionAnalysisError(
            "provide exactly these mission GSC roles: " + ", ".join(sorted(EXPECTED_FASTFILES))
        )
    if output_json is not None and Path(output_json).exists() and not overwrite:
        raise FileExistsError(f"refusing to overwrite existing report: {output_json}")

    _reject_output_aliases(output_json, fastfiles, missions)
    before = verify_fastfiles(fastfiles)
    if before["status"] != "MATCHED_MISSION_FASTFILES":
        report = {
            "schema_version": 1,
            "tool": "run_verified_mission_gsc_analysis",
            "status": "BLOCKED_FASTFILE_IDENTITY_MISMATCH",
            "fastfile_verification": {"before": before, "after": None},
            "mission_analysis": None,
            "errors": before["errors"],
            "validation_boundary": "No GSC ownership or call-graph analysis runs unless all three exact mission FastFiles match.",
        }
        if output_json is not None:
            _atomic_write_json(output_json, report, overwrite)
        return report

    mission_analysis = run_analysis(missions)
    after = verify_fastfiles(fastfiles)
    same_inputs = (
        after["status"] == "MATCHED_MISSION_FASTFILES"
        and _identity_signature(before) == _identity_signature(after)
    )
    report = {
        "schema_version": 1,
        "tool": "run_verified_mission_gsc_analysis",
        "status": (
            "VERIFIED_MISSION_GSC_ANALYSIS_COMPLETED"
            if same_inputs and mission_analysis["status"] == "PLAYER_CONTAINER_CALLGRAPH_COMPLETED"
            else "BLOCKED_INPUTS_CHANGED_OR_ANALYSIS_PARTIAL"
        ),
        "fastfile_verification": {"before": before, "after": after},
        "mission_analysis": mission_analysis,
        "source_tree_sha256": mission_analysis["summary"]["source_tree_sha256"],
        "errors": [] if same_inputs else [
            "One or more FastFile identities changed or failed verification during the GSC analysis."
        ],
        "validation_boundary": (
            "The exact supplied FastFile identities and stable GSC source trees are verified. "
            "The resulting call graph is static evidence only and does not prove runtime Player 2 initialization."
        ),
    }
    if output_json is not None:
        _atomic_write_json(output_json, report, overwrite)
    return report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--black-ice-fastfile", type=Path, required=True)
    parser.add_argument("--carrier-fastfile", type=Path, required=True)
    parser.add_argument("--cornered-fastfile", type=Path, required=True)
    parser.add_argument("--black-ice-gsc", type=Path, required=True)
    parser.add_argument("--carrier-gsc", type=Path, required=True)
    parser.add_argument("--cornered-gsc", type=Path, required=True)
    parser.add_argument("--output-json", type=Path, required=True)
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args(argv)
    try:
        report = run_verified_analysis(
            {
                "black_ice": args.black_ice_fastfile,
                "carrier": args.carrier_fastfile,
                "cornered": args.cornered_fastfile,
            },
            {
                "black_ice": args.black_ice_gsc,
                "carrier": args.carrier_gsc,
                "cornered": args.cornered_gsc,
            },
            args.output_json,
            args.overwrite,
        )
        print(json.dumps({"status": report["status"]}, indent=2))
        return 0 if report["status"] == "VERIFIED_MISSION_GSC_ANALYSIS_COMPLETED" else 1
    except (OSError, VerifiedMissionAnalysisError, ValueError) as exc:
        print(f"verified mission analysis failed: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
