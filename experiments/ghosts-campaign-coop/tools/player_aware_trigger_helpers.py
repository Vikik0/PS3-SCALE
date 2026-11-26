#!/usr/bin/env python3
"""Build pinned, player-aware trigger helper variants without modifying retail FastFiles."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

EXPECTED_BLACK_ICE_UTIL_SHA256 = "3de920d152d16b5c70673d322ce2f9d65e22fab5a88ba7a4c7dad3291828c355"
EXPECTED_PLAYER_SCRIPTED_ANIM_UTIL_SHA256 = "40886bdad63803469e7e54bfd565ebe8fea61aa8643d2a4ec9d4be385bb76b0e"


class PatchError(ValueError):
    """Raised when the supplied decompiled GSC does not match the pinned shape."""


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def verify_pinned_source(data: bytes, expected_sha256: str, label: str) -> str:
    actual = sha256_bytes(data)
    if actual != expected_sha256:
        raise PatchError(f"{label} source SHA-256 mismatch: expected {expected_sha256}, got {actual}")
    return actual


def _function_span(source: str, name: str) -> tuple[int, int]:
    """Find one top-level function definition, ignoring indented call sites."""
    matches = list(re.finditer(r"^" + re.escape(name) + r"\s*\(", source, re.MULTILINE))
    if len(matches) != 1:
        raise PatchError(f"expected exactly one top-level definition of {name}, found {len(matches)}")

    start = matches[0].start()
    open_brace = source.find("{", matches[0].end())
    if open_brace < 0:
        raise PatchError(f"missing opening brace for {name}")
    signature = source[matches[0].start():open_brace]
    if ";" in signature:
        raise PatchError(f"invalid function signature for {name}")

    depth = 0
    in_string = False
    escaped = False
    in_line_comment = False
    in_block_comment = False
    i = open_brace
    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""
        if in_line_comment:
            if ch == "\n":
                in_line_comment = False
        elif in_block_comment:
            if ch == "*" and nxt == "/":
                in_block_comment = False
                i += 1
        elif in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                in_string = False
        elif ch == "/" and nxt == "/":
            in_line_comment = True
            i += 1
        elif ch == "/" and nxt == "*":
            in_block_comment = True
            i += 1
        elif ch == '"':
            in_string = True
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return start, i + 1
        i += 1
    raise PatchError(f"unterminated function body: {name}")


def extract_function(source: str, name: str) -> str:
    start, end = _function_span(source, name)
    return source[start:end]


def replace_function(source: str, name: str, replacement: str) -> str:
    start, end = _function_span(source, name)
    return source[:start] + replacement + source[end:]


def patch_black_ice_source(source: str) -> str:
    if re.search(r"^waittill_trigger_for_player\s*\(", source, re.MULTILINE):
        raise PatchError("Black Ice player-aware helper already exists")
    original = extract_function(source, "waittill_trigger")
    expected_signature = "waittill_trigger( var_0, var_1, var_2, var_3, var_4, var_5, var_6 )"
    if not original.splitlines()[0].strip() == expected_signature:
        raise PatchError("Black Ice waittill_trigger signature does not match pinned baseline")
    if "level.player" not in original:
        raise PatchError("Black Ice waittill_trigger has no primary-player references to adapt")

    helper = original.replace(
        "waittill_trigger(",
        "waittill_trigger_for_player( player, ",
        1,
    ).replace("level.player", "player")
    params = "var_0, var_1, var_2, var_3, var_4, var_5, var_6"
    wrapper = (
        f"waittill_trigger( {params} )\n"
        "{\n"
        f"    waittill_trigger_for_player( level.player, {params} );\n"
        "}"
    )
    result = replace_function(source, "waittill_trigger", wrapper + "\n\n" + helper)
    if "level.player" in extract_function(result, "waittill_trigger_for_player"):
        raise PatchError("Black Ice player-aware helper still references level.player")
    return result


def patch_player_scripted_anim_source(source: str) -> str:
    if re.search(r"^waittill_trigger_for_player\s*\(", source, re.MULTILINE):
        raise PatchError("player-scripted-animation player-aware helper already exists")
    old_wait = extract_function(source, "waittill_trigger")
    old_trigger = extract_function(source, "_trigger_handle_triggering")
    expected_signature = "waittill_trigger( var_0, var_1, var_2, var_3, var_4, var_5 )"
    if old_wait.splitlines()[0].strip() != expected_signature:
        raise PatchError("player_scripted_anim_util waittill_trigger signature mismatch")
    if old_trigger.splitlines()[0].strip() != "_trigger_handle_triggering( var_0 )":
        raise PatchError("player_scripted_anim_util trigger callback signature mismatch")
    if "level.player" not in old_wait or "level.player" not in old_trigger:
        raise PatchError("player_scripted_anim_util baseline lacks expected primary-player references")

    helper = old_wait.replace(
        "waittill_trigger(",
        "waittill_trigger_for_player( player, ",
        1,
    ).replace("level.player", "player")
    old_call = "var_0 thread _trigger_handle_triggering( var_7 );"
    if helper.count(old_call) != 1:
        raise PatchError("expected one trigger callback thread call in waittill_trigger")
    helper = helper.replace(
        old_call,
        "var_0 thread _trigger_handle_triggering( player, var_7 );",
        1,
    )
    trigger_helper = old_trigger.replace(
        "_trigger_handle_triggering( var_0 )",
        "_trigger_handle_triggering( player, var_0 )",
        1,
    ).replace("level.player", "player")

    params = "var_0, var_1, var_2, var_3, var_4, var_5"
    wrapper = (
        f"waittill_trigger( {params} )\n"
        "{\n"
        f"    waittill_trigger_for_player( level.player, {params} );\n"
        "}"
    )
    result = replace_function(source, "waittill_trigger", wrapper + "\n\n" + helper)
    result = replace_function(result, "_trigger_handle_triggering", trigger_helper)
    for name in ("waittill_trigger_for_player", "_trigger_handle_triggering"):
        if "level.player" in extract_function(result, name):
            raise PatchError(f"{name} still references level.player")
    if "var_0 thread _trigger_handle_triggering( player, var_7 );" not in extract_function(
        result, "waittill_trigger_for_player"
    ):
        raise PatchError("player argument was not propagated to trigger callback")
    return result


def verify_shared_source_pair(carrier_data: bytes, cornered_data: bytes) -> None:
    verify_shared_source_pair(carrier_data, cornered_data)


def build_bundle(
    black_ice_util_path: Path,
    carrier_player_scripted_anim_path: Path,
    cornered_player_scripted_anim_path: Path,
) -> tuple[dict[str, bytes], dict]:
    black_data = Path(black_ice_util_path).read_bytes()
    carrier_data = Path(carrier_player_scripted_anim_path).read_bytes()
    cornered_data = Path(cornered_player_scripted_anim_path).read_bytes()

    black_hash = verify_pinned_source(black_data, EXPECTED_BLACK_ICE_UTIL_SHA256, "Black Ice utility")
    carrier_hash = verify_pinned_source(
        carrier_data, EXPECTED_PLAYER_SCRIPTED_ANIM_UTIL_SHA256, "Carrier player-scripted-animation utility"
    )
    cornered_hash = verify_pinned_source(
        cornered_data, EXPECTED_PLAYER_SCRIPTED_ANIM_UTIL_SHA256, "Cornered player-scripted-animation utility"
    )
    if carrier_data != cornered_data:
        raise PatchError("Carrier and Cornered shared player_scripted_anim_util sources differ")

    black_text = black_data.decode("utf-8")
    shared_text = carrier_data.decode("utf-8")
    patched_black = patch_black_ice_source(black_text).encode("utf-8")
    patched_shared = patch_player_scripted_anim_source(shared_text).encode("utf-8")

    outputs = {
        "black_ice/black_ice_util.player_aware.gsc": patched_black,
        "carrier/player_scripted_anim_util.player_aware.gsc": patched_shared,
        "cornered/player_scripted_anim_util.player_aware.gsc": patched_shared,
    }
    manifest = {
        "schema_version": 1,
        "target": {"engine": "IW6", "platform": "PS3", "game_title_id": "BLUS31270", "tu": "01.16"},
        "scope": "experimental player-aware trigger helpers only; mission call sites are not migrated; not installed",
        "inputs": {
            "black_ice_util": {"sha256": black_hash, "bytes": len(black_data)},
            "carrier_player_scripted_anim_util": {"sha256": carrier_hash, "bytes": len(carrier_data)},
            "cornered_player_scripted_anim_util": {"sha256": cornered_hash, "bytes": len(cornered_data)},
        },
        "outputs": {
            name: {"sha256": sha256_bytes(data), "bytes": len(data)}
            for name, data in outputs.items()
        },
        "behavior": [
            "legacy waittill_trigger wrappers continue to use level.player",
            "new waittill_trigger_for_player helpers use the explicit player argument",
            "player_scripted_anim_util propagates the same player into _trigger_handle_triggering",
            "no original mission handler or signed FastFile is modified",
        ],
    }
    return outputs, manifest


def write_bundle(outputs: dict[str, bytes], manifest: dict, output_dir: Path) -> Path:
    root = Path(output_dir).resolve()
    root.mkdir(parents=True, exist_ok=True)
    manifest["outputs"] = dict(manifest["outputs"])
    for relative, data in outputs.items():
        destination = (root / relative).resolve()
        destination.relative_to(root)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    manifest_path = root / "MANIFEST.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest_path


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--black-ice-util", type=Path, required=True)
    parser.add_argument("--carrier-player-scripted-anim-util", type=Path, required=True)
    parser.add_argument("--cornered-player-scripted-anim-util", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        outputs, manifest = build_bundle(
            args.black_ice_util,
            args.carrier_player_scripted_anim_util,
            args.cornered_player_scripted_anim_util,
        )
        path = write_bundle(outputs, manifest, args.output_dir)
        print(path.read_text(encoding="utf-8"), end="")
        return 0
    except (OSError, UnicodeDecodeError, PatchError, ValueError) as exc:
        parser.exit(2, f"error: {exc}\n")


if __name__ == "__main__":
    raise SystemExit(main())
