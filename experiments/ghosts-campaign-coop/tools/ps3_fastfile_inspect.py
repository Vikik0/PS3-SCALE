#!/usr/bin/env python3
"""Read-only inspector and zone extractor for Call of Duty: Ghosts PS3 IW6 FastFiles."""
from __future__ import annotations
import argparse, hashlib, json, struct, sys, zlib
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

OUTER_MAGIC=b"IWff0100"
INNER_MAGIC=b"IWffS100"
IW6_VERSION=0x22E
STREAM_DELTA=0x20000
ZONE_BLOCK_SIZE=0x10000
DEFAULT_MAX_BLOCKS=4096
MAX_MARKER_SCAN=0x40000
MAX_ASSET_STREAM=0x4000000
MAX_INFLATED_ASSET_BYTES=256*1024*1024

class FastFileError(ValueError):
    """Raised when a file is not a supported PS3 Ghosts FastFile."""

@dataclass(frozen=True)
class FastFileReport:
    file_size:int
    file_sha256:str
    outer_magic:str
    version:int
    inner_magic_offset:int
    stream_offset:int
    block_count:int
    block_trailer_size:int
    outer_zone_size:int
    decompressed_size:int
    inner_streams_inflated:int
    raw_zone_sha256:str
    zone_sha256:str
    complete:bool
    stop_reason:str
    trailing_bytes:int
    trailing_sha256:str|None

def _inflate_raw_block(payload:bytes)->tuple[bytes,int]:
    decoder=zlib.decompressobj(wbits=-15)
    try:
        output=decoder.decompress(payload,ZONE_BLOCK_SIZE+1)
    except zlib.error as exc:
        raise FastFileError(f"raw-deflate block is invalid: {exc}") from exc
    if len(output)!=ZONE_BLOCK_SIZE:
        raise FastFileError(f"block expanded to {len(output)} bytes; expected {ZONE_BLOCK_SIZE}")
    if not decoder.eof:
        raise FastFileError("raw-deflate block ended before its end marker")
    if decoder.unconsumed_tail:
        raise FastFileError("raw-deflate block has unconsumed input")
    # Four opaque bytes after raw-DEFLATE were present in every user-supplied
    # retail sample. They are part of the u16 block length, not the FF terminator.
    trailer_size=len(decoder.unused_data)
    if trailer_size not in (0,4):
        raise FastFileError(f"unexpected {trailer_size}-byte raw-deflate block trailer")
    return output,trailer_size

def _asset_header_lengths(zone:bytes,zlib_offset:int)->tuple[int,int]|None:
    p=zlib_offset-1
    if p<0 or zone[p]!=0:
        return None
    p-=1
    name_end=p+1
    while p>0 and 0x20<=zone[p]<=0x7E and (name_end-p)<127:
        p-=1
    name_start=p+1
    if name_end-name_start<1:
        return None
    for trailing_ff,size_bytes in ((8,12),(4,8)):
        if name_start<trailing_ff+size_bytes+4:
            continue
        if zone[name_start-trailing_ff:name_start]!=b"\xff"*trailing_ff:
            continue
        fields=name_start-trailing_ff-size_bytes
        if zone[fields-4:fields]!=b"\xff"*4:
            continue
        comp_len=int.from_bytes(zone[fields:fields+4],"big")
        dec_len=int.from_bytes(zone[fields+4:fields+8],"big")
        if 0<comp_len<MAX_ASSET_STREAM and 0<dec_len<MAX_ASSET_STREAM:
            return comp_len,dec_len
    return None

