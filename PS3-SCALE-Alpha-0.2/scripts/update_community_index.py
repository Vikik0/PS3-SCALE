#!/usr/bin/env python3
"""Build a compact, read-only index of public PS3 community NCL files."""

from __future__ import annotations

import csv
import json
import os
import re
import sys
import urllib.error
import urllib.request
from urllib.parse import quote
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "data" / "community_game_files.tsv"
MAX_FILE_BYTES = 1024 * 1024

SOURCES = (
    ("Nascar1243/PS3-FPS-Patches", "main", "HARDWARE_TESTED_FPS"),
    ("DoSpamu/RPCS3toArtemisPatches", "master", "COMMUNITY_UNVERIFIED"),
    ("RobertoTorino/ArtemisPS3Offline", "main", "ARTEMIS_DATABASE"),
    ("PS3-Pro/Game-Cheats", "main", "COMMUNITY_CHEATS"),
)

TITLE_ID_RE = re.compile(r"(?<![A-Z0-9])(?:[A-Z]{4}\d{5}|[A-Z]{5}\d{4})(?![A-Z0-9])", re.I)
VERSION_RE = re.compile(
    r"(?<![A-Z0-9])(?P<prefix>av|tv|v)?(?P<version>\d{2}\.\d{2})(?!\d)",
    re.I,
)
PATCH_PREFIX_RE = re.compile(
    r"^(?:File|Mem)\s+0x[0-9A-Fa-f]+\s+.*?(?:New\s+[0-9A-Fa-f]+)",
    re.I,
)


def parse_ncl_name(path: str) -> tuple[str, str, str]:
    """Return (first Title ID, game version, readable title) from an NCL path."""
    filename = PurePosixPath(path).name
    stem = re.sub(r"\.ncl$", "", filename, flags=re.I)
    ids = TITLE_ID_RE.findall(stem)
    title_id = ids[0].upper() if ids else ""

    app_version = ""
    for match in VERSION_RE.finditer(stem):
        if (match.group("prefix") or "").lower() not in ("av", "tv"):
            app_version = match.group("version")
            break

    title = PATCH_PREFIX_RE.sub("", stem)
    title = TITLE_ID_RE.sub(" ", title)
    title = re.sub(r"(?i)\b(?:av|tv|v)\d{2}\.\d{2}\b", " ", title)
    title = re.sub(r"(?<!\d)\d{2}\.\d{2}(?!\d)", " ", title)
    title = title.replace("_", " ").replace("-", " ")
    title = re.sub(r"\s+", " ", title).strip(" ._-\t\r\n")
    return title_id, app_version, title[:127]


def source_kind_for(repository: str, path: str, default_kind: str) -> str:
    if repository == "DoSpamu/RPCS3toArtemisPatches":
        if path.startswith("PSXPlace Confirmed/") or \
                path.startswith("Working Artemis Patches/"):
            return "COMMUNITY_CONFIRMED"
    return default_kind


def github_json(url: str) -> dict:
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "PS3-SCALE-community-index",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=60) as response:
            return json.load(response)
    except urllib.error.HTTPError as error:
        body = error.read(1000).decode("utf-8", errors="replace")
        raise RuntimeError(f"GitHub API returned HTTP {error.code}: {body}") from error


def graphics_description_from_ncl(content: str) -> str:
    """Extract only clearly named FPS/frame-rate sections from an NCL file."""
    matches: list[str] = []
    for raw_line in content.splitlines():
        line = raw_line.strip().lstrip("#/* ").strip()
        if not line or len(line) > 92:
            continue
        if re.search(
            r"\b(?:fps|frame[\s_-]*rate|frame[\s_-]*limit|"
            r"uncap(?:ped)?|unlock[\s_-]*fps|60[\s_-]*fps)\b",
            line,
            re.I,
        ):
            if line not in matches:
                matches.append(line)
        if len(matches) >= 2:
            break
    if matches:
        return "; ".join(matches)[:127]
    return "FPS patch; exact in-game effect is not documented in this file."


