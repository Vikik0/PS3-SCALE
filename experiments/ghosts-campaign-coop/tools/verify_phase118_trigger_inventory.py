#!/usr/bin/env python3
"""Validate the pinned Phase 118 Ghosts Campaign trigger call-site inventory."""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

EXPECTED_ARCHIVE = "Ghosts_Campaign_Missions_Phase31.zip"
EXPECTED_ARCHIVE_BYTES = 1_530_334
EXPECTED_ARCHIVE_SHA256 = "ca4c0fb46a12b6179b406bc6f7ee5e10729c66ac7a7d2664bf9c3d3cfa14a77a"
EXPECTED_COUNTS = {"Black Ice": 3, "Carrier": 2, "Cornered": 10}
EXPECTED_SOURCE_HASHES = {
    "maps/black_ice_ascend.gsc": "7204bb1ec7c217f95a157765642c2953c0aa5a892d4b17f50cdd7a7367f945c0",
    "maps/black_ice_command.gsc": "fd224c0d5fd7417dcfeaed5c761d353d47afb244584e964e02c5d717fc47b6f3",
    "maps/black_ice_flarestack.gsc": "87257aab8fa64715abc08636768a1c52a93ffc2a1d4c8e36b0a6e4878df8c11f",
    "maps/carrier_deck_transition.gsc": "71f4e9dbb35ae33290d44195b0c2b4fec6307cb488068e0a3459ba49f204ae07",
    "maps/carrier_defend_sparrow.gsc": "851374e3d50cf27549922afaf8a03b569af6d4b975a84d3c5443a3580611f27f",
    "maps/cornered_building_entry.gsc": "723471a1f7aa700b982ce5b7225decd2d2e274bd88206352c083dc119bab8214",
    "maps/cornered_destruct.gsc": "11619ed70e83f89ce4d2d3e61f73dc4f87a0557827e68dc3acc6ec5c6488bcb3",
    "maps/cornered_interior.gsc": "79eae3c39c2e2502428c902cd30ed5686d5c87f4dfc028a3e48f386a8870fa76",
    "maps/cornered_intro.gsc": "a5bb759c6fa663bb24841793077a9322a7b5374f38f51ead3f42b21cac66d9d8",
    "maps/cornered_rappel.gsc": "83ac9beae5fa4914eab16e15ac32af6f448a0f0f87816dd15dd39e131f7d00fe",
    "maps/cornered_zipline_turret.gsc": "b92e8b9c363145eb6b140e620fe47b736627bab2148806b1d9061e524bec879b",
}
EXPECTED_CALLSITES = {
    ("Black Ice", "maps/black_ice_ascend.gsc", 293, "ascend_logic", r"maps\black_ice_util::waittill_trigger"),
    ("Black Ice", "maps/black_ice_command.gsc", 66, "magicreloadwhenreachenemy", r"maps\black_ice_util::waittill_trigger"),
    ("Black Ice", "maps/black_ice_flarestack.gsc", 268, "event_player_turns_key", r"maps\black_ice_util::waittill_trigger"),
    ("Carrier", "maps/carrier_deck_transition.gsc", 432, "osprey_control", r"maps\player_scripted_anim_util::waittill_trigger"),
    ("Carrier", "maps/carrier_defend_sparrow.gsc", 139, "run_deck_transition", r"maps\player_scripted_anim_util::waittill_trigger"),
    ("Cornered", "maps/cornered_building_entry.gsc", 346, "handle_building_entry", r"maps\player_scripted_anim_util::waittill_trigger"),
    ("Cornered", "maps/cornered_building_entry.gsc", 671, "upload_virus_loop_anims", r"maps\player_scripted_anim_util::waittill_trigger"),
    ("Cornered", "maps/cornered_building_entry.gsc", 766, "upload_virus_anims", r"maps\player_scripted_anim_util::waittill_trigger"),
    ("Cornered", "maps/cornered_building_entry.gsc", 783, "upload_virus_anims", r"maps\player_scripted_anim_util::waittill_trigger"),
    ("Cornered", "maps/cornered_building_entry.gsc", 2757, "handle_rappel_inverted", r"maps\player_scripted_anim_util::waittill_trigger"),
    ("Cornered", "maps/cornered_destruct.gsc", 794, "hvt_office_anim_struct", r"maps\player_scripted_anim_util::waittill_trigger"),
    ("Cornered", "maps/cornered_interior.gsc", 2398, "junction_elevator_control_doors_open", r"maps\player_scripted_anim_util::waittill_trigger"),
    ("Cornered", "maps/cornered_intro.gsc", 1896, "zipline_mvmt_npc", r"maps\player_scripted_anim_util::waittill_trigger"),
    ("Cornered", "maps/cornered_rappel.gsc", 131, "player_combat_rappel", r"maps\player_scripted_anim_util::waittill_trigger"),
    ("Cornered", "maps/cornered_zipline_turret.gsc", 20, "player_handle_speed_for_knockdown", r"maps\player_scripted_anim_util::waittill_trigger"),
}
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
    if archive.get("bytes") != EXPECTED_ARCHIVE_BYTES:
        errors.append("source archive byte size does not match the pinned reference")
    archive_sha = archive.get("sha256")
    if not _valid_sha256(archive_sha):
        errors.append("source archive SHA-256 must be 64 lowercase hex characters")
    elif archive_sha != EXPECTED_ARCHIVE_SHA256:
        errors.append("source archive SHA-256 does not match the pinned reference")

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
    observed_sites: set[tuple] = set()
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
            expected_file_sha = EXPECTED_SOURCE_HASHES.get(filename)
            if expected_file_sha is None:
                errors.append(f"{label} references a source file outside the pinned inventory")
            elif source_sha != expected_file_sha:
                errors.append(f"{label} source file SHA-256 does not match the pinned source")
        key = (mission, filename, line, caller)
        if key in seen_sites:
            errors.append(f"{label} duplicates an existing call site")
        seen_sites.add(key)
        observed_sites.add((mission, filename, line, caller, item.get("helper")))

    if observed_sites != EXPECTED_CALLSITES:
        errors.append("call-site identities do not match the pinned source inventory")
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