def _inflate_zone_assets(zone:bytes)->tuple[bytes,int]:
    """Expand validated inline zlib streams while preserving their asset headers."""
    streams=[]
    inflated_total=0
    i=0x40
    while i<len(zone)-2:
        if zone[i]!=0x78 or zone[i+1] not in (0x01,0x5E,0x9C,0xDA):
            i+=1
            continue
        lengths=_asset_header_lengths(zone,i)
        if lengths is None:
            i+=1
            continue
        comp_len,dec_len=lengths
        end=i+comp_len
        if end>len(zone):
            i+=1
            continue
        decoder=zlib.decompressobj(wbits=zlib.MAX_WBITS)
        try:
            content=decoder.decompress(zone[i:end],dec_len+1)
            if len(content)!=dec_len or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
                i+=1
                continue
        except zlib.error:
            i+=1
            continue
        inflated_total+=len(content)
        if inflated_total>MAX_INFLATED_ASSET_BYTES:
            raise FastFileError("inner asset inflation exceeded the 256 MiB safety limit")
        streams.append((i,comp_len,content))
        i=end
    if not streams:
        return zone,0
    rebuilt=bytearray()
    cursor=0
    for start,comp_len,content in streams:
        rebuilt.extend(zone[cursor:start])
        rebuilt.extend(content)
        cursor=start+comp_len
    rebuilt.extend(zone[cursor:])
    return bytes(rebuilt),len(streams)

def inspect_bytes_full(data:bytes,*,max_blocks:int=DEFAULT_MAX_BLOCKS)->tuple[FastFileReport,bytes,bytes]:
    """Return (report, inner-inflated zone, outer-only zone)."""
    if max_blocks<1:
        raise ValueError("max_blocks must be positive")
    if len(data)<12 or data[:8]!=OUTER_MAGIC:
        raise FastFileError("missing PS3 Ghosts outer magic IWff0100")
    version=struct.unpack_from(">I",data,8)[0]
    if version!=IW6_VERSION:
        raise FastFileError(f"unsupported FastFile version 0x{version:08X}; expected IW6 0x{IW6_VERSION:08X}")

    candidates=[]
    search_from=12
    # Clockwork's real marker is at 0x16644, beyond the earlier 64 KiB limit.
    marker_limit=min(len(data)-len(INNER_MAGIC),MAX_MARKER_SCAN)
    while search_from<=marker_limit:
        marker=data.find(INNER_MAGIC,search_from,marker_limit+len(INNER_MAGIC))
        if marker<0:
            break
        search_from=marker+1
        stream_offset=marker+STREAM_DELTA
        if stream_offset+2>len(data):
            continue

        position=stream_offset
        zone_parts=[]
        stop_reason="eof"
        trailing_bytes=0
        trailer_size=None
        while position<len(data):
            remaining=len(data)-position
            if remaining<2:
                stop_reason="truncated_block_length"
                trailing_bytes=remaining
                break
            compressed_size=struct.unpack_from(">H",data,position)[0]
            if compressed_size==1 and data[position+2:position+6]==b"\0"*4:
                # Retail stream terminator: 00 01 followed by exactly four zero bytes.
                position+=2
                stop_reason="end_marker_0x0001"
                trailing_bytes=len(data)-position
                break
            if compressed_size==0:
                position+=2
                stop_reason="zero_terminator"
                trailing_bytes=len(data)-position
                break
            payload_start=position+2
            payload_end=payload_start+compressed_size
            if payload_end>len(data):
                stop_reason="truncated_block_payload"
                trailing_bytes=remaining
                break
            try:
                block,current_trailer=_inflate_raw_block(data[payload_start:payload_end])
            except FastFileError:
                stop_reason="invalid_or_non_deflate_trailing_data"
                trailing_bytes=remaining
                break
            if trailer_size is None:
                trailer_size=current_trailer
            elif current_trailer!=trailer_size:
                stop_reason="inconsistent_block_trailer_size"
                trailing_bytes=remaining
                break
            zone_parts.append(block)
            position=payload_end
            if len(zone_parts)>=max_blocks and position<len(data):
                stop_reason="max_blocks_reached"
                trailing_bytes=len(data)-position
                break

        if not zone_parts:
            continue
        complete=(
            stop_reason=="eof"
            or (stop_reason=="end_marker_0x0001" and trailing_bytes==4)
            or (stop_reason=="zero_terminator" and trailing_bytes==0)
        )
        raw_zone=b"".join(zone_parts)
        zone,inner_streams=_inflate_zone_assets(raw_zone)
        tail=data[position:] if trailing_bytes else b""
        report=FastFileReport(
            file_size=len(data),
            file_sha256=hashlib.sha256(data).hexdigest(),
            outer_magic=OUTER_MAGIC.decode("ascii"),
            version=version,
            inner_magic_offset=marker,
            stream_offset=stream_offset,
            block_count=len(zone_parts),
            block_trailer_size=trailer_size or 0,
            outer_zone_size=len(raw_zone),
            decompressed_size=len(zone),
            inner_streams_inflated=inner_streams,
            raw_zone_sha256=hashlib.sha256(raw_zone).hexdigest(),
            zone_sha256=hashlib.sha256(zone).hexdigest(),
            complete=complete,
            stop_reason=stop_reason,
            trailing_bytes=trailing_bytes,
            trailing_sha256=hashlib.sha256(tail).hexdigest() if tail else None,
        )
        candidates.append((report,zone,raw_zone))

    if not candidates:
        raise FastFileError("IW6 signed-container marker found no valid 64 KiB raw-deflate block stream")
    candidates.sort(key=lambda item:(item[0].block_count,item[0].complete,-item[0].trailing_bytes),reverse=True)
    return candidates[0]

