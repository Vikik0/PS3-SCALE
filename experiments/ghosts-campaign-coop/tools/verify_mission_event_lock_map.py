#!/usr/bin/env python3
"""Validate the planned mission-to-event-lock ID map against the Local Session GSC API."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

EXPECTED_POLICIES = {
    ("Black Ice", "player_fail_finale_instakill"): "MISSION_COORDINATOR_ONLY",
    ("Black Ice", "ascend_logic"): "MISSION_COORDINATOR_WITH_PLAYER_INPUT_HANDLERS",
    ("Black Ice", "event_player_turns_key"): "SHARED_INTERACTION_SINGLE_OWNER",
    ("Carrier", "knockback_hesh"): "MISSION_COORDINATOR_ONLY",
    ("Carrier", "cut_the_rope"): "SHARED_INTERACTION_SINGLE_OWNER",
    ("Carrier", "victory_choppers_land"): "MISSION_COORDINATOR_ONLY",
    ("Cornered", "gun_down_trigger_internal"): "SHARED_INTERACTION_SINGLE_OWNER",
    ("Cornered", "fall_player"): "MISSION_COORDINATOR_ONLY",
    ("Cornered", "upload_virus_enter"): "SHARED_INTERACTION_SINGLE_OWNER",
}
EXPECTED_LOCK_IDS = {
    ("Black Ice", "event_player_turns_key"): 0,
    ("Carrier", "cut_the_rope"): 1,
    ("Cornered", "gun_down_trigger_internal"): 2,
    ("Cornered", "upload_virus_enter"): 3,
}
REQUIRED_API = (
    "ghc_campaign_event_lock_acquire",
    "ghc_campaign_event_lock_release",
    "ghc_campaign_event_lock_abort_player",
    "ghc_campaign_event_lock_owner",
    "ghc_campaign_event_lock_reset",
)


def validate_map(data: dict, gsc_source: str) -> list[str]:
    errors: list[str] = []
    limits = data.get("event_id_range", {})
    minimum, maximum = limits.get("min"), limits.get("max")
    if (minimum, maximum) != (0, 15):
        errors.append("event_id_range must remain 0..15")
    if data.get("integration_status") != "PLANNED_NOT_WIRED_TO_ORIGINAL_MISSION_HANDLERS":
        errors.append("integration_status must state that original mission handlers are not wired")
    if data.get("mission_handlers_modified") is not False:
        errors.append("mission_handlers_modified must remain false until runtime gate is passed")

    events = data.get("events")
    if not isinstance(events, list):
        return errors + ["events must be a list"]

    observed: dict[tuple[str, str], dict] = {}
    used_ids: dict[int, tuple[str, str]] = {}
    for index, event in enumerate(events):
        key = (event.get("mission"), event.get("routine"))
        if key in observed:
            errors.append(f"duplicate mission routine at index {index}: {key}")
            continue
        observed[key] = event

        expected_policy = EXPECTED_POLICIES.get(key)
        if expected_policy is None:
            errors.append(f"unexpected mission routine: {key}")
        elif event.get("policy") != expected_policy:
            errors.append(f"{key}: expected policy {expected_policy}")

        event_id = event.get("event_lock_id")
        if event.get("policy") == "SHARED_INTERACTION_SINGLE_OWNER":
            if not isinstance(event_id, int) or isinstance(event_id, bool):
                errors.append(f"{key}: shared interaction requires an integer event_lock_id")
            elif event_id < minimum or event_id > maximum:
                errors.append(f"{key}: event_lock_id outside configured range")
            elif event_id in used_ids:
                errors.append(f"event_lock_id {event_id} reused by {used_ids[event_id]} and {key}")
            else:
                used_ids[event_id] = key
            if event.get("owner") != "first_valid_player_to_acquire":
                errors.append(f"{key}: shared interaction must use first_valid_player_to_acquire")
        else:
            if event_id is not None:
                errors.append(f"{key}: mission coordinator policy must not claim a player event lock")

        if not isinstance(event.get("shared_state"), list) or not event["shared_state"]:
            errors.append(f"{key}: shared_state must be a non-empty list")
        if not isinstance(event.get("per_player_state"), list) or not event["per_player_state"]:
            errors.append(f"{key}: per_player_state must be a non-empty list")
        if not event.get("reason"):
            errors.append(f"{key}: missing ownership rationale")

    missing = set(EXPECTED_POLICIES) - set(observed)
    extra = set(observed) - set(EXPECTED_POLICIES)
    if missing:
        errors.append(f"missing mission routines: {sorted(missing)}")
    if extra:
        errors.append(f"unexpected mission routines: {sorted(extra)}")

    for key, expected_id in EXPECTED_LOCK_IDS.items():
        event = observed.get(key)
        if event and event.get("event_lock_id") != expected_id:
            errors.append(f"{key}: event ID must remain {expected_id}")

    for function in REQUIRED_API:
        if function not in gsc_source:
            errors.append(f"Local Session GSC API missing {function}")

    return errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--map",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "reports" / "PHASE88_MISSION_EVENT_LOCK_MAP.json",
    )
    parser.add_argument(
        "--gsc",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "client" / "ps3" / "gsc" / "ghc_campaign_local_session.gsc",
    )
    args = parser.parse_args(argv)
    try:
        data = json.loads(args.map.read_text(encoding="utf-8"))
        gsc_source = args.gsc.read_text(encoding="utf-8")
    except (OSError, json.JSONDecodeError) as exc:
        print(f"FAIL: unable to load event map or GSC source: {exc}")
        return 1

    errors = validate_map(data, gsc_source)
    if errors:
        for error in errors:
            print(f"FAIL: {error}")
        return 1

    print("PASS: all nine mission event ownership decisions and stable lock IDs validated")
    print(f"mission routines: {len(data['events'])}")
    print(f"shared interaction lock IDs: {len(EXPECTED_LOCK_IDS)}")
    print("mission handler integration: not wired (hardware/native-client gate remains open)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
