#!/usr/bin/env python3
"""Verify a Ghosts TU1.16 capture/development kit against its SHA-256 manifest."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath

REQUIRED_FILES = (
    "PS3/ghosts_campaign_pad_probe.fake.sprx",
    "PS3/ghosts_pad_input_probe.fake.sprx",
    "PS3/ghosts_coop/ghosts_coop.fake.sprx",
    "Experimental_GSC_Not_Installed/ghc_campaign_player_registry.gscbin",
    "Experimental_GSC_Not_Installed/ghc_campaign_local_session.gscbin",
    "PS3/dump_campaign_tu116.bat",
    "PS3/dump_multiplayer_tu116.bat",
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _safe_path(root: Path, relative: str) -> Path:
    pure = PurePosixPath(relative)
    if pure.is_absolute() or not pure.parts or any(part in ("", ".", "..") for part in pure.parts):
        raise ValueError(f"unsafe manifest path: {relative!r}")
    candidate = root.joinpath(*pure.parts)
    try:
        candidate.resolve().relative_to(root.resolve())
    except ValueError as exc:
        raise ValueError(f"manifest path escapes package root: {relative!r}") from exc
    return candidate


def _validate_gsc_manifest(root: Path, name: str) -> list[str]:
    base = Path("Experimental_GSC_Not_Installed")
    manifest_path = root / base / f"{name}.manifest.json"
    source_path = root / base / f"{name}.gsc"
    binary_path = root / base / f"{name}.gscbin"
    errors: list[str] = []

    if not manifest_path.is_file():
        return [f"missing GSC manifest: {manifest_path.relative_to(root)}"]
    if not source_path.is_file():
        errors.append(f"missing GSC source: {source_path.relative_to(root)}")
    if not binary_path.is_file():
        errors.append(f"missing GSCBIN: {binary_path.relative_to(root)}")
    if errors:
        return errors

    try:
        data = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return [f"invalid GSC manifest {manifest_path.name}: {exc}"]

    if data.get("source_sha256") != _sha256(source_path):
        errors.append(f"{name}: source SHA-256 mismatch")
    if data.get("compiled_gscbin_sha256") != _sha256(binary_path):
        errors.append(f"{name}: compiled GSCBIN SHA-256 mismatch")
    if data.get("compiled_gscbin_bytes") != binary_path.stat().st_size:
        errors.append(f"{name}: compiled GSCBIN size mismatch")
    if "not injected or runtime-tested" not in data.get("status", "").lower():
        errors.append(f"{name}: manifest must state that the GSC is not injected/runtime-tested")
    return errors


def _validate_event_lock_map(root: Path) -> list[str]:
    map_path = root / "Reports" / "PHASE88_MISSION_EVENT_LOCK_MAP.json"
    tool_path = root / "Tools" / "verify_mission_event_lock_map.py"
    if not map_path.exists() and not tool_path.exists():
        return []
    if not map_path.is_file() or not tool_path.is_file():
        return ["mission event map and validator must both be present"]

    source_path = root / "Experimental_GSC_Not_Installed" / "ghc_campaign_local_session.gsc"
    if not source_path.is_file():
        return ["mission event map requires the Local Session GSC source"]

    try:
        data = json.loads(map_path.read_text(encoding="utf-8"))
        spec = importlib.util.spec_from_file_location("ghosts_event_map_validator", tool_path)
        if spec is None or spec.loader is None:
            return ["unable to load mission event map validator"]
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        errors = module.validate_map(data, source_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError, AttributeError) as exc:
        return [f"mission event map validation failed to run: {exc}"]
    return [f"mission event map: {error}" for error in errors]



def _validate_event_activator_audit(root: Path) -> list[str]:
    report_path = root / "Reports" / "PHASE89_MISSION_EVENT_ACTIVATOR_AUDIT.json"
    tool_path = root / "Tools" / "mission_event_activator_audit.py"
    if not report_path.exists() and not tool_path.exists():
        return []
    if not report_path.is_file() or not tool_path.is_file():
        return ["mission event activator audit report and validator must both be present"]

    try:
        data = json.loads(report_path.read_text(encoding="utf-8"))
        spec = importlib.util.spec_from_file_location("ghosts_event_activator_audit", tool_path)
        if spec is None or spec.loader is None:
            return ["unable to load mission event activator audit validator"]
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        errors = module.validate_report(data)
    except (OSError, json.JSONDecodeError, AttributeError) as exc:
        return [f"mission event activator audit validation failed to run: {exc}"]
    return [f"mission event activator audit: {error}" for error in errors]




def _validate_event_refactor_graph(root: Path) -> list[str]:
    report_path = root / "Reports" / "PHASE90_MISSION_EVENT_REFACTOR_GRAPH.json"
    tool_path = root / "Tools" / "mission_event_activator_audit.py"
    if not report_path.exists() and not tool_path.exists():
        return []
    if not report_path.is_file() or not tool_path.is_file():
        return ["mission event refactor graph and validator must both be present"]

    try:
        data = json.loads(report_path.read_text(encoding="utf-8"))
        spec = importlib.util.spec_from_file_location("ghosts_event_refactor_graph", tool_path)
        if spec is None or spec.loader is None:
            return ["unable to load mission event refactor graph validator"]
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        errors = module.validate_dependency_graph(data)
    except (OSError, json.JSONDecodeError, AttributeError, TypeError, ValueError) as exc:
        return [f"mission event refactor graph validation failed to run: {exc}"]
    return [f"mission event refactor graph: {error}" for error in errors]



def validate_package(root: Path) -> tuple[list[str], dict]:
    root = Path(root).resolve()
    manifest_path = root / "MANIFEST.json"
    errors: list[str] = []
    if not manifest_path.is_file():
        return [f"missing MANIFEST.json in {root}"], {}

    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return [f"invalid MANIFEST.json: {exc}"], {}

    target = manifest.get("target", {})
    if target.get("title_id") != "BLUS31270" or target.get("app_ver") != "01.16":
        errors.append("package target must be BLUS31270 / TU 01.16")
    if "not a playable co-op mod" not in manifest.get("scope", "").lower():
        errors.append("manifest scope must state that this is not a playable co-op mod")

    file_entries = manifest.get("files")
    if not isinstance(file_entries, dict) or not file_entries:
        errors.append("manifest files must be a non-empty object")
        file_entries = {}

    verified = 0
    for relative, expected in sorted(file_entries.items()):
        try:
            path = _safe_path(root, relative)
        except ValueError as exc:
            errors.append(str(exc))
            continue
        if not path.is_file():
            errors.append(f"missing file: {relative}")
            continue
        if path.stat().st_size != expected.get("size_bytes"):
            errors.append(f"size mismatch: {relative}")
            continue
        if _sha256(path) != expected.get("sha256"):
            errors.append(f"SHA-256 mismatch: {relative}")
            continue
        verified += 1

    for relative in REQUIRED_FILES:
        try:
            path = _safe_path(root, relative)
        except ValueError as exc:
            errors.append(str(exc))
            continue
        if not path.is_file():
            errors.append(f"required package file missing: {relative}")

    # Experimental GSC scripts must never be packaged as PS3 PRX payloads.
    ps3_dir = root / "PS3"
    if ps3_dir.exists():
        for path in ps3_dir.rglob("*.gscbin"):
            errors.append(f"GSCBIN must remain under Experimental_GSC_Not_Installed, found {path.relative_to(root)}")

    for name in ("ghc_campaign_player_registry", "ghc_campaign_local_session"):
        errors.extend(_validate_gsc_manifest(root, name))
    errors.extend(_validate_event_lock_map(root))
    errors.extend(_validate_event_activator_audit(root))
    errors.extend(_validate_event_refactor_graph(root))

    summary = {
        "package": manifest.get("package"),
        "target": target,
        "files_declared": len(file_entries),
        "files_verified": verified,
        "gsc_manifests_verified": 2,
        "event_activator_audit_verified": (root / "Reports" / "PHASE89_MISSION_EVENT_ACTIVATOR_AUDIT.json").is_file()
        and (root / "Tools" / "mission_event_activator_audit.py").is_file(),
        "event_refactor_graph_verified": (root / "Reports" / "PHASE90_MISSION_EVENT_REFACTOR_GRAPH.json").is_file()
        and (root / "Tools" / "mission_event_activator_audit.py").is_file(),
    }
    return errors, summary


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "package_dir",
        nargs="?",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="extracted kit directory containing MANIFEST.json",
    )
    args = parser.parse_args(argv)
    errors, summary = validate_package(args.package_dir)
    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1
    print("PASS: Ghosts TU1.16 kit integrity verified")
    for key, value in summary.items():
        print(f"{key}: {value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
