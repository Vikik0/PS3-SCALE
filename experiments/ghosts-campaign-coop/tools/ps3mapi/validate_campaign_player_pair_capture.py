#!/usr/bin/env python3
"""Validate operator-supplied native Campaign player-pair runtime observations.

This tool validates evidence; it does not collect PS3 memory or infer missing
addresses. A PASS requires a stable pair with distinct native clients, input
ports, player structures, camera/HUD ownership, and Campaign aliases.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any

EXPECTED_TITLE_ID = "BLUS31270"
EXPECTED_TITLE_UPDATE = "01.16"
DEFAULT_MAX_SAMPLE_GAP_MS = 1000
ADDRESS_RE = re.compile(r"^0x[0-9a-fA-F]{8,16}$")
REQUIRED_PLAYER_FIELDS = (
    "entity_id",
    "local_client_index",
    "controller_port",
    "is_splitscreen_player",
    "is_primary",
    "gentity_address",
    "gclient_address",
    "player_state_address",
    "camera_state_address",
    "hud_state_address",
    "camera_owner_entity_id",
    "hud_owner_entity_id",
)


def _address(value: Any) -> bool:
    return (
        isinstance(value, str)
        and ADDRESS_RE.fullmatch(value) is not None
        and int(value, 16) != 0
    )


def _is_explicit_pair_loss(observation: dict[str, Any]) -> bool:
    """Return true only for an observed split-screen loss or sub-two-player array.

    A malformed pointer, alias mismatch, or other structural validation error
    is not evidence that the native pair disconnected and must not be used to
    claim a reconnect/rebind cycle.
    """
    if observation.get("split_screen") is False:
        return True
    players = observation.get("players")
    count = observation.get("player_array_count")
    return (
        type(count) is int
        and count < 2
        and isinstance(players, list)
        and len(players) < 2
    )


def _valid_observation(observation: dict[str, Any]) -> tuple[bool, str, tuple | None]:
    split_screen = observation["split_screen"]
    players = observation["players"]
    player_array_count = observation["player_array_count"]

    if split_screen is not True:
        return False, "split_screen_not_active", None
    if player_array_count != len(players):
        return False, "player_array_count_mismatch", None
    if player_array_count != 2 or len(players) != 2:
        return False, "native_player_count_not_two", None

    for player in players:
        if not isinstance(player, dict):
            return False, "player_record_not_object", None
        missing = [field for field in REQUIRED_PLAYER_FIELDS if field not in player]
        if missing:
            return False, "missing_player_fields:" + ",".join(missing), None
        if not isinstance(player["entity_id"], str) or not player["entity_id"].strip():
            return False, "invalid_entity_id", None
        if type(player["local_client_index"]) is not int or player["local_client_index"] not in (0, 1):
            return False, "invalid_local_client_index", None
        if type(player["controller_port"]) is not int or player["controller_port"] not in (0, 1):
            return False, "invalid_controller_port", None
        if type(player["is_splitscreen_player"]) is not bool or type(player["is_primary"]) is not bool:
            return False, "non_boolean_player_flags", None
        if player["is_splitscreen_player"] is not True:
            return False, "player_not_native_split_screen", None
        for field in (
            "gentity_address",
            "gclient_address",
            "player_state_address",
            "camera_state_address",
            "hud_state_address",
        ):
            if not _address(player[field]):
                return False, "invalid_" + field, None
        if player["camera_owner_entity_id"] != player["entity_id"]:
            return False, "camera_owner_mismatch", None
        if player["hud_owner_entity_id"] != player["entity_id"]:
            return False, "hud_owner_mismatch", None

    if {p["local_client_index"] for p in players} != {0, 1}:
        return False, "local_client_indices_not_distinct", None
    if {p["controller_port"] for p in players} != {0, 1}:
        return False, "controller_ports_not_distinct", None
    if {p["is_primary"] for p in players} != {False, True}:
        return False, "primary_secondary_roles_invalid", None

    for field in (
        "entity_id",
        "gentity_address",
        "gclient_address",
        "player_state_address",
        "camera_state_address",
        "hud_state_address",
    ):
        if len({p[field] for p in players}) != 2:
            return False, field + "_not_distinct", None

    primary = next(p for p in players if p["is_primary"])
    secondary = next(p for p in players if not p["is_primary"])
    if observation.get("level_player_entity_id") != primary["entity_id"]:
        return False, "level_player_alias_mismatch", None
    if observation.get("playengineeffects_entity_id") != secondary["entity_id"]:
        return False, "playengineeffects_alias_mismatch", None

    signature = tuple(
        (
            p["local_client_index"],
            p["controller_port"],
            p["entity_id"],
            p["gentity_address"].lower(),
            p["gclient_address"].lower(),
            p["player_state_address"].lower(),
            p["camera_state_address"].lower(),
            p["hud_state_address"].lower(),
            p["is_primary"],
        )
        for p in sorted(players, key=lambda item: item["local_client_index"])
    )
    return True, "valid_native_pair", signature


def validate_capture(
    data: dict[str, Any],
    required_stable_samples: int = 5,
    max_sample_gap_ms: int = DEFAULT_MAX_SAMPLE_GAP_MS,
) -> dict[str, Any]:
    if not isinstance(data, dict):
        raise ValueError("capture root must be a JSON object")
    if data.get("schema_version") != 1:
        raise ValueError("schema_version must be 1")
    if data.get("title_id") != EXPECTED_TITLE_ID:
        raise ValueError(f"title_id must be {EXPECTED_TITLE_ID}")
    if data.get("title_update") != EXPECTED_TITLE_UPDATE:
        raise ValueError(f"title_update must be {EXPECTED_TITLE_UPDATE}")
    if data.get("mode") != "CAMPAIGN":
        raise ValueError("mode must be CAMPAIGN")
    if not isinstance(data.get("capture_id"), str) or not data["capture_id"].strip():
        raise ValueError("capture_id is required")
    if not isinstance(data.get("evidence_source"), str) or not data["evidence_source"].strip():
        raise ValueError("evidence_source is required")
    if type(required_stable_samples) is not int or required_stable_samples < 2:
        raise ValueError("required_stable_samples must be an integer >= 2")
    if type(max_sample_gap_ms) is not int or max_sample_gap_ms < 1:
        raise ValueError("max_sample_gap_ms must be a positive integer")

    observations = data.get("observations")
    if not isinstance(observations, list) or not observations:
        raise ValueError("observations must be a non-empty array")

    previous_timestamp = -1
    current_signature = None
    current_run = 0
    best_run = 0
    best_signature = None
    saw_explicit_pair_loss = False
    reconnect_run = 0
    reconnect_signature = None
    best_reconnect_run = 0
    best_reconnect_signature = None
    rejected = []
    continuity_breaks = []

    for index, observation in enumerate(observations):
        if not isinstance(observation, dict):
            raise ValueError(f"observation {index} must be an object")
        timestamp = observation.get("timestamp_ms")
        if type(timestamp) is not int or timestamp < 0:
            raise ValueError(f"observation {index} timestamp_ms must be a non-negative integer")
        if timestamp <= previous_timestamp:
            raise ValueError("observation timestamps must be strictly increasing")

        required_observation_fields = ("split_screen", "players", "player_array_count")
        missing_observation_fields = [
            field for field in required_observation_fields if field not in observation
        ]
        if missing_observation_fields:
            raise ValueError(
                f"observation {index} missing required fields: "
                + ", ".join(missing_observation_fields)
            )

        if previous_timestamp >= 0:
            sample_gap = timestamp - previous_timestamp
            if sample_gap > max_sample_gap_ms:
                continuity_breaks.append({
                    "index": index,
                    "timestamp_ms": timestamp,
                    "gap_ms": sample_gap,
                    "reason": "sample_gap_exceeded",
                })
                current_signature = None
                current_run = 0
                reconnect_signature = None
                reconnect_run = 0
        previous_timestamp = timestamp

        if type(observation.get("split_screen")) is not bool:
            raise ValueError(f"observation {index} split_screen must be boolean")
        players = observation.get("players")
        if not isinstance(players, list):
            raise ValueError(f"observation {index} players must be an array")
        count = observation.get("player_array_count")
        if type(count) is not int or count < 0:
            raise ValueError(f"observation {index} player_array_count must be a non-negative integer")

        valid, reason, signature = _valid_observation(observation)
        if not valid:
            rejected.append({
                "index": index,
                "timestamp_ms": timestamp,
                "reason": reason,
                "explicit_pair_loss": _is_explicit_pair_loss(observation),
            })
            if _is_explicit_pair_loss(observation):
                saw_explicit_pair_loss = True
            current_signature = None
            current_run = 0
            reconnect_signature = None
            reconnect_run = 0
            continue

        if signature == current_signature:
            current_run += 1
        else:
            current_signature = signature
            current_run = 1
        if current_run > best_run:
            best_run = current_run
            best_signature = signature

        if saw_explicit_pair_loss:
            if signature == reconnect_signature:
                reconnect_run += 1
            else:
                reconnect_signature = signature
                reconnect_run = 1
            if reconnect_run > best_reconnect_run:
                best_reconnect_run = reconnect_run
                best_reconnect_signature = signature

    stable = best_run >= required_stable_samples
    reconnect = best_reconnect_run >= required_stable_samples
    best_pair_mapping = []
    if best_signature is not None:
        for row in best_signature:
            best_pair_mapping.append({
                "local_client_index": row[0],
                "controller_port": row[1],
                "entity_id": row[2],
                "role": "PRIMARY" if row[8] else "SECONDARY",
            })

    return {
        "status": "PASS_STRUCTURAL_CAPTURE_VALIDATION" if stable else "INCOMPLETE_PAIR_OBSERVATION_DATA",
        "capture_id": data["capture_id"],
        "evidence_source": data["evidence_source"],
        "evidence_authenticity": "NOT_AUTHENTICATED_BY_THIS_TOOL",
        "title_id": data["title_id"],
        "title_update": data["title_update"],
        "mode": data["mode"],
        "observation_count": len(observations),
        "required_stable_samples": required_stable_samples,
        "max_sample_gap_ms": max_sample_gap_ms,
        "best_stable_run": best_run,
        "stable_pair_observation_validated": stable,
        "explicit_pair_loss_observed": saw_explicit_pair_loss,
        "reconnect_stable_run_after_pair_loss": best_reconnect_run,
        "reconnect_current_run_after_last_break": reconnect_run,
        "best_reconnect_pair_signature": best_reconnect_signature,
        "auto_rebind_cycle_observed_in_supplied_data": reconnect,
        "native_local_client_fields_consistent": stable,
        "controller_to_player_mapping_fields_consistent": stable,
        "independent_camera_hud_fields_consistent": stable,
        "campaign_alias_fields_consistent": stable,
        "best_pair_signature": best_signature,
        "best_pair_mapping": best_pair_mapping,
        "rejected_observations": rejected,
        "continuity_breaks": continuity_breaks,
        "limitations": [
            "The validator checks supplied observations; it does not read PS3 memory or authenticate the operator's capture source.",
            "A structural validation error is not treated as proof of a disconnect; reconnect requires an explicit split-screen loss or sub-two-player array observation.",
            "Stable samples must be no more than max_sample_gap_ms apart; widely separated snapshots cannot be combined into one stable run.",
            "A passing pair capture does not by itself prove that the original mission handlers are safe to patch.",
            "A reconnect cycle is reported separately and requires an explicit pair-loss observation followed by a new stable run; the best qualifying post-loss run is retained even if later samples break continuity.",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--required-stable-samples", type=int, default=5)
    parser.add_argument("--max-sample-gap-ms", type=int, default=DEFAULT_MAX_SAMPLE_GAP_MS)
    args = parser.parse_args(argv)
    try:
        data = json.loads(args.capture.read_text(encoding="utf-8"))
        result = validate_capture(
            data,
            required_stable_samples=args.required_stable_samples,
            max_sample_gap_ms=args.max_sample_gap_ms,
        )
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))
    return 0 if result["stable_pair_observation_validated"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
