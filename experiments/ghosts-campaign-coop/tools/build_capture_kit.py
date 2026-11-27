#!/usr/bin/env python3
"""Assemble the reproducible Ghosts TU1.16 development and capture kit from CI artifacts."""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import tempfile
import zipfile
from pathlib import Path

try:
    from .verify_capture_kit import validate_package
except ImportError:  # Direct script execution in the CI packaging job.
    from verify_capture_kit import validate_package

PS3MAPI_FILES = (
    "CAPTURE_METADATA_TEMPLATE.txt",
    "README_AR.md",
    "SELF_MEMORY_MAP_TU116.json",
    "TU116_EXECUTABLE_MANIFEST.json",
    "analyze_tu116_runtime_capture.py",
    "CAMPAIGN_NATIVE_PLAYER_CAPTURE_TEMPLATE.json",
    "campaign_capture_binding.py",
    "validate_campaign_player_pair_capture.py",
    "compare_runtime_strings.py",
    "diff_runtime_elfs.py",
    "dump_campaign_tu116.bat",
    "dump_multiplayer_tu116.bat",
    "reconstruct_elf_from_ps3mapi_dumps.py",
    "scan_clockwork_script_asset.py",
    "scan_runtime_strings.py",
    "verify_capture_metadata.py",
    "verify_tu116_dump.py",
)


def _find_one(root: Path, filename: str) -> Path:
    matches = [path for path in root.rglob(filename) if path.is_file()]
    if len(matches) != 1:
        raise FileNotFoundError(
            f"expected exactly one {filename!r} under {root}, found {len(matches)}"
        )
    return matches[0]


def _copy_one(root: Path, filename: str, destination: Path) -> Path:
    source = _find_one(root, filename)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    return destination


