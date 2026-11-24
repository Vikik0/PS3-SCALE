#!/usr/bin/env python3
"""Verify user-owned Call of Duty: Ghosts PS3 BLUS31270 TU 1.16 inputs.

Read-only: this tool hashes and inspects local PARAM.SFO, SELF and optional
FastFiles/decrypted PPU ELFs. It never decrypts, patches, copies or uploads
retail game files.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

try:
    from .ps3_self_inspect import SelfFormatError, inspect_self
    from .ps3_sfo_inspect import SfoFormatError, parse_sfo
    from .ps3_elf_inspect import ElfFormatError, PpuElf
except ImportError:
    from ps3_self_inspect import SelfFormatError, inspect_self
    from ps3_sfo_inspect import SfoFormatError, parse_sfo
    from ps3_elf_inspect import ElfFormatError, PpuElf


TARGET_TITLE_ID = "BLUS31270"
TARGET_APP_VERSION = "01.16"
EXPECTED_UPDATE_SFO_SHA256 = "bd329d97380933ce0fff988e7c196b74520a1ae83bd856943c39cb05d9a62025"

EXPECTED_SELF_FILES: dict[str, dict[str, Any]] = {
    "EBOOT.BIN": {
        "size_bytes": 41488,
        "sha256": "af0dc21621d4c12c7f63899d1eef7c66405c3aa78c13daccfca58d02dc165aab",
        "entry_point": "0x20080",
    },
    "default.self": {
        "size_bytes": 4291568,
        "sha256": "ffa0f174740cc588b85a8fe0a730695c858413572841a01cbcc2b161a6d9006e",
        "entry_point": "0x7E9508",
    },
    "default_mp.self": {
        "size_bytes": 6265632,
        "sha256": "6dfa702761860435e22f2db7bc8bb848f2286223605415523680770ea03f6bfd",
        "entry_point": "0xA75E38",
    },
}
EXPECTED_FASTFILES: dict[str, dict[str, Any]] = {
    "common.ff": {
        "size_bytes": 29796135,
        "sha256": "5fc31ea915894566896cc5081d3688675ecde341d42e8c38b19ad37434ca4bcb",
    },
    "clockwork.ff": {
        "size_bytes": 85855599,
        "sha256": "a2ac70c7349047f2eded5f45a5e46ff8278689839fe1655106263520fe639783",
    },
}


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _relative(path: Path, root: Path) -> str:
    try:
        return str(path.resolve().relative_to(root.resolve()))
    except ValueError:
        return path.name


def _find_unique(root: Path, filename: str) -> tuple[Path | None, str | None]:
    if not root.exists() or not root.is_dir():
        return None, f"input root is not a directory: {root}"
    matches = sorted(
        (path for path in root.rglob("*") if path.is_file() and path.name.casefold() == filename.casefold()),
        key=lambda path: str(path).casefold(),
    )
    if not matches:
        return None, f"missing {filename}"
    if len(matches) > 1:
        return None, f"ambiguous {filename}: {len(matches)} matching files"
    return matches[0], None


def _inspect_sfo(root: Path, label: str, expected_category: str, expected_app_version: str) -> tuple[dict[str, Any], list[str]]:
    errors: list[str] = []
    path, error = _find_unique(root, "PARAM.SFO")
    if error:
        return {"status": "MISSING_OR_AMBIGUOUS", "error": error}, [f"{label}: {error}"]
    assert path is not None
    try:
        data = path.read_bytes()
        parsed = parse_sfo(data)
    except (OSError, SfoFormatError) as exc:
        return {"status": "INVALID", "path": _relative(path, root), "error": str(exc)}, [f"{label}: {exc}"]
    entries = parsed["entries"]
    checks = {
        "title_id": entries.get("TITLE_ID") == TARGET_TITLE_ID,
        "category": entries.get("CATEGORY") == expected_category,
        "app_version": entries.get("APP_VER") == expected_app_version,
    }
    if not all(checks.values()):
        errors.append(
            f"{label}: expected TITLE_ID={TARGET_TITLE_ID}, CATEGORY={expected_category}, "
            f"APP_VER={expected_app_version}; found {entries.get('TITLE_ID')}, "
            f"{entries.get('CATEGORY')}, {entries.get('APP_VER')}"
        )
    if label == "installed TU PARAM.SFO" and _sha256(data) != EXPECTED_UPDATE_SFO_SHA256:
        errors.append(f"{label}: SHA-256 does not match the verified BLUS31270 TU 1.16 metadata file")
    return {
        "status": "VERIFIED" if not errors else "MISMATCH",
        "relative_path": _relative(path, root),
        "size_bytes": len(data),
        "sha256": _sha256(data),
        "title": entries.get("TITLE"),
        "title_id": entries.get("TITLE_ID"),
        "app_version": entries.get("APP_VER"),
        "version": entries.get("VERSION"),
        "category": entries.get("CATEGORY"),
        "checks": checks,
    }, errors


def _inspect_self_file(root: Path, filename: str, expected: dict[str, Any]) -> tuple[dict[str, Any], list[str]]:
    errors: list[str] = []
    path, error = _find_unique(root, filename)
    if error:
        return {"status": "MISSING_OR_AMBIGUOUS", "error": error}, [f"{filename}: {error}"]
    assert path is not None
    try:
        data = path.read_bytes()
        report = inspect_self(data, filename)
    except (OSError, SelfFormatError) as exc:
        return {"status": "INVALID", "relative_path": _relative(path, root), "error": str(exc)}, [f"{filename}: {exc}"]

    container = report["container"]
    app_info = container["app_info"]
    elf = report["embedded_elf_header"]
    encrypted_indices = [segment["index"] for segment in report["segments"] if segment["encrypted"]]
    checks = {
        "size_bytes": report["file_size"] == expected["size_bytes"],
        "sha256": report["sha256"] == expected["sha256"],
        "key_revision": container["key_revision"] == 28,
        "program_type": app_info["program_type"] == 8,
        "elf_class": elf["class"] == "ELF64",
        "endianness": elf["endianness"] == "big",
        "machine": elf["machine"] == "PowerPC64",
        "entry_point": elf["entry_point"].lower() == expected["entry_point"].lower(),
        "program_header_count": elf["program_header_count"] == 8,
        "encrypted_segments": encrypted_indices == [0, 1, 2, 3, 4],
    }
    for field, passed in checks.items():
        if not passed:
            errors.append(f"{filename}: {field} does not match the verified installed TU 1.16 profile")
    return {
        "status": "VERIFIED" if not errors else "MISMATCH",
        "relative_path": _relative(path, root),
        "size_bytes": report["file_size"],
        "sha256": report["sha256"],
        "key_revision": container["key_revision"],
        "program_type": app_info["program_type"],
        "elf": elf,
        "encrypted_segment_indices": encrypted_indices,
        "checks": checks,
        "decryption_status": "ENCRYPTED_SEGMENTS_PRESENT" if encrypted_indices else "NO_ENCRYPTED_SEGMENTS_REPORTED",
    }, errors


def _inspect_optional_fastfile(root: Path, filename: str, expected: dict[str, Any]) -> dict[str, Any]:
    path, error = _find_unique(root, filename)
    if error:
        return {"status": "NOT_FOUND_OPTIONAL", "error": error}
    assert path is not None
    data = path.read_bytes()
    checks = {
        "size_bytes": len(data) == expected["size_bytes"],
        "sha256": _sha256(data) == expected["sha256"],
    }
    return {
        "status": "VERIFIED" if all(checks.values()) else "MISMATCH",
        "relative_path": _relative(path, root),
        "size_bytes": len(data),
        "sha256": _sha256(data),
        "checks": checks,
    }


def _inspect_decrypted_elf(path: Path, expected_entry: str, label: str) -> tuple[dict[str, Any], list[str]]:
    errors: list[str] = []
    try:
        data = path.read_bytes()
        elf = PpuElf(data)
    except (OSError, ElfFormatError) as exc:
        return {"status": "INVALID", "file": path.name, "error": str(exc)}, [f"{label}: {exc}"]
    entry = f"0x{elf.header['entry']:X}"
    program_header_count = elf.header["program_header_count"]
    checks = {
        "entry_point": entry.lower() == expected_entry.lower(),
        "program_header_count": program_header_count == 8,
        "load_segment_present": any(segment.kind == 1 for segment in elf.segments),
    }
    for field, passed in checks.items():
        if not passed:
            errors.append(f"{label}: {field} does not match the installed TU 1.16 SELF profile")
    return {
        "status": "VERIFIED" if not errors else "MISMATCH",
        "file": path.name,
        "size_bytes": len(data),
        "sha256": _sha256(data),
        "entry_point": entry,
        "program_header_count": program_header_count,
        "machine": "PowerPC64",
        "endianness": "big",
        "load_segment_count": sum(segment.kind == 1 for segment in elf.segments),
        "checks": checks,
    }, errors


def verify_inputs(
    disc_root: Path,
    update_root: Path,
    campaign_elf: Path | None = None,
    multiplayer_elf: Path | None = None,
) -> dict[str, Any]:
    errors: list[str] = []
    disc_sfo, found = _inspect_sfo(disc_root, "disc PARAM.SFO", "DG", "01.00")
    errors.extend(found)
    update_sfo, found = _inspect_sfo(update_root, "installed TU PARAM.SFO", "GD", "01.16")
    errors.extend(found)

    self_reports: dict[str, Any] = {}
    for filename, expected in EXPECTED_SELF_FILES.items():
        report, found = _inspect_self_file(update_root, filename, expected)
        self_reports[filename] = report
        errors.extend(found)

    fastfiles = {
        name: _inspect_optional_fastfile(disc_root, name, expected)
        for name, expected in EXPECTED_FASTFILES.items()
    }
    for name, report in fastfiles.items():
        if report["status"] == "MISMATCH":
            errors.append(f"{name}: optional FastFile does not match the previously verified base sample")

    decrypted: dict[str, Any] = {}
    if campaign_elf is not None:
        report, found = _inspect_decrypted_elf(campaign_elf, "0x7E9508", "Campaign decrypted ELF")
        decrypted["campaign"] = report
        errors.extend(found)
    else:
        decrypted["campaign"] = {"status": "NOT_PROVIDED"}
    if multiplayer_elf is not None:
        report, found = _inspect_decrypted_elf(multiplayer_elf, "0xA75E38", "Multiplayer decrypted ELF")
        decrypted["multiplayer"] = report
        errors.extend(found)
    else:
        decrypted["multiplayer"] = {"status": "NOT_PROVIDED"}

    if campaign_elf is not None and multiplayer_elf is not None:
        if (
            decrypted["campaign"].get("status") == "VERIFIED"
            and decrypted["multiplayer"].get("status") == "VERIFIED"
            and decrypted["campaign"].get("sha256") == decrypted["multiplayer"].get("sha256")
        ):
            errors.append("Campaign and Multiplayer decrypted ELF inputs have identical SHA-256; expected distinct builds")

    retail_ok = not errors
    native_ready = (
        retail_ok
        and decrypted["campaign"].get("status") == "VERIFIED"
        and decrypted["multiplayer"].get("status") == "VERIFIED"
    )
    return {
        "schema_version": 1,
        "tool": "verify_ghosts_tu116_inputs",
        "status": "MATCHED_RETAIL_INPUTS" if retail_ok else "MISMATCH_OR_INCOMPLETE",
        "target": {
            "game": "Call of Duty: Ghosts",
            "title_id": TARGET_TITLE_ID,
            "title_update": TARGET_APP_VERSION,
            "platform": "PS3",
            "region": "US",
        },
        "inputs": {
            "disc_param_sfo": disc_sfo,
            "installed_update_param_sfo": update_sfo,
            "installed_update_self_files": self_reports,
            "optional_base_fastfiles": fastfiles,
            "optional_decrypted_ppu_elfs": decrypted,
        },
        "readiness": {
            "retail_input_identity_verified": retail_ok,
            "campaign_decrypted_elf_verified": decrypted["campaign"].get("status") == "VERIFIED",
            "multiplayer_decrypted_elf_verified": decrypted["multiplayer"].get("status") == "VERIFIED",
            "native_ppu_comparison_ready": native_ready,
            "self_segments_are_decrypted_by_this_tool": False,
        },
        "errors": errors,
        "limitations": [
            "This tool only validates local file identity and public metadata; it does not decrypt SELF segments.",
            "A matching SELF hash and ELF header do not reveal PPU instructions while executable segments remain encrypted.",
            "A decrypted ELF is accepted only when it matches the installed TU 1.16 entry point and PPU ELF format; this does not prove runtime capture authenticity.",
            "No retail game binaries are copied into the repository, report, or network.",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--disc-root", required=True, type=Path, help="extracted disc root or PS3_GAME folder")
    parser.add_argument("--update-root", required=True, type=Path, help="installed BLUS31270 update root")
    parser.add_argument("--campaign-elf", type=Path, help="optional locally decrypted TU 1.16 default.self ELF")
    parser.add_argument("--multiplayer-elf", type=Path, help="optional locally decrypted TU 1.16 default_mp.self ELF")
    parser.add_argument("--output", type=Path, help="write the metadata-only JSON manifest to this path")
    args = parser.parse_args(argv)
    try:
        report = verify_inputs(args.disc_root, args.update_root, args.campaign_elf, args.multiplayer_elf)
    except (OSError, ValueError) as exc:
        print(f"input verification failed: {exc}", file=sys.stderr)
        return 2
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
        print(f"wrote metadata-only input report: {args.output}")
    else:
        sys.stdout.write(rendered)
    return 0 if report["status"] == "MATCHED_RETAIL_INPUTS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
