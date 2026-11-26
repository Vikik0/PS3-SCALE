#!/usr/bin/env python3
"""Bind an operator-supplied player-pair capture to the exact Campaign SELF file."""
from __future__ import annotations

import hashlib
import re
from pathlib import Path
from typing import Any

SHA256_RE = re.compile(r"^[0-9a-fA-F]{64}$")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def bind_capture_to_campaign_self(
    capture: dict[str, Any],
    campaign_self: Path,
) -> dict[str, Any]:
    """Compare the capture's declared SELF hash with the analyzer's exact input.

    A matching hash binds the JSON to the file being analyzed, but it does not
    authenticate who collected the runtime observations or prove they came
    from a physical PS3.
    """
    if not isinstance(capture, dict):
        raise ValueError("player-pair capture root must be a JSON object")

    actual_hash = sha256_file(Path(campaign_self))
    declared_hash = capture.get("campaign_self_sha256")

    if declared_hash is None:
        return {
            "status": "NOT_PROVIDED",
            "campaign_self_sha256": actual_hash,
            "capture_declared_campaign_self_sha256": None,
            "capture_bound_to_analyzer_self": False,
        }

    if not isinstance(declared_hash, str) or SHA256_RE.fullmatch(declared_hash) is None:
        raise ValueError("campaign_self_sha256 must be a 64-character hexadecimal SHA-256")

    if declared_hash.lower() != actual_hash:
        raise ValueError(
            "campaign_player_capture campaign_self_sha256 does not match "
            "the supplied Campaign default.self"
        )

    return {
        "status": "PASS_CAMPAIGN_SELF_SHA256_MATCH",
        "campaign_self_sha256": actual_hash,
        "capture_declared_campaign_self_sha256": declared_hash.lower(),
        "capture_bound_to_analyzer_self": True,
    }
