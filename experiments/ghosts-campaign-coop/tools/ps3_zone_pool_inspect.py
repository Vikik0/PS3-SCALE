#!/usr/bin/env python3
"""Read-only inspector for the asset pool in an outer-decompressed IW6 PS3 zone."""
from __future__ import annotations
import argparse,csv,json,struct,sys
from collections import Counter
from pathlib import Path
from typing import Any

KNOWN_TYPES={0x02:"xanim",0x04:"xmodel",0x05:"material",0x08:"techset",0x13:"vehicle_track",0x14:"map_ents",0x28:"rawfile",0x29:"scriptfile"}
MAX_TAGS=100_000
MAX_ASSETS=2_000_000

class PoolError(ValueError):
    """Raised when the zone pool cannot be located safely."""

def inspect_pool(zone:bytes)->dict[str,Any]:
    if len(zone)<0x38:
        raise PoolError("zone is too small for an IW6 XFile header")
    tag_count=struct.unpack_from(">I",zone,0x28)[0]
    asset_count=struct.unpack_from(">I",zone,0x30)[0]
    if tag_count>MAX_TAGS or asset_count>MAX_ASSETS:
        raise PoolError("zone header count exceeds safety limits")
    tags=[]
    after_tags=None
    probe=[]
    if tag_count==0:
        pool_offset=0x38
    else:
        strings_start=0x3C+tag_count*4
        if strings_start>len(zone):
            raise PoolError("tag pointer placeholder table is outside the zone")
        offset=strings_start
        for _ in range(tag_count):
            end=zone.find(b"\0",offset)
            if end<0:
                raise PoolError("unterminated zone tag string")
            tags.append(zone[offset:end].decode("ascii","replace"))
            offset=end+1
        after_tags=offset
        best=None
        # Retail zones have a small variable trailer between tag strings and pool.
        # Locate the longest run of plausible big-endian 32-bit type IDs.
        for delta in range(33):
            candidate=offset+delta
            if candidate+asset_count*8>len(zone):
                continue
            valid=0
            leading=0
            in_leading=True
            for i in range(asset_count):
                type_word=struct.unpack_from(">I",zone,candidate+i*8+4)[0]
                ok=type_word<=0x35
                if ok:
                    valid+=1
                if in_leading and ok:
                    leading+=1
                else:
                    in_leading=False
            row={"delta":delta,"valid_records":valid,"leading_valid_records":leading}
            probe.append(row)
            score=(valid,leading,-delta)
            if best is None or score>best[0]:
                best=(score,candidate)
        if best is None or best[0][0]<max(1,asset_count-4):
            raise PoolError("could not locate a consistent asset pool in the 32-byte trailer probe")
        pool_offset=best[1]
    if pool_offset+asset_count*8>len(zone):
        raise PoolError("asset pool extends beyond zone bounds")
    counts=Counter()
    records=[]
    sentinels=[]
    for i in range(asset_count):
        off=pool_offset+i*8
        pointer=struct.unpack_from(">I",zone,off)[0]
        type_word=struct.unpack_from(">I",zone,off+4)[0]
        valid=type_word<=0x35
        name=KNOWN_TYPES.get(type_word,f"unknown_0x{type_word:X}") if valid else "sentinel_or_invalid"
        if valid:
            counts[f"0x{type_word:02X}:{name}"]+=1
        row={"index":i,"pool_offset":off,"pointer":f"0x{pointer:08X}","type_word":f"0x{type_word:08X}","type_id":type_word if valid else None,"type":name,"valid":valid}
        records.append(row)
        if not valid:
            sentinels.append(row)
    map_ents=[r for r in records if r["type_id"]==0x14]
    scriptfiles=[r for r in records if r["type_id"]==0x29]
    rawfiles=[r for r in records if r["type_id"]==0x28]
    return {
        "zone_size":len(zone),"tag_count":tag_count,"tags":tags,
        "asset_count_header":asset_count,"valid_record_count":sum(counts.values()),
        "sentinel_or_invalid_count":len(sentinels),"after_tags_offset":after_tags,
        "pool_offset":pool_offset,"pool_trailer_bytes":pool_offset-after_tags if after_tags is not None else 0,
        "pool_probe":probe,"type_counts":dict(counts),
        "scriptfile_count_type_0x29":len(scriptfiles),"rawfile_count_type_0x28":len(rawfiles),
        "map_ents_count_type_0x14":len(map_ents),"map_ents_entries":map_ents,
        "vehicle_track_entries":[r for r in records if r["type_id"]==0x13],
        "sentinel_entries":sentinels,
    }

def main(argv:list[str]|None=None)->int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("zone",type=Path,help="outer-decompressed raw .zone")
    parser.add_argument("--report-json",type=Path)
    parser.add_argument("--records-csv",type=Path,help="optional complete 8-byte asset-pool inventory")
    args=parser.parse_args(argv)
    try:
        zone=args.zone.read_bytes()
        report=inspect_pool(zone)
        rendered=json.dumps(report,indent=2,sort_keys=True)
        print(rendered)
        if args.report_json:
            args.report_json.parent.mkdir(parents=True,exist_ok=True)
            args.report_json.write_text(rendered+"\n",encoding="utf-8")
        if args.records_csv:
            args.records_csv.parent.mkdir(parents=True,exist_ok=True)
            with args.records_csv.open("w",newline="",encoding="utf-8") as f:
                writer=csv.DictWriter(f,fieldnames=["index","pool_offset","pointer","type_word","type_id","type","valid"])
                writer.writeheader()
                for i in range(report["asset_count_header"]):
                    off=report["pool_offset"]+i*8
                    pointer=struct.unpack_from(">I",zone,off)[0]
                    type_word=struct.unpack_from(">I",zone,off+4)[0]
                    valid=type_word<=0x35
                    writer.writerow({"index":i,"pool_offset":off,"pointer":f"0x{pointer:08X}","type_word":f"0x{type_word:08X}","type_id":type_word if valid else "","type":KNOWN_TYPES.get(type_word,f"unknown_0x{type_word:X}") if valid else "sentinel_or_invalid","valid":valid})
        return 0
    except (OSError,PoolError,ValueError) as exc:
        print(f"error: {exc}",file=sys.stderr)
        return 1

if __name__=="__main__":
    raise SystemExit(main())
