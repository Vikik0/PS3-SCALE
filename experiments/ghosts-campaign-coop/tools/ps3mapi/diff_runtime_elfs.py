#!/usr/bin/env python3
"""Compare overlapping Campaign/Multiplayer PT_LOAD bytes by virtual address."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
try:
    from .scan_runtime_strings import load_segments
except ImportError:
    from scan_runtime_strings import load_segments

def _sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def compare(campaign_elf: Path, multiplayer_elf: Path, block_size: int = 0x1000, executable_only: bool = True) -> dict:
    if block_size < 0x100 or block_size > 0x10000 or block_size & (block_size - 1):
        raise ValueError("block_size must be a power of two from 0x100 through 0x10000")
    campaign_entry, campaign_segments = load_segments(campaign_elf)
    multiplayer_entry, multiplayer_segments = load_segments(multiplayer_elf)
    pairs = []
    total_equal = 0
    total_different = 0
    total_blocks = 0
    changed_blocks = []
    changed_truncated = False

    for c_index, c_flags, c_vaddr, c_blob in campaign_segments:
        if executable_only and not (c_flags & 1):
            continue
        c_end = c_vaddr + len(c_blob)
        for m_index, m_flags, m_vaddr, m_blob in multiplayer_segments:
            if executable_only and not (m_flags & 1):
                continue
            m_end = m_vaddr + len(m_blob)
            start = max(c_vaddr, m_vaddr)
            end = min(c_end, m_end)
            if start >= end:
                continue
            length = end - start
            c_offset = start - c_vaddr
            m_offset = start - m_vaddr
            pair_equal = 0
            pair_different = 0
            pair_blocks = 0
            for offset in range(0, length, block_size):
                size = min(block_size, length - offset)
                c_chunk = c_blob[c_offset + offset:c_offset + offset + size]
                m_chunk = m_blob[m_offset + offset:m_offset + offset + size]
                pair_blocks += 1
                total_blocks += 1
                if c_chunk == m_chunk:
                    pair_equal += size
                    total_equal += size
                else:
                    pair_different += size
                    total_different += size
                    if len(changed_blocks) < 512:
                        changed_blocks.append({
                            "campaign_segment": c_index,
                            "multiplayer_segment": m_index,
                            "virtual_address": f"0x{start + offset:X}",
                            "size": size,
                            "campaign_sha256": _sha(c_chunk),
                            "multiplayer_sha256": _sha(m_chunk),
                        })
                    else:
                        changed_truncated = True
            pairs.append({
                "campaign_segment": c_index,
                "multiplayer_segment": m_index,
                "campaign_flags": c_flags,
                "multiplayer_flags": m_flags,
                "overlap_start": f"0x{start:X}",
                "overlap_end_exclusive": f"0x{end:X}",
                "overlap_size": length,
                "block_count": pair_blocks,
                "identical_bytes": pair_equal,
                "different_bytes": pair_different,
                "identical_fraction": round(pair_equal / length, 6) if length else 0.0,
            })

    return {
        "campaign_elf": str(campaign_elf),
        "multiplayer_elf": str(multiplayer_elf),
        "campaign_entry_point": f"0x{campaign_entry:X}",
        "multiplayer_entry_point": f"0x{multiplayer_entry:X}",
        "comparison": "file-backed PT_LOAD bytes at matching virtual addresses",
        "executable_only": executable_only,
        "block_size": block_size,
        "overlapping_segment_pairs": pairs,
        "totals": {
            "blocks_compared": total_blocks,
            "identical_bytes": total_equal,
            "different_bytes": total_different,
            "identical_fraction": round(total_equal / (total_equal + total_different), 6)
                if total_equal + total_different else None,
        },
        "changed_blocks": changed_blocks,
        "changed_blocks_truncated": changed_truncated,
        "interpretation_limit": (
            "Different bytes may reflect build changes, runtime relocations, data, or mode-specific code. "
            "A changed block is not a function boundary and does not identify a safe hook."
        ),
    }

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign_elf", type=Path)
    parser.add_argument("multiplayer_elf", type=Path)
    parser.add_argument("--block-size", type=lambda value: int(value, 0), default=0x1000)
    parser.add_argument("--include-data", action="store_true", help="compare non-executable PT_LOAD segments too")
    parser.add_argument("--output", type=Path, default=Path("campaign_mp_runtime_binary_diff.json"))
    args = parser.parse_args(argv)
    try:
        result = compare(args.campaign_elf, args.multiplayer_elf, args.block_size, not args.include_data)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(result["totals"], indent=2))
    print(f"Overlapping segment pairs: {len(result['overlapping_segment_pairs'])}")
    print(f"Report: {args.output}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
