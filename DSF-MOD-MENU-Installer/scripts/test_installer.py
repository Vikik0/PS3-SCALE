#!/usr/bin/env python3
"""Host-side regression checks for installer policy and path handling."""
from pathlib import Path

source = Path("source/main.c").read_text(encoding="utf-8")
assert '"/dev_hdd0/plugins/simple-cheat-menu.sprx"' in source
assert '"/dev_hdd0/boot_plugins.txt"' in source
assert '"/dev_hdd0/game/DSFMOD01/USRDIR/simple-cheat-menu.sprx"' in source
assert 'mode=fetch\\n' in source
assert "boot_add_plugin_line" in source
assert "boot_remove_plugin_line" in source
assert "BOOT_BACKUP" in source and "PLUGIN_BACKUP" in source

plugin_line = "/dev_hdd0/plugins/simple-cheat-menu.sprx"

def add_line(text):
    lines = text.splitlines()
    if plugin_line not in lines:
        lines.append(plugin_line)
    return "\n".join(lines) + ("\n" if lines else "")

def remove_line(text):
    return "".join(line + "\n" for line in text.splitlines() if line != plugin_line)

original = "/dev_hdd0/plugins/webftp_server.sprx\n/dev_hdd0/plugins/other.sprx\n"
installed = add_line(original)
assert installed.count(plugin_line) == 1
assert installed.startswith(original)
assert add_line(installed) == installed
assert remove_line(installed) == original
assert remove_line(original) == original

no_final_newline = "/dev_hdd0/plugins/webftp_server.sprx"
assert add_line(no_final_newline).splitlines() == [
    "/dev_hdd0/plugins/webftp_server.sprx", plugin_line
]

print("DSF installer host policy tests: PASS")