def metadata_for_file(
    repository: str,
    branch: str,
    path: str,
    source_kind: str,
    size: int,
) -> tuple[str, str]:
    """Classify the file for the UI without treating cheat databases as graphics."""
    if source_kind == "HARDWARE_TESTED_FPS":
        description = "FPS patch; exact in-game effect is not documented in this file."
        try:
            url = (
                f"https://raw.githubusercontent.com/{repository}/"
                f"{quote(branch, safe='')}/{quote(path, safe='/')}"
            )
            request = urllib.request.Request(
                url,
                headers={"User-Agent": "PS3-SCALE-community-index"},
            )
            with urllib.request.urlopen(request, timeout=20) as response:
                content = response.read(min(size, 128 * 1024)).decode(
                    "utf-8", errors="replace"
                )
            description = graphics_description_from_ncl(content)
        except (OSError, urllib.error.URLError, TimeoutError):
            pass
        return "GRAPHICS_FPS", description

    if source_kind == "COMMUNITY_CHEATS":
        return "GAMEPLAY_CHEAT", "Gameplay cheat code; not a graphics enhancement."
    if source_kind == "ARTEMIS_DATABASE":
        return "GAMEPLAY_CODE", "Artemis code entry; not verified as a graphics enhancement."
    return "COMMUNITY_CODE", "Community NCL code; graphics effect is not confirmed."


def build_records() -> list[tuple[str, ...]]:
    records: list[tuple[str, ...]] = []
    seen: set[tuple[str, str]] = set()

    for repository, branch, default_kind in SOURCES:
        url = (
            f"https://api.github.com/repos/{repository}/git/trees/"
            f"{branch}?recursive=1"
        )
        tree = github_json(url)
        if tree.get("truncated"):
            raise RuntimeError(
                f"GitHub returned a truncated tree for {repository}; "
                "refusing to publish an incomplete index."
            )

        for entry in tree.get("tree", []):
            path = entry.get("path", "")
            size = int(entry.get("size") or 0)
            if entry.get("type") != "blob" or \
                    not path.lower().endswith(".ncl") or \
                    size <= 0 or size > MAX_FILE_BYTES:
                continue
            if len(path.encode("utf-8")) >= 384:
                continue

            title_id, app_version, title = parse_ncl_name(path)
            if not title_id and not title:
                continue

            key = (repository, path)
            if key in seen:
                continue
            seen.add(key)

            kind = source_kind_for(repository, path, default_kind)
            category, description = metadata_for_file(
                repository, branch, path, kind, size
            )
            clean_title = title.replace("\t", " ").replace("\r", " ").replace("\n", " ")
            clean_path = path.replace("\t", " ").replace("\r", " ").replace("\n", " ")
            clean_description = description.replace("\t", " ").replace("\r", " ").replace("\n", " ")[:127]
            records.append((
                title_id,
                app_version,
                clean_title,
                kind,
                repository,
                branch,
                clean_path,
                str(size),
                category,
                clean_description,
            ))

    priority = {
        "HARDWARE_TESTED_FPS": 0,
        "COMMUNITY_CONFIRMED": 1,
        "COMMUNITY_UNVERIFIED": 2,
        "ARTEMIS_DATABASE": 3,
        "COMMUNITY_CHEATS": 4,
    }
    records.sort(key=lambda row: (
        priority.get(row[3], 9),
        row[2].casefold(),
        row[0],
        row[1],
        row[4],
        row[6],
    ))
    return records


def write_index(records: list[tuple[str, ...]]) -> None:
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT.open("w", encoding="utf-8", newline="") as handle:
        handle.write("# PS3 SCALE community game-file index v2\n")
        handle.write("# Public GitHub NCL metadata; community files are never auto-applied.\n")
        handle.write("# Fields: TITLE_ID APP_VER TITLE SOURCE_KIND REPOSITORY BRANCH PATH SIZE CATEGORY DESCRIPTION\n")
        writer = csv.writer(
            handle,
            delimiter="\t",
            quotechar='"',
            quoting=csv.QUOTE_MINIMAL,
            lineterminator="\n",
        )
        writer.writerows(records)


def main() -> int:
    records = build_records()
    if not records:
        raise RuntimeError("No community NCL files were found; refusing empty index.")
    write_index(records)
    print(f"Wrote {len(records)} community NCL records to {OUTPUT}")
    print(f"Index size: {OUTPUT.stat().st_size} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
