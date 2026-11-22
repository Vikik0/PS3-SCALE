#!/usr/bin/env python3
"""Guard PS3 package identity and direct static-core launch invariants."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
script = (ROOT / "scripts/build_online_pkg_clean.sh").read_text()
sfo_root = ET.parse(ROOT / "sfo.xml").getroot()
sfo_values = {
    node.attrib["name"]: (node.text or "").strip()
    for node in sfo_root.findall("value")
}
app_id_match = re.search(r'^APP_ID="([^"]+)"$', script, re.M)
content_id_match = re.search(r'^CONTENT_ID="([^"]+)"$', script, re.M)
assert app_id_match and content_id_match, "Build script must declare APP_ID and CONTENT_ID"
app_id = app_id_match.group(1)
content_id = content_id_match.group(1)
assert re.fullmatch(r"[A-Z]{4}[0-9]{5}", app_id), f"Invalid PS3 TITLE_ID: {app_id}"
assert content_id == f"UP0001-{app_id}_00-0000000000000001"
assert sfo_values.get("TITLE_ID") == app_id, "SFO template TITLE_ID must match the package"
assert sfo_values.get("TITLE") == "Crash Bash Online"
assert sfo_values.get("CATEGORY") == "HG"
assert '/dev_hdd0/game/CBON00001/USRDIR/cores/pcsx_rearmed_libretro_psl1ght.SELF' in script
assert 'make -C "$RA" -f Makefile.psl1ght -j"$JOBS" create-core CONTENTID="$CONTENT_ID"' in script
assert 'make_self_npdrm "$RA/build/retroarch_psl1ght.elf" "$USRDIR/EBOOT.BIN" "$CONTENT_ID"' in script
assert '"$NM" -g "$RA/retroarch_psl1ght.elf"' in script, "Check static symbols before stripping"
assert 'sfo --title "$APP_TITLE" --appid "$APP_ID" -f "$ROOT/sfo.xml"' in script
assert "values.get(\"TITLE_ID\") != app_id" in script
assert "content_id.encode(\"ascii\") not in eboot" in script
assert 'CORE_SELF="$USRDIR/cores/pcsx_rearmed_libretro_psl1ght.SELF"' in script
assert "create-core CONTENTID=" in script, "The core-info identity SELF must be created"

prototype_makefile = (ROOT / "Makefile").read_text()
prototype_sfo = ET.parse(ROOT / "sfo-native.xml").getroot()
prototype_sfo_values = {
    node.attrib["name"]: (node.text or "").strip()
    for node in prototype_sfo.findall("value")
}
prototype_app_id = re.search(r"^APPID := ([A-Z0-9]+)$", prototype_makefile, re.M)
prototype_content_id = re.search(r"^CONTENTID := ([A-Z0-9_-]+)$", prototype_makefile, re.M)
prototype_title = re.search(r"^TITLE := (.+)$", prototype_makefile, re.M)
assert prototype_app_id and re.fullmatch(r"[A-Z]{4}[0-9]{5}", prototype_app_id.group(1))
assert prototype_content_id and prototype_content_id.group(1).startswith(
    f"UP0001-{prototype_app_id.group(1)}_00-"
), "Native lobby Content ID must match its app ID"
assert prototype_title and prototype_sfo_values.get("TITLE") == prototype_title.group(1)
assert prototype_sfo_values.get("TITLE_ID") == prototype_app_id.group(1), (
    "Native lobby SFO TITLE_ID must match its Makefile APPID"
)
prototype_online = (ROOT / "source/online.c").read_text()
assert f"/dev_hdd0/game/{prototype_app_id.group(1)}/USRDIR/cacert.pem" in prototype_online, (
    "Native lobby CA bundle path must match its installed app ID"
)
assert prototype_app_id.group(1) != app_id, "Native lobby prototype and RetroArch app must have distinct IDs"

for relative in (
    "README.md",
    "BUILD_TRIGGER.md",
    "HARDWARE_TEST_PLAN.md",
    "PS1-ENGINE-BASE.md",
):
    text = (ROOT / relative).read_text()
    assert "CBASH0001" not in text, f"Stale installed app path in {relative}"

print("PS3 package metadata and direct static-core launch checks passed.")
