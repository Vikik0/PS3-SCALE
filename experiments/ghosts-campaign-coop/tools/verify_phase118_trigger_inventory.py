#!/usr/bin/env python3
"""Validate the pinned Phase 118 Ghosts Campaign trigger call-site inventory."""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

EXPECTED_ARCHIVE = "Ghosts_Campaign_Missions_Phase31.zip"
EXPECTED_COUNTS = {"Black Ice": 3, "Carrier": 2, "Cornered": 10}
EXPECTED_INTERACTIONS = (
    ("Black Ice", "event_player_turns_key", 0),
    ("Carrier", "cut_the_rope", 1),
    ("Cornered", "gun_down_trigger_internal", 2),
    ("Cornered", "upload_virus_enter", 3),
)
EXPECTED_INTERACTION_LABELS = [
    "Black Ice:event_player_turns_key",
    "Carrier:cut_the_rope",
    "Cornered:gun_down_trigger_internal",
    "Cornered:upload_virus_enter",
]
EXPECTED_HELPERS = {
    "Black Ice": r"maps\black_ice_util::waittill_trigger",
    "Carrier": r"maps\player_scripted_anim_util::waittill_trigger",
    "Cornered": r"maps\player_scripted_anim_util::waittill_trigger",
}
EXPECTED_STATUS = "STATIC_AUDIT_COMPLETE; MISSION_MIGRATION_BLOCKED_PENDING_PHYSICAL_PS3_RUNTIME_EVIDENCE"


def _valid_sha256(value: object) -> bool:
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{64}", value) is not None


def _valid_int(value: object, minimum: int = 0) -> bool:
    return isinstance(value, int) and not isinstance(value, bool) and value >= minimum


