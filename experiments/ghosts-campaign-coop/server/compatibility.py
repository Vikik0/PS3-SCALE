"""Known Call of Duty: Ghosts PS3 title profiles.

Profiles are identification/target metadata only. They are not proof that a
Campaign co-op client has been validated on any retail game build.
"""
from __future__ import annotations

from dataclasses import asdict, dataclass

TARGET_TITLE_UPDATE = "1.16"
TARGET_FIRMWARE_TYPE = "CEX"
TARGET_FIRMWARE_VERSION = "4.93"


@dataclass(frozen=True)
class GameProfile:
    title_id: str
    region: str
    distribution: str
    edition: str = "Standard"


_PROFILES = (
    GameProfile("BLUS31270", "US", "disc"),
    GameProfile("NPUB31301", "US", "digital"),
    GameProfile("NPUB50021", "US", "digital", "Hardened Edition"),
    GameProfile("BLES01945", "EU/AU", "disc"),
    GameProfile("BLES01948", "EU/AU", "disc"),
    GameProfile("NPEB01832", "EU", "digital"),
    GameProfile("NPEB01835", "EU", "digital"),
    GameProfile("BLJM61125", "JP", "disc"),
    GameProfile("BLJM61126", "JP", "disc"),
    GameProfile("NPJB00528", "JP", "digital"),
)

GAME_PROFILES = {profile.title_id: profile for profile in _PROFILES}


def list_profiles() -> dict:
    return {
        "game": "Call of Duty: Ghosts",
        "platform": "PlayStation 3",
        "target_title_update": TARGET_TITLE_UPDATE,
        "target_firmware": {
            "type": TARGET_FIRMWARE_TYPE,
            "version": TARGET_FIRMWARE_VERSION,
            "status": "target_only_not_hardware_verified",
        },
        "title_ids": [asdict(profile) for profile in _PROFILES],
        "important": (
            "These are known regional/distribution identifiers, not a claim that "
            "one EBOOT/SPRX works across every build. Each title ID needs its "
            "matching game update and a separately validated client build."
        ),
    }


def inspect_install(
    title_id: str,
    title_update: str,
    firmware_type: str,
    firmware_version: str,
) -> dict:
    normalized_title_id = (title_id or "").strip().upper()
    normalized_tu = (title_update or "").strip().lstrip("0")
    normalized_firmware_type = (firmware_type or "").strip().upper()
    normalized_firmware_version = (firmware_version or "").strip()

    profile = GAME_PROFILES.get(normalized_title_id)
    if not profile:
        return {
            "status": "unknown_title_id",
            "compatible": False,
            "title_id": normalized_title_id,
            "message": "This title ID is not in the current Ghosts PS3 profile registry.",
        }

    if normalized_tu != TARGET_TITLE_UPDATE:
        return {
            "status": "unsupported_title_update",
            "compatible": False,
            "profile": asdict(profile),
            "required_title_update": TARGET_TITLE_UPDATE,
            "message": "The current research target is title update 1.16 only.",
        }

    if normalized_firmware_type != TARGET_FIRMWARE_TYPE:
        return {
            "status": "unsupported_firmware_type",
            "compatible": False,
            "profile": asdict(profile),
            "required_firmware": {
                "type": TARGET_FIRMWARE_TYPE,
                "version": TARGET_FIRMWARE_VERSION,
            },
            "message": "The first client target is CEX. DEX and HEN need separate builds and tests.",
        }

    if normalized_firmware_version != TARGET_FIRMWARE_VERSION:
        return {
            "status": "firmware_not_yet_verified",
            "compatible": False,
            "profile": asdict(profile),
            "target_firmware": {
                "type": TARGET_FIRMWARE_TYPE,
                "version": TARGET_FIRMWARE_VERSION,
            },
            "message": "CEX 4.93 is the first target; other CEX firmware versions are not verified yet.",
        }

    return {
        "status": "target_profile_not_hardware_verified",
        "compatible": None,
        "profile": asdict(profile),
        "title_update": TARGET_TITLE_UPDATE,
        "firmware": {
            "type": TARGET_FIRMWARE_TYPE,
            "version": TARGET_FIRMWARE_VERSION,
        },
        "message": "This matches the first development target, but Campaign co-op has not been proven on hardware.",
    }
