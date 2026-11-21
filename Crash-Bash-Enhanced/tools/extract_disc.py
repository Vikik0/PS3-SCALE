#!/usr/bin/env python3
"""Extract a retail Crash Bash (USA) PS1 BIN/CUE and split its CRASHBSH.DAT.

Run:
  python3 tools/extract_disc.py --bin "Crash Bash (USA).bin" --cue "Crash Bash (USA).cue" --out ./assets
No game data is written into the repository.
"""
import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

RAW_SECTOR = 2352
USER_SECTOR = 2048
PSX_DAT_TABLE = 0x8004E110
PSX_DAT_COUNT = 992


def cue_validate(cue_path, bin_path):
    if cue_path is None:
        return
    text = Path(cue_path).read_text(encoding="utf-8", errors="replace")
    match = re.search(r'^\s*FILE\s+"([^"]+)"\s+BINARY\s*$', text, re.I | re.M)
    if not match:
        raise ValueError('Unsupported CUE: expected one FILE "..." BINARY entry')
    if Path(match.group(1)).name.lower() != Path(bin_path).name.lower():
        raise ValueError("CUE references a different BIN filename")
    if not re.search(r'^\s*TRACK\s+01\s+MODE2/2352\s*$', text, re.I | re.M):
        raise ValueError("Expected a MODE2/2352 data track")


def sector_payload(fp, lba):
    fp.seek(lba * RAW_SECTOR)
    raw = fp.read(RAW_SECTOR)
    if len(raw) != RAW_SECTOR:
        raise ValueError(f"Short raw sector at LBA {lba}")
    if raw[:12] != b"\x00" + b"\xff" * 10 + b"\x00" or raw[15] != 2:
        raise ValueError(f"Invalid Mode 2 sector at LBA {lba}")
    sub = raw[16:20]
    if sub != raw[20:24]:
        raise ValueError(f"Mode 2 subheader copies differ at LBA {lba}")
    if sub[2] & 0x20:
        raise ValueError(f"Mode 2 Form 2 sector is not supported at LBA {lba}")
    return raw[24:24 + USER_SECTOR]


def iso_record(buf, pos):
    length = buf[pos]
    if length == 0:
        return None
    rec = buf[pos:pos + length]
    if length < 34 or len(rec) != length:
        raise ValueError("Malformed ISO9660 directory record")
    extent = int.from_bytes(rec[2:6], "little")
    size = int.from_bytes(rec[10:14], "little")
    flags = rec[25]
    name_len = rec[32]
    name_bytes = rec[33:33 + name_len]
    if len(name_bytes) != name_len:
        raise ValueError("Malformed ISO9660 filename")
    if name_bytes == b"\x00":
        name = "."
    elif name_bytes == b"\x01":
        name = ".."
    else:
        name = name_bytes.decode("ascii", "replace")
    return {"name": name, "extent": extent, "size": size,
            "is_dir": bool(flags & 2), "length": length}


def iso_read_dir(fp, entry):
    out = bytearray()
    count = (entry["size"] + USER_SECTOR - 1) // USER_SECTOR
    for i in range(count):
        out.extend(sector_payload(fp, entry["extent"] + i))
    return bytes(out[:entry["size"]])