def validate_inventory(inventory: dict, validation: dict, report_text: str | None = None) -> list[str]:
    """Return semantic consistency errors for the Phase 118 inventory and gate."""
    errors: list[str] = []
    if not isinstance(inventory, dict):
        return ["inventory must be a JSON object"]
    if not isinstance(validation, dict):
        return ["validation summary must be a JSON object"]

    if inventory.get("schema_version") != 1 or inventory.get("phase") != 118:
        errors.append("inventory schema_version/phase must be 1/118")
    if validation.get("schema_version") != 1 or validation.get("phase") != 118:
        errors.append("validation schema_version/phase must be 1/118")

    archive = inventory.get("source_archive")
    if not isinstance(archive, dict):
        errors.append("source_archive must be an object")
        archive = {}
    if archive.get("filename") != EXPECTED_ARCHIVE:
        errors.append("source archive filename must remain Ghosts_Campaign_Missions_Phase31.zip")
    if not _valid_int(archive.get("bytes"), 1):
        errors.append("source archive byte size must be a positive integer")
    archive_sha = archive.get("sha256")
    if not _valid_sha256(archive_sha):
        errors.append("source archive SHA-256 must be 64 lowercase hex characters")

    summary = inventory.get("summary")
    if not isinstance(summary, dict):
        errors.append("inventory summary must be an object")
        summary = {}
    for mission, expected in EXPECTED_COUNTS.items():
        if summary.get(mission.lower().replace(" ", "_")) != expected:
            errors.append(f"inventory summary count mismatch for {mission}")
    if summary.get("total") != 15:
        errors.append("inventory summary total must be 15")
    if summary.get("migrated") != 0:
        errors.append("inventory must keep all 15 original call sites unmigrated")

    callsites = inventory.get("callsites")
    if not isinstance(callsites, list) or len(callsites) != 15:
        errors.append("inventory must contain exactly 15 original call sites")
        callsites = []
    observed_counts = {mission: 0 for mission in EXPECTED_COUNTS}
    seen_sites: set[tuple] = set()
    file_hashes: dict[str, str] = {}
    for index, item in enumerate(callsites):
        label = f"callsite[{index}]"
        if not isinstance(item, dict):
            errors.append(f"{label} must be an object")
            continue
        mission = item.get("mission")
        if mission not in EXPECTED_COUNTS:
            errors.append(f"{label} has an unknown mission")
        else:
            observed_counts[mission] += 1
            if item.get("helper") != EXPECTED_HELPERS[mission]:
                errors.append(f"{label} has an unexpected legacy helper for {mission}")
        filename = item.get("file")
        if not isinstance(filename, str) or not filename.startswith("maps/") or not filename.endswith(".gsc"):
            errors.append(f"{label} must identify a maps/*.gsc source file")
        line = item.get("line")
        if not _valid_int(line, 1):
            errors.append(f"{label} line must be a positive integer")
        caller = item.get("caller")
        if not isinstance(caller, str) or not caller.strip():
            errors.append(f"{label} caller must be a non-empty function name")
        source_sha = item.get("source_sha256")
        if not _valid_sha256(source_sha):
            errors.append(f"{label} source SHA-256 is invalid")
        elif isinstance(filename, str):
            previous = file_hashes.setdefault(filename, source_sha)
            if previous != source_sha:
                errors.append(f"{label} source hash conflicts with another call site in {filename}")
        key = (mission, filename, line, caller)
        if key in seen_sites:
            errors.append(f"{label} duplicates an existing call site")
        seen_sites.add(key)

    if observed_counts != EXPECTED_COUNTS:
        errors.append("call-site mission distribution must be 3 Black Ice / 2 Carrier / 10 Cornered")

    interactions = inventory.get("critical_shared_interactions")
    if not isinstance(interactions, list) or len(interactions) != 4:
        errors.append("inventory must contain exactly four critical shared interactions")
        interactions = []
    observed_interactions = []
    for index, item in enumerate(interactions):
        if not isinstance(item, dict):
            errors.append(f"critical interaction[{index}] must be an object")
            continue
        observed_interactions.append((item.get("mission"), item.get("routine"), item.get("event_lock_id")))
        if item.get("status") != "BLOCKED_ACTIVATOR_LINEAGE_MISSING":
            errors.append("all four critical interactions must remain blocked pending activator lineage")
    if tuple(observed_interactions) != EXPECTED_INTERACTIONS:
        errors.append("critical interaction names and Event Lock IDs must remain the four pinned 0..3 interactions")
    carrier = next((x for x in interactions if isinstance(x, dict) and x.get("routine") == "cut_the_rope"), {})
    if carrier.get("event_shape") != "notifyonplayercommand":
        errors.append("Carrier cut_the_rope must remain classified as notifyonplayercommand")

    adapters = inventory.get("adapters")
    if not isinstance(adapters, dict) or adapters.get("entity_trigger") != "ghc_campaign_wait_for_owned_trigger" or adapters.get("player_command") != "ghc_campaign_wait_for_owned_player_command":
        errors.append("inventory must pin both entity-trigger and player-command adapters")
    decision = inventory.get("decision", "")
    if not isinstance(decision, str) or "do not mass-replace legacy helper calls" not in decision.lower():
        errors.append("inventory decision must prohibit mass replacement of legacy mission helpers")

    boundary = inventory.get("runtime_boundary")
    if not isinstance(boundary, dict):
        errors.append("inventory runtime_boundary must be an object")
        boundary = {}
    if boundary.get("physical_ps3_tested") is not False:
        errors.append("inventory must not claim a physical PS3 test")
    if boundary.get("native_campaign_player_two_proven") is not False:
        errors.append("inventory must not claim native Campaign Player 2 is proven")
    if boundary.get("shared_interactions_ready") != "0/4":
        errors.append("inventory must keep shared interactions at 0/4")
    if boundary.get("retail_fastfiles_modified") is not False:
        errors.append("inventory must not claim retail FastFiles were modified")

    if validation.get("source_archive_sha256") != archive_sha:
        errors.append("validation source archive SHA-256 does not match the inventory")
    validation_counts = validation.get("counts")
    if not isinstance(validation_counts, dict):
        errors.append("validation counts must be an object")
        validation_counts = {}
    for key, expected in (("black_ice", 3), ("carrier", 2), ("cornered", 10), ("total", 15), ("migrated", 0)):
        if validation_counts.get(key) != expected:
            errors.append(f"validation count mismatch for {key}")
    if validation.get("four_shared_interactions") != EXPECTED_INTERACTION_LABELS:
        errors.append("validation summary must list the four pinned shared interactions in order")
    if validation.get("status") != EXPECTED_STATUS:
        errors.append("validation status must preserve the physical-PS3 migration gate")

    validation_boundary = validation.get("runtime_boundary")
    if not isinstance(validation_boundary, dict):
        errors.append("validation runtime_boundary must be an object")
        validation_boundary = {}
    if validation_boundary.get("physical_ps3_tested") is not False:
        errors.append("validation must not claim a physical PS3 test")
    if validation_boundary.get("player_two") != "NOT_PROVEN":
        errors.append("validation must keep Player 2 as NOT_PROVEN")
    if validation_boundary.get("shared_interactions_ready") != "0/4":
        errors.append("validation must keep shared interactions at 0/4")
    if validation_boundary.get("retail_fastfiles_modified") is not False:
        errors.append("validation must not claim retail FastFiles were modified")

    if report_text is not None:
        if "NOT_PROVEN" not in report_text or "0/4" not in report_text:
            errors.append("Arabic phase report must disclose Player 2 NOT_PROVEN and 0/4 interactions")
        if "لا يوجد تعديل على EBOOT" not in report_text:
            errors.append("Arabic phase report must disclose that no retail executable/FastFile was modified")
    return errors


def validate_files(inventory_path: Path, validation_path: Path, report_path: Path | None = None) -> list[str]:
    try:
        inventory = json.loads(Path(inventory_path).read_text(encoding="utf-8"))
        validation = json.loads(Path(validation_path).read_text(encoding="utf-8"))
        report_text = Path(report_path).read_text(encoding="utf-8") if report_path else None
    except (OSError, json.JSONDecodeError, UnicodeDecodeError) as exc:
        return [f"unable to read Phase 118 inventory evidence: {exc}"]
    return validate_inventory(inventory, validation, report_text)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--validation", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args(argv)
    errors = validate_files(args.inventory, args.validation, args.report)
    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1
    print("PASS: Phase 118 trigger inventory is internally consistent")
    print("call sites: 15 (Black Ice 3 / Carrier 2 / Cornered 10)")
    print("critical shared interactions: 4 blocked; Player 2 remains NOT_PROVEN")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