def _copy_tree(source: Path, destination: Path) -> None:
    if not source.is_dir():
        raise FileNotFoundError(f"artifact directory missing: {source}")
    for path in source.rglob("*"):
        if path.is_file():
            relative = path.relative_to(source)
            target = destination / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_capture_kit(
    project_root: Path,
    artifacts_root: Path,
    output: Path,
    run_metadata: dict | None = None,
) -> dict:
    project_root = Path(project_root).resolve()
    artifacts_root = Path(artifacts_root).resolve()
    output = Path(output).resolve()
    run_metadata = run_metadata or {}

    artifact_dirs = {
        name: artifacts_root / name
        for name in ("pad-probe", "input-probe", "bootstrap", "session", "registry")
    }
    for path in artifact_dirs.values():
        if not path.is_dir():
            raise FileNotFoundError(f"missing downloaded CI artifact directory: {path}")

    with tempfile.TemporaryDirectory(prefix="ghosts-tu116-kit-") as temp:
        root = Path(temp) / "Ghosts_TU116_Development_Capture_Kit"
        root.mkdir(parents=True)

        # Diagnostic probes and mode selectors.
        for filename in (
            "ghosts_campaign_pad_probe.prx",
            "ghosts_campaign_pad_probe.fake.sprx",
            "ghosts_campaign_pad_probe.elf",
        ):
            _copy_one(artifact_dirs["pad-probe"], filename, root / "PS3" / filename)
        _copy_one(
            artifact_dirs["pad-probe"], "README_AR.md",
            root / "Docs" / "PAD_PROBE_README_AR.md",
        )
        mode_files = list((artifact_dirs["pad-probe"]).rglob("ghosts_probe_mode_*.txt"))
        if len(mode_files) != 2:
            raise FileNotFoundError(f"expected Campaign and Multiplayer mode files, found {len(mode_files)}")
        mode_destination = root / "PS3" / "mode_files"
        mode_destination.mkdir(parents=True, exist_ok=True)
        for source in mode_files:
            shutil.copy2(source, mode_destination / source.name)

        for filename in (
            "ghosts_pad_input_probe.prx",
            "ghosts_pad_input_probe.fake.sprx",
            "ghosts_pad_input_probe.elf",
        ):
            _copy_one(artifact_dirs["input-probe"], filename, root / "PS3" / filename)
        _copy_one(
            artifact_dirs["input-probe"], "README_AR.md",
            root / "Docs" / "PAD_INPUT_PROBE_README_AR.md",
        )

        # CEX bootstrap PRX build; flatten the artifact's build/ folder.
        for filename in (
            "ghosts_coop.prx",
            "ghosts_coop.fake.sprx",
            "ghosts_coop.elf",
            "libghosts_coop_stub.a",
        ):
            _copy_one(
                artifact_dirs["bootstrap"], filename,
                root / "PS3" / "ghosts_coop" / filename,
            )

        # Experimental GSC scripts are deliberately kept outside PS3 payloads.
        for filename in (
            "ghc_campaign_local_session.gsc",
            "ghc_campaign_local_session.gscbin",
            "ghc_campaign_local_session.manifest.json",
        ):
            _copy_one(
                artifact_dirs["session"], filename,
                root / "Experimental_GSC_Not_Installed" / filename,
            )
        for filename in (
            "ghc_campaign_player_registry.gscbin",
            "ghc_campaign_player_registry.manifest.json",
        ):
            _copy_one(
                artifact_dirs["registry"], filename,
                root / "Experimental_GSC_Not_Installed" / filename,
            )
        registry_source = (
            project_root / "client" / "ps3" / "gsc" / "ghc_campaign_player_registry.gsc"
        )
        if not registry_source.is_file():
            raise FileNotFoundError(f"missing registry source: {registry_source}")
        shutil.copy2(
            registry_source,
            root / "Experimental_GSC_Not_Installed" / registry_source.name,
        )

        # Arabic quick starts and static mission trigger inventory.
        quickstart = (
            project_root / "docs" / "phase116-ps3-runtime-capture-quickstart-ar.md"
        )
        if not quickstart.is_file():
            raise FileNotFoundError(f"missing PS3 runtime capture quick start: {quickstart}")
        shutil.copy2(
            quickstart,
            root / "Docs" / "PS3_RUNTIME_CAPTURE_QUICKSTART_AR.md",
        )
        callsite_doc = project_root / "docs" / "phase118-trigger-callsite-inventory.md"
        if not callsite_doc.is_file():
            raise FileNotFoundError(f"missing trigger call-site inventory: {callsite_doc}")
        shutil.copy2(
            callsite_doc,
            root / "Docs" / "PHASE118_TRIGGER_CALLSITE_INVENTORY.md",
        )
        phase119_doc = project_root / "docs" / "phase119-inventory-validation-gate.md"
        if not phase119_doc.is_file():
            raise FileNotFoundError(f"missing Phase 119 validation gate document: {phase119_doc}")
        shutil.copy2(
            phase119_doc,
            root / "Docs" / "PHASE119_INVENTORY_VALIDATION_GATE.md",
        )
        phase120_doc = project_root / "docs" / "phase120-player-pair-capture-validator-hardening.md"
        if not phase120_doc.is_file():
            raise FileNotFoundError(f"missing Phase 120 player-pair validator document: {phase120_doc}")
        shutil.copy2(
            phase120_doc,
            root / "Docs" / "PHASE120_PLAYER_PAIR_CAPTURE_VALIDATOR_HARDENING.md",
        )
        phase121_doc = project_root / "docs" / "phase121-cross-platform-kit-import-shim.md"
        if not phase121_doc.is_file():
            raise FileNotFoundError(f"missing Phase 121 import shim document: {phase121_doc}")
        shutil.copy2(
            phase121_doc,
            root / "Docs" / "PHASE121_CROSS_PLATFORM_KIT_IMPORT_SHIM.md",
        )

        quickstart = (
            project_root / "docs" / "phase116-ps3-runtime-capture-quickstart-ar.md"
        )
        if not quickstart.is_file():
            raise FileNotFoundError(f"missing PS3 runtime capture quick start: {quickstart}")
        # Capture scripts, runtime analyzers, and integrity verifier.
        ps3mapi_root = project_root / "tools" / "ps3mapi"
        ps3mapi_destination = root / "Tools" / "ps3mapi"
        ps3mapi_destination.mkdir(parents=True, exist_ok=True)
        for filename in PS3MAPI_FILES:
            source = ps3mapi_root / filename
            if not source.is_file():
                raise FileNotFoundError(f"missing PS3MAPI tool: {source}")
            shutil.copy2(source, ps3mapi_destination / filename)
        for filename in (
            "compare_pad_probe_logs.py",
            "compare_pad_input_probe_logs.py",
            "verify_capture_kit.py",
            "verify_mission_event_lock_map.py",
            "mission_event_activator_audit.py",
            "verify_phase118_trigger_inventory.py",
            "compare_ppu_function_inventories.py",
            "compare_splitscreen_builtin_xrefs.py",
            "ps3_elf_inspect.py",
            "run_ghidra_ppu_inventory.py",
        ):
            source = project_root / "tools" / filename
            if not source.is_file():
                raise FileNotFoundError(f"missing analyzer tool: {source}")
            shutil.copy2(source, root / "Tools" / filename)

        ghidra_sources = project_root / "tools" / "ghidra"
        if not ghidra_sources.is_dir():
            raise FileNotFoundError(f"missing Ghidra script directory: {ghidra_sources}")
        _copy_tree(ghidra_sources, root / "Tools" / "ghidra")

        # Python imports use lowercase "tools.*"; keep the display folder as "Tools"
        # while making the package portable to case-sensitive filesystems.
        import_package = root / "tools"
        import_package.mkdir(parents=True, exist_ok=True)
        (import_package / "__init__.py").write_text(
            '"""Case-sensitive import shim for the packaged Tools directory."""\\n'
            "from pathlib import Path\\n"
            '__path__ = [str(Path(__file__).resolve().parent.parent / "Tools")]\\n',
            encoding="utf-8",
        )

        # Keep the two dump scripts at the documented PS3 root paths too.
        for filename in ("dump_campaign_tu116.bat", "dump_multiplayer_tu116.bat"):
            shutil.copy2(root / "Tools" / "ps3mapi" / filename, root / "PS3" / filename)

        # Include phase reports when present.
        reports_dir = root / "Reports"
        reports_dir.mkdir(parents=True, exist_ok=True)
        for filename in (
            "PHASE84_REPORT_AR.md",
            "PHASE85_REPORT_AR.md",
            "PHASE86_REPORT_AR.md",
            "PHASE84_MISSION_EVENT_OWNERSHIP.json",
            "PHASE85_EVENT_LOCK_API.json",
            "PHASE86_CAPTURE_KIT_VALIDATION.json",
            "PHASE87_REPORT_AR.md",
            "PHASE87_CAPTURE_KIT_CI.json",
            "PHASE88_REPORT_AR.md",
            "PHASE88_EVENT_LOCK_PLAN.json",
            "PHASE88_MISSION_EVENT_LOCK_MAP.json",
            "PHASE89_REPORT_AR.md",
            "PHASE89_MISSION_EVENT_ACTIVATOR_AUDIT.json",
            "PHASE90_REPORT_AR.md",
            "PHASE90_MISSION_EVENT_REFACTOR_GRAPH.json",
            "PHASE112_REPORT_AR.md",
            "PHASE112_VALIDATION.json",
            "PHASE113_REPORT_AR.md",
            "PHASE113_VALIDATION.json",
            "PHASE114_REPORT_AR.md",
            "PHASE114_VALIDATION.json",
            "PHASE115_REPORT_AR.md",
            "PHASE115_VALIDATION.json",
            "PHASE117_REPORT_AR.md",
            "PHASE117_VALIDATION.json",
            "PHASE118_REPORT_AR.md",
            "PHASE118_TRIGGER_CALLSITE_INVENTORY.json",
            "PHASE118_VALIDATION.json",
            "PHASE119_REPORT_AR.md",
            "PHASE119_VALIDATION.json",
            "PHASE120_REPORT_AR.md",
            "PHASE120_VALIDATION.json",
            "PHASE121_REPORT_AR.md",
            "PHASE121_VALIDATION.json",
        ):
            source = project_root / "reports" / filename
            if source.is_file():
                shutil.copy2(source, reports_dir / filename)

        readme = """# Ghosts TU 1.16 Development & Capture Kit

Target: Call of Duty: Ghosts PS3 CEX, BLUS31270, Title Update 01.16.

## Verify the package first

Run from the extracted package root:

    python Tools/verify_capture_kit.py

Do not copy files to the console unless the validator prints PASS.

The event-lock plan can also be checked explicitly:

    python Tools/verify_mission_event_lock_map.py --map Reports/PHASE88_MISSION_EVENT_LOCK_MAP.json --gsc Experimental_GSC_Not_Installed/ghc_campaign_local_session.gsc

The mission trigger/activator audit report is pinned to the original source archive. To reproduce it, provide the original archive:

    python Tools/mission_event_activator_audit.py --archive Ghosts_Campaign_Missions_Phase31.zip --json-out Reports/PHASE89_MISSION_EVENT_ACTIVATOR_AUDIT.json

The Phase 118 call-site inventory is validated with:

    python Tools/verify_phase118_trigger_inventory.py --inventory Reports/PHASE118_TRIGGER_CALLSITE_INVENTORY.json --validation Reports/PHASE118_VALIDATION.json --report Reports/PHASE118_REPORT_AR.md

## Contents

- PS3 CEX bootstrap PRX build and the two read-only controller probes.
- Campaign and Multiplayer probe mode files.
- PS3MAPI memory-dump scripts and TU1.16 runtime analyzers.
- Experimental GSC source and GSCBIN under Experimental_GSC_Not_Installed only.
- Selected Phase 84–90 and Phase 112–121 reports, event-lock map, activator-lineage audit, mission refactor graph, complete Campaign/Multiplayer analyzer dependency set, native player-pair capture template/validator, cross-platform Python import shim, Arabic runtime capture quick start, validated trigger call-site inventory and MANIFEST.json with per-file SHA-256 checksums.

## Recommended offline test

Run DualShock Input Probe v1 alone in Campaign Offline, unload it, then repeat in Multiplayer Offline. Do not run it together with Pad Probe v4. The input probe reads cellPadGetData from a separate thread; actual-console stability still requires verification.

Use the comparators in Tools to analyze both logs. A passing port-input comparison proves only that distinct data was returned for ports 0 and 1. It does not prove a second native Local Client or a playable second Campaign player.

## Experimental GSC warning

The Local Session bridge and player registry are compiled GSC scripts, not PRX files. They are not inserted into a retail FastFile and are not runtime-tested. Do not load GSCBIN through PS3MAPI. The bridge includes activator-aware entity-trigger and player-command adapters, but neither is wired into original mission handlers yet.

## Phase 119 inventory gate

The Phase 118 inventory validator runs as part of python Tools/verify_capture_kit.py. It checks all 15 pinned legacy call sites, the four blocked shared interactions, and the NOT_PROVEN runtime boundary.

## Phase 120 player-pair capture

The package includes the native Campaign player-pair template and validator, the SELF-binding helper, and every Python module imported by the combined runtime analyzer. Optional Ghidra analysis scripts are included as well. The verifier rejects a kit missing any of these dependencies.

Fill the template only with measured PS3 runtime values, calculate campaign_self_sha256 from the exact Campaign default.self, and run:

    python Tools/ps3mapi/validate_campaign_player_pair_capture.py --capture campaign_player_pair_capture.json

The combined analyzer checks the SELF hash binding when the capture is supplied. Structural validation does not authenticate the operator or prove a physical PS3 capture; Player 2 remains NOT_PROVEN until genuine runtime evidence is reviewed.

## Phase 121 cross-platform imports

The lowercase tools/__init__.py shim maps Python's tools.* imports to the user-facing Tools directory. This keeps the ZIP layout readable while allowing the combined analyzer to start on case-sensitive filesystems.

## Not yet proven

- Native PS3 Local Client 1.
- Second Campaign player entity, independent camera, HUD or command stream.
- Playable Campaign co-op or split-screen.
- Runtime stability of the diagnostic probes on physical hardware.

Do not send PSN credentials, IDPS, PSID or license files for analysis.
"""
        (root / "README_AR.md").write_text(readme, encoding="utf-8")

        manifest = {
            "package": "Ghosts TU 1.16 Development & Capture Kit",
            "target": {"title_id": "BLUS31270", "app_ver": "01.16", "platform": "PS3 CEX"},
            "scope": "Read-only capture probes and experimental GSC artifacts; not a playable co-op mod.",
            "ci": run_metadata,
            "files": {},
        }
        for path in sorted(p for p in root.rglob("*") if p.is_file() and p.name != "MANIFEST.json"):
            manifest["files"][str(path.relative_to(root)).replace("\\", "/")] = {
                "size_bytes": path.stat().st_size,
                "sha256": _sha256(path),
            }
        (root / "MANIFEST.json").write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

        errors, summary = validate_package(root)
        if errors:
            raise RuntimeError("capture kit validation failed: " + "; ".join(errors))

        output.parent.mkdir(parents=True, exist_ok=True)
        if output.exists():
            output.unlink()
        with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in sorted(root.rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(root.parent))

    return {
        "output": str(output),
        "size_bytes": output.stat().st_size,
        "sha256": _sha256(output),
        "files_verified": summary["files_verified"],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--artifacts-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    run_metadata = {
        "workflow_run_number": __import__("os").environ.get("GITHUB_RUN_NUMBER"),
        "workflow_run_id": __import__("os").environ.get("GITHUB_RUN_ID"),
        "source_commit": __import__("os").environ.get("GITHUB_SHA"),
    }
    result = build_capture_kit(args.project_root, args.artifacts_root, args.output, run_metadata)
    print("PASS: reproducible Ghosts TU1.16 capture kit built and verified")
    for key, value in result.items():
        print(f"{key}: {value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
