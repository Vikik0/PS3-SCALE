#!/usr/bin/env python3
"""Read-only fingerprint check for a local Ghosts install and RPCS3 workspace."""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Any
from tools.ps3_sfo_inspect import SfoFormatError, parse_sfo

TITLE_ID_RE = re.compile(r"\b[A-Z]{4}\d{5}\b")
PPU_HASH_RE = re.compile(r"\bPPU-[0-9a-fA-F]{40}\b")
PAD_BINDING_RE = re.compile(r"\bPad\s+(?P<port>\d+):\s+device='(?P<device>[^']*)',\s+handler=(?P<handler>[^,]+),\s+VID=0x", re.IGNORECASE)
DEFAULT_TITLE_ID = "BLUS31270"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def locate_game_files(game_root: Path) -> tuple[Path, Path]:
    root = game_root.expanduser().resolve()
    if not root.is_dir():
        raise FileNotFoundError(f"game root is not a directory: {root}")
    for base in (root, root / "PS3_GAME"):
        sfo, eboot = base / "PARAM.SFO", base / "USRDIR" / "EBOOT.BIN"
        if sfo.is_file() and eboot.is_file():
            return sfo, eboot
    raise FileNotFoundError(
        "could not find PARAM.SFO and USRDIR/EBOOT.BIN; pass the disc root, "
        "PS3_GAME folder, or installed dev_hdd0/game/<TITLEID> folder"
    )


def locate_rpcs3(root: Path | None, explicit: Path | None) -> Path | None:
    if explicit is not None:
        candidate = explicit.expanduser().resolve()
        return candidate if candidate.is_file() else None
    if root is None:
        return None
    base = root.expanduser().resolve()
    candidates = (
        base / "rpcs3.exe", base / "RPCS3.exe", base / "rpcs3", base / "RPCS3",
        base / "RPCS3.app" / "Contents" / "MacOS" / "rpcs3",
    )
    return next((p for p in candidates if p.is_file()), None)


def locate_log(root: Path | None, explicit: Path | None) -> Path | None:
    if explicit is not None:
        candidate = explicit.expanduser().resolve()
        return candidate if candidate.is_file() else None
    if root is None:
        return None
    base = root.expanduser().resolve()
    candidates = (
        base / "RPCS3.log", base / "rpcs3.log",
        base / "logs" / "RPCS3.log", base / "logs" / "rpcs3.log",
    )
    return next((p for p in candidates if p.is_file()), None)


def inspect_log(path: Path | None) -> dict[str, Any]:
    if path is None:
        return {
            "status": "WAITING_FOR_FIRST_BOOT", "path": None,
            "ppu_hashes": [], "latest_ppu_hash": None, "title_ids": [],
            "controller_ports": {
                "status": "NO_LOG_EVIDENCE",
                "port0": None,
                "port1": None,
                "distinct_device_labels": None,
            },
        }
    content = path.read_text(encoding="utf-8", errors="replace")
    hashes = [m.group(0).upper() for m in PPU_HASH_RE.finditer(content)]
    bindings: dict[int, dict[str, str]] = {}
    for match in PAD_BINDING_RE.finditer(content):
        port = int(match.group("port"))
        if port in (0, 1):
            bindings[port] = {
                "device": match.group("device").strip(),
                "handler": match.group("handler").strip(),
            }

    def is_bound(binding: dict[str, str] | None) -> bool:
        if not binding:
            return False
        handler = binding["handler"].strip().lower()
        return bool(binding["device"]) and handler not in {"null", "nullpad", "none"}

    port0, port1 = bindings.get(0), bindings.get(1)
    both_bound = is_bound(port0) and is_bound(port1)
    distinct_labels = port0["device"] != port1["device"] if both_bound else None
    return {
        "status": "HASH_FOUND" if hashes else "HASH_NOT_FOUND",
        "path": str(path),
        "sha256": sha256_file(path),
        "ppu_hashes": list(dict.fromkeys(hashes)),
        "latest_ppu_hash": hashes[-1] if hashes else None,
        "title_ids": sorted(set(TITLE_ID_RE.findall(content))),
        "controller_ports": {
            "status": "BOTH_PORTS_HAVE_NON_NULL_BINDINGS" if both_bound else "INCOMPLETE_LOG_EVIDENCE",
            "port0": port0,
            "port1": port1,
            "distinct_device_labels": distinct_labels,
            "note": (
                "RPCS3 pad-thread log evidence only; this does not prove both controllers "
                "produce input or that Ghosts enters split-screen Campaign."
            ),
        },
        "note": "Confirm the surrounding log lines belong to the selected Ghosts boot.",
    }


