#!/usr/bin/env python3
"""Guard the unified PS3 PKG: one app ID, one XMB entry, nested RetroArch runtime."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
build_script = (ROOT / "scripts/build_online_pkg_clean.sh").read_text()
makefile = (ROOT / "Makefile").read_text()
launcher = (ROOT / "source/game_launcher.c").read_text()
online = (ROOT / "source/online.c").read_text()
disc_browser = (ROOT / "source/disc_browser.c").read_text()
stage_script = (ROOT / "scripts/stage_game_disc.py").read_text()
personal_builder = (ROOT / "scripts/build_personal_pkg.sh").read_text()
workflow = (ROOT.parent / ".github/workflows/crash-bash-build.yml").read_text()
sfo_root = ET.parse(ROOT / "sfo.xml").getroot()
sfo_values = {
    node.attrib["name"]: (node.text or "").strip()
    for node in sfo_root.findall("value")
}

app_id_match = re.search(r'^APP_ID="([^"]+)"$', build_script, re.M)
content_id_match = re.search(r'^CONTENT_ID="([^"]+)"$', build_script, re.M)
assert app_id_match and content_id_match
app_id = app_id_match.group(1)
content_id = content_id_match.group(1)
assert app_id == "CBON00001"
assert content_id == "UP0001-CBON00001_00-0000000000000001"
assert sfo_values.get("TITLE_ID") == app_id
assert sfo_values.get("TITLE") == "Crash Bash Online"
assert sfo_values.get("CATEGORY") == "HG"

# Native Tiny3D lobby is the single XMB application; RetroArch is nested within it.
assert "TITLE := Crash Bash Online" in makefile
assert "APPID := CBON00001" in makefile
assert "CONTENTID := UP0001-$(APPID)_00-0000000000000001" in makefile
assert "SFOXML := $(CURDIR)/sfo.xml" in makefile
assert "APPID := CBEN00001" not in makefile
assert "/dev_hdd0/game/CBON00001/USRDIR/cacert.pem" in online
assert '#define CB_GAME_EBOOT "/dev_hdd0/game/CBON00001/USRDIR/retroarch/EBOOT.BIN"' in launcher

# Embedded RetroArch and its static core resolve under the same installed title.
assert "/dev_hdd0/game/CBON00001/USRDIR/retroarch/cores/pcsx_rearmed_libretro_psl1ght.SELF" in build_script
assert 'system_directory = "/dev_hdd0/game/$APP_ID/USRDIR/retroarch/system"' in build_script
assert 'RETROARCH_STAGE="$ROOT/build-artifacts/retroarch-stage/USRDIR/retroarch"' in build_script
assert 'cp -a "$USRDIR/." "$RETROARCH_STAGE/"' in build_script
assert 'test -s "$RETROARCH_STAGE/EBOOT.BIN"' in build_script
assert 'make_self_npdrm "$RA/build/retroarch_psl1ght.elf" "$USRDIR/EBOOT.BIN" "$CONTENT_ID"' in build_script
assert 'sfo --title "$APP_TITLE" --appid "$APP_ID" -f "$ROOT/sfo.xml"' in build_script
assert '"$NM" -g "$RA/retroarch_psl1ght.elf"' in build_script

# The private personal builder embeds the supplied BIN/CUE in the same one-PKG app.
assert '"/dev_hdd0/game/CBON00001/USRDIR/PSXISO"' in disc_browser
assert "stage_game_disc.py" in personal_builder
assert "--required" in personal_builder
assert "PSXISO" in stage_script
assert 'DISC_DEST="$ROOT/pkgfiles/USRDIR/PSXISO"' in personal_builder
assert "Crash-Bash-Online-Personal-PS3.pkg" in personal_builder
assert 'if [[ -n "${CRASH_BASH_DISC_ARCHIVE:-}" ]]' in personal_builder
assert '--archive "$CRASH_BASH_DISC_ARCHIVE"' in personal_builder
assert 'cmp -s "$source_file" "$packaged_file"' in personal_builder
assert 'PERSONAL_README="$RETROARCH_DIR/README-Crash-Bash-Online.txt"' in personal_builder
assert "CRASH BASH ONLINE — PERSONAL GAME-INCLUSIVE PS3 PACKAGE" in personal_builder
assert "No second PKG or external game copy is required for this included disc." in personal_builder

# Public CI embeds the runtime and publishes one installable PKG without game data.
assert "needs: online-package-build" in workflow
assert "name: Crash-Bash-RetroArch-Stage" in workflow
assert "Crash-Bash-Online-Unified-PS3.pkg" in workflow
assert "Crash-Bash-Online-Unified-PS3-PKG" in workflow
assert "Crash-Bash-Online-PS3-PKG" not in workflow
assert "CBEN00001" not in (ROOT / "README.md").read_text()
assert "CBEN00001" not in (ROOT / "HARDWARE_TEST_PLAN.md").read_text()

print("Unified Crash Bash PS3 package metadata checks passed.")