def iso_list_dir(fp, entry):
    data = iso_read_dir(fp, entry)
    result, pos = [], 0
    while pos < len(data):
        if data[pos] == 0:
            pos = ((pos // USER_SECTOR) + 1) * USER_SECTOR
            continue
        rec = iso_record(data, pos)
        if rec is None:
            break
        result.append(rec)
        pos += rec["length"]
    return result


def iso_find(fp, path):
    pvd = sector_payload(fp, 16)
    if pvd[1:6] != b"CD001" or pvd[0] != 1 or pvd[6] != 1:
        raise ValueError("ISO9660 primary volume descriptor not found at LBA 16")
    root = iso_record(pvd, 156)
    if root is None or not root["is_dir"]:
        raise ValueError("ISO9660 root directory record is invalid")
    current = root
    for part in [p for p in path.replace("\\", "/").split("/") if p]:
        wanted = part.split(";")[0].rstrip(".").upper()
        matches = [e for e in iso_list_dir(fp, current)
                   if e["name"].split(";")[0].rstrip(".").upper() == wanted]
        if len(matches) != 1:
            raise ValueError(f"ISO path component {part!r}: expected one match, found {len(matches)}")
        current = matches[0]
    return current


def iso_extract(fp, entry, output):
    remaining = entry["size"]
    with open(output, "wb") as dst:
        lba = entry["extent"]
        while remaining:
            payload = sector_payload(fp, lba)
            count = min(remaining, USER_SECTOR)
            dst.write(payload[:count])
            remaining -= count
            lba += 1


def psx_dat_index(exe_bytes, table_addr=PSX_DAT_TABLE, count=PSX_DAT_COUNT):
    if len(exe_bytes) < 0x800 or exe_bytes[:8] != b"PS-X EXE":
        raise ValueError("Not a PS-X EXE")
    text_addr = struct.unpack_from("<I", exe_bytes, 0x18)[0]
    file_offset = 0x800 + table_addr - text_addr
    end = file_offset + count * 8
    if file_offset < 0x800 or end > len(exe_bytes):
        raise ValueError("DAT index table lies outside the executable")
    entries = [struct.unpack_from("<II", exe_bytes, file_offset + i * 8)
               for i in range(count)]
    return entries, file_offset, text_addr


def dat_gaps(entries, total_size):
    ranges = sorted((sector * USER_SECTOR, sector * USER_SECTOR + size)
                    for sector, size in entries)
    gaps, pos = [], 0
    for start, end in ranges:
        if start < pos:
            raise ValueError("Overlapping DAT index entries")
        if end > total_size:
            raise ValueError("DAT index entry exceeds archive size")
        if start > pos:
            gaps.append((pos, start))
        pos = end
    if pos < total_size:
        gaps.append((pos, total_size))
    return gaps


def split_dat(dat_bytes, entries):
    gaps = dat_gaps(entries, len(dat_bytes))
    files = [dat_bytes[sector * USER_SECTOR:sector * USER_SECTOR + size]
             for sector, size in entries]
    filler = b"".join(dat_bytes[start:end] for start, end in gaps)
    rebuilt = bytearray(len(dat_bytes))
    for (sector, size), content in zip(entries, files):
        rebuilt[sector * USER_SECTOR:sector * USER_SECTOR + len(content)] = content
    pos = 0
    for start, end in gaps:
        rebuilt[start:end] = filler[pos:pos + end - start]
        pos += end - start
    if bytes(rebuilt) != dat_bytes:
        raise ValueError("DAT split failed byte-identical round-trip verification")
    return files, filler, gaps


def write_assets(dat_bytes, exe_bytes, entries, files, filler, gaps, out_dir):
    entry_dir = out_dir / "CRASHBSH"
    entry_dir.mkdir(parents=True, exist_ok=True)
    index_entries = []
    for i, ((sector, size), content) in enumerate(zip(entries, files)):
        ident = f"{i:03X}"
        (entry_dir / f"{ident}.bin").write_bytes(content)
        index_entries.append({
            "id": ident, "sector": sector, "size": size,
            "sha256": hashlib.sha256(content).hexdigest()
        })
    (entry_dir / "filler.bin").write_bytes(filler)
    metadata = {
        "source": "CRASHBSH.DAT",
        "size": len(dat_bytes),
        "sha1": hashlib.sha1(dat_bytes).hexdigest(),
        "sha256": hashlib.sha256(dat_bytes).hexdigest(),
        "executable_sha256": hashlib.sha256(exe_bytes).hexdigest(),
        "index_table_address": f"0x{PSX_DAT_TABLE:08X}",
        "index_table_count": len(entries),
        "filler_size": len(filler),
        "gap_count": len(gaps),
        "entries": index_entries
    }
    (entry_dir / "index.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    return metadata


def extract_disc(bin_path, out_dir, cue_path=None):
    cue_validate(cue_path, bin_path)
    bin_path, out_dir = Path(bin_path), Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    if bin_path.stat().st_size % RAW_SECTOR:
        raise ValueError("BIN size is not a whole number of 2352-byte sectors")
    with open(bin_path, "rb") as fp:
        dat_entry = iso_find(fp, "CRASHBSH/CRASHBSH.DAT")
        exe_entry = iso_find(fp, "SCUS_945.70")
        cnf_entry = iso_find(fp, "SYSTEM.CNF")
        iso_extract(fp, dat_entry, out_dir / "CRASHBSH.DAT")
        iso_extract(fp, exe_entry, out_dir / "SCUS_945.70")
        iso_extract(fp, cnf_entry, out_dir / "SYSTEM.CNF")
        bashy_entry = iso_find(fp, "BASHY")
        iso_extract(fp, bashy_entry, out_dir / "BASHY")
    dat_bytes = (out_dir / "CRASHBSH.DAT").read_bytes()
    exe_bytes = (out_dir / "SCUS_945.70").read_bytes()
    entries, table_offset, text_addr = psx_dat_index(exe_bytes)
    files, filler, gaps = split_dat(dat_bytes, entries)
    metadata = write_assets(dat_bytes, exe_bytes, entries, files, filler, gaps, out_dir)
    metadata.update({
        "disc_bin": bin_path.name,
        "disc_bin_size": bin_path.stat().st_size,
        "executable_table_file_offset": f"0x{table_offset:X}",
        "executable_text_address": f"0x{text_addr:08X}",
        "dat_entry_count": len(files)
    })
    (out_dir / "disc_manifest.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin", required=True, help="retail Mode2/2352 BIN image")
    parser.add_argument("--cue", help="matching single-track MODE2/2352 CUE")
    parser.add_argument("--out", required=True, help="output directory; no game data is uploaded")
    args = parser.parse_args()
    try:
        result = extract_disc(args.bin, args.out, args.cue)
    except (OSError, ValueError) as exc:
        print(f"extract_disc: {exc}", file=sys.stderr)
        return 1
    print(f"Extracted {result['dat_entry_count']} DAT entries.")
    print(f"CRASHBSH.DAT: {result['size']} bytes; SHA-1 {result['sha1']}")
    print(f"Assets: {Path(args.out) / 'CRASHBSH'}")
    print("Byte-identical DAT reconstruction: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