def inspect_installation(
    game_root: Path,
    expected_title_id: str = DEFAULT_TITLE_ID,
    rpcs3_root: Path | None = None,
    rpcs3_exe: Path | None = None,
    log_path: Path | None = None,
    expected_ppu_hash: str | None = None,
) -> dict[str, Any]:
    expected_title_id = expected_title_id.upper()
    if not re.fullmatch(r"[A-Z]{4}\d{5}", expected_title_id):
        raise ValueError("expected Title ID must look like BLUS31270")

    sfo_path, eboot_path = locate_game_files(game_root)
    sfo = parse_sfo(sfo_path.read_bytes())
    actual_title_id = sfo.get("title_id")
    title_match = actual_title_id == expected_title_id
    with eboot_path.open("rb") as stream:
        magic = stream.read(4)
    container = "PLAIN_ELF" if magic == b"\x7fELF" else (
        "SCE_CONTAINER" if magic == b"SCE\x00" else "UNKNOWN"
    )

    emulator = locate_rpcs3(rpcs3_root, rpcs3_exe)
    log = inspect_log(locate_log(rpcs3_root, log_path))
    expected_hash_match = None
    if expected_ppu_hash:
        normalized = expected_ppu_hash.upper()
        if not re.fullmatch(r"PPU-[0-9A-F]{40}", normalized):
            raise ValueError("expected PPU hash must be PPU- followed by 40 hex digits")
        expected_hash_match = log["latest_ppu_hash"] == normalized

    ready = bool(title_match and emulator is not None)
    warnings = [
        "PARAM.SFO APP_VER is not proof of Title Update 1.16.",
        "EBOOT fingerprinting does not decrypt SELF or prove the installed update.",
        "A PPU hash must be tied to this exact Ghosts boot before patching.",
        "RPCS3 success does not prove CEX 4.93 hardware behavior.",
    ]
    if container == "SCE_CONTAINER":
        warnings.append("EBOOT.BIN is an SCE container; the ELF inspector needs a matching decrypted PPU ELF.")
    if not title_match:
        warnings.append(f"Title ID mismatch: expected {expected_title_id}, found {actual_title_id!r}.")
    if emulator is None:
        warnings.append("RPCS3 executable not found; supply --rpcs3-root or --rpcs3-exe.")
    if expected_hash_match is False:
        warnings.append("Latest logged PPU hash does not match --expected-ppu-hash.")

    return {
        "tool": "ghosts-rpcs3-preflight",
        "schema_version": 1,
        "overall": "READY_FOR_BASELINE_BOOT" if ready else "BLOCKED",
        "read_only": True,
        "selected_profile": {
            "expected_title_id": expected_title_id,
            "actual_title_id": actual_title_id,
            "title_id_match": title_match,
            "title": sfo.get("title"),
            "app_version_metadata_only": sfo.get("app_version"),
            "title_update_1_16_status": "NOT_VERIFIED",
            "firmware_target": "CEX 4.93; hardware validation required",
        },
        "game_files": {
            "param_sfo": {
                "path": str(sfo_path), "size_bytes": sfo_path.stat().st_size,
                "sha256": sha256_file(sfo_path),
            },
            "eboot": {
                "path": str(eboot_path), "size_bytes": eboot_path.stat().st_size,
                "sha256": sha256_file(eboot_path), "container": container,
                "decrypted_ppu_elf_available": container == "PLAIN_ELF",
            },
        },
        "rpcs3": {
            "root": str(rpcs3_root.expanduser().resolve()) if rpcs3_root else None,
            "executable": str(emulator) if emulator else None,
            "found": emulator is not None,
            "log": log,
        },
        "checks": {
            "game_identity": "PASS" if title_match else "FAIL",
            "eboot_present": "PASS",
            "rpcs3_executable": "PASS" if emulator else "WAITING",
            "ppu_hash": log["status"],
            "expected_ppu_hash_match": (
                "NOT_REQUESTED" if expected_hash_match is None
                else "PASS" if expected_hash_match else "FAIL"
            ),
            "title_update": "NOT_VERIFIED",
            "two_controller_ports": log["controller_ports"]["status"],
            "ghosts_prx_loader": "NOT_IMPLEMENTED",
            "campaign_second_player": "NOT_IMPLEMENTED",
        },
        "patch_gate": {
            "ready": False,
            "reason": "Wait for verified TU 1.16 identity, matching decrypted ELF, TOC/imports and safe injection site.",
        },
        "warnings": warnings,
        "next_steps": [
            "Confirm BLUS31270 and that this EBOOT belongs to the installed game.",
            "Boot Ghosts once in RPCS3 with the normal GUI and inspect the matching log entry.",
            "Verify two independent controller ports in an existing offline local mode.",
            "Prove TU 1.16 independently; do not infer it from PARAM.SFO APP_VER.",
            "Only then inspect a matching decrypted PPU ELF and derive a Ghosts-specific loader.",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-root", type=Path, required=True)
    parser.add_argument("--expected-title-id", default=DEFAULT_TITLE_ID)
    parser.add_argument("--rpcs3-root", type=Path)
    parser.add_argument("--rpcs3-exe", type=Path)
    parser.add_argument("--log", type=Path)
    parser.add_argument("--expected-ppu-hash")
    parser.add_argument("--output-json", type=Path)
    args = parser.parse_args(argv)
    try:
        report = inspect_installation(
            args.game_root, args.expected_title_id, args.rpcs3_root,
            args.rpcs3_exe, args.log, args.expected_ppu_hash,
        )
    except (OSError, SfoFormatError, ValueError) as exc:
        print(f"RPCS3 preflight failed: {exc}", file=sys.stderr)
        return 2
    rendered = json.dumps(report, indent=2, ensure_ascii=False) + "\n"
    if args.output_json:
        args.output_json.parent.mkdir(parents=True, exist_ok=True)
        args.output_json.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0 if report["overall"] == "READY_FOR_BASELINE_BOOT" else 2


if __name__ == "__main__":
    raise SystemExit(main())