def inspect_bytes(data:bytes,*,max_blocks:int=DEFAULT_MAX_BLOCKS)->tuple[FastFileReport,bytes]:
    report,zone,_=inspect_bytes_full(data,max_blocks=max_blocks)
    return report,zone

def inspect_file_full(path:Path,*,max_blocks:int=DEFAULT_MAX_BLOCKS)->tuple[FastFileReport,bytes,bytes]:
    try:
        data=path.read_bytes()
    except OSError as exc:
        raise FastFileError(f"cannot read input file: {exc}") from exc
    return inspect_bytes_full(data,max_blocks=max_blocks)

def inspect_file(path:Path,*,max_blocks:int=DEFAULT_MAX_BLOCKS)->tuple[FastFileReport,bytes]:
    report,zone,_=inspect_file_full(path,max_blocks=max_blocks)
    return report,zone

def main(argv:list[str]|None=None)->int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fastfile",type=Path,help="user-owned Ghosts PS3 .ff file")
    parser.add_argument("--output-zone",type=Path,help="write the fully inflated zone")
    parser.add_argument("--output-raw-zone",type=Path,help="write outer-decompressed zone with inner asset zlib streams intact")
    parser.add_argument("--allow-unparsed-tail",action="store_true",help="explicitly allow writing the validated blocks while reporting uninterpreted trailing file data")
    parser.add_argument("--report-json",type=Path,help="also write the inspection report as JSON")
    parser.add_argument("--max-blocks",type=int,default=DEFAULT_MAX_BLOCKS)
    args=parser.parse_args(argv)
    try:
        report,zone,raw_zone=inspect_file_full(args.fastfile,max_blocks=args.max_blocks)
        if args.output_zone or args.output_raw_zone:
            if not report.complete and not args.allow_unparsed_tail:
                raise FastFileError(
                    f"refusing to write an incomplete zone ({report.stop_reason}; {report.trailing_bytes} trailing bytes); "
                    "review the report and use --allow-unparsed-tail only if the validated block stream is sufficient"
                )
            paths=[p.resolve() for p in (args.output_zone,args.output_raw_zone) if p]
            if args.fastfile.resolve() in paths:
                raise FastFileError("output path must not overwrite the input FastFile")
            if len(set(paths))!=len(paths):
                raise FastFileError("raw-zone and inflated-zone outputs must use different paths")
            if args.output_zone:
                args.output_zone.parent.mkdir(parents=True,exist_ok=True)
                args.output_zone.write_bytes(zone)
            if args.output_raw_zone:
                args.output_raw_zone.parent.mkdir(parents=True,exist_ok=True)
                args.output_raw_zone.write_bytes(raw_zone)
        payload:dict[str,Any]=asdict(report)
        if args.output_zone:
            payload["output_zone"]=str(args.output_zone)
        if args.output_raw_zone:
            payload["output_raw_zone"]=str(args.output_raw_zone)
        rendered=json.dumps(payload,indent=2,sort_keys=True)
        print(rendered)
        if args.report_json:
            args.report_json.parent.mkdir(parents=True,exist_ok=True)
            args.report_json.write_text(rendered+"\n",encoding="utf-8")
        return 0 if report.complete else 2
    except (FastFileError,OSError,ValueError) as exc:
        print(f"error: {exc}",file=sys.stderr)
        return 1

if __name__=="__main__":
    raise SystemExit(main())
