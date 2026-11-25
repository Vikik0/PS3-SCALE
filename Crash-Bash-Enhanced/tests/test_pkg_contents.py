#!/usr/bin/env python3
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HELPER = ROOT / "scripts" / "verify_pkg_contents.py"


def write_pkg(path: Path, item_count: int) -> None:
    header = bytearray(0x40)
    header[:4] = b"\x7fPKG"
    struct.pack_into(">I", header, 4, 1)
    struct.pack_into(">I", header, 0x14, item_count)
    path.write_bytes(header)


def run_case(item_count: int, listing: str, expect_success: bool) -> tuple[subprocess.CompletedProcess, str]:
    with tempfile.TemporaryDirectory(prefix="cb-pkg-verify-") as temp:
        root = Path(temp)
        stage = root / "stage"
        (stage / "USRDIR").mkdir(parents=True)
        (stage / "ICON0.PNG").write_bytes(b"icon")
        (stage / "USRDIR" / "EBOOT.BIN").write_bytes(b"self")
        pkg_file = root / "test.pkg"
        report = root / "PKG-CONTENTS.txt"
        write_pkg(pkg_file, item_count)

        listing_file = root / "listing.txt"
        listing_file.write_text(listing, encoding="utf-8")
        fake_bin = root / "bin"
        fake_bin.mkdir()
        fake_pkg = fake_bin / "pkg"
        fake_pkg.write_text(
            "#!/usr/bin/env python3\n"
            "import os, sys\n"
            "if len(sys.argv) != 3 or sys.argv[1] != '-l': raise SystemExit(2)\n"
            "print(open(os.environ['FAKE_PKG_LISTING'], encoding='utf-8').read(), end='')\n",
            encoding="utf-8",
        )
        fake_pkg.chmod(0o755)

        env = dict(os.environ)
        env["PATH"] = str(fake_bin) + os.pathsep + env.get("PATH", "")
        env["FAKE_PKG_LISTING"] = str(listing_file)
        result = subprocess.run(
            [sys.executable, str(HELPER), str(pkg_file), str(stage), str(report)],
            env=env,
            text=True,
            capture_output=True,
        )
        if expect_success:
            assert result.returncode == 0, result.stderr
            assert "PKG item count: 3 / 1024" in report.read_text(encoding="utf-8")
        else:
            assert result.returncode != 0
        return result, result.stderr


def main() -> None:
    valid_listing = """[X] Item Count: 00000003
Listing: "test.pkg"
+) overwrite, -) no overwrite

   raw data:+          4: ICON0.PNG
   raw data:+          4: USRDIR/EBOOT.BIN
  directory:+          0: USRDIR
"""
    run_case(3, valid_listing, True)
    run_case(3, valid_listing.replace("USRDIR/EBOOT.BIN", "USRDIR/MISSING.BIN"), False)
    run_case(1024, valid_listing, False)
    print("PKG entry-cap and package-content verifier tests passed.")


if __name__ == "__main__":
    main()
