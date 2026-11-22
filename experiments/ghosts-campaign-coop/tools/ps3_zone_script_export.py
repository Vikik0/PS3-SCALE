#!/usr/bin/env python3
"""Export validated Ghosts PS3 scriptfile/rawfile assets from an outer-decompressed IW6 zone.

Input is the raw .zone written by ps3_fastfile_inspect.py --output-raw-zone:
outer 64 KiB blocks are expanded, but per-asset zlib bodies remain compressed.
Long assets are converted to GSC Tool's GSC\0 wrapper; short assets are exported
as rawfiles. This is read-only and does not rebuild a retail FastFile.
"""
from __future__ import annotations
import argparse,hashlib,json,re,struct,sys,zlib
from dataclasses import asdict,dataclass
from pathlib import Path,PurePosixPath
from typing import Any

FF4=b"\xff"*4
FF8=b"\xff"*8
MAX_COMPRESSED_LEN=0x4000000
MAX_DECOMPRESSED_LEN=0x4000000
MAX_NAME_LEN=127

class ZoneScanError(ValueError):
    """Raised when a zone or asset cannot be scanned safely."""

@dataclass(frozen=True)
class AssetRecord:
    header_offset:int
    body_offset:int
    name:str
    shape:str
    compressed_len:int
    decompressed_len:int
    bytecode_len:int
    kind:str
    output_name:str
    output_size:int
    sha256:str

def _read_name(data:bytes,start:int)->tuple[str,int]|None:
    end=data.find(b"\0",start,min(len(data),start+MAX_NAME_LEN+1))
    if end<=start:
        return None
    raw=data[start:end]
    if any(x<0x20 or x>0x7e for x in raw):
        return None
    try:
        name=raw.decode("ascii")
    except UnicodeDecodeError:
        return None
    normalized=name.replace("\\","/")
    if not normalized or normalized.startswith("/") or ".." in PurePosixPath(normalized).parts or re.match(r"^[A-Za-z]:",name):
        return None
    return name,end

def _parse_header(data:bytes,offset:int):
    if offset<0 or offset+16>len(data) or data[offset:offset+4]!=FF4:
        return None
    comp=int.from_bytes(data[offset+4:offset+8],"big")
    dec=int.from_bytes(data[offset+8:offset+12],"big")
    if not (0<comp<MAX_COMPRESSED_LEN and 0<dec<MAX_DECOMPRESSED_LEN):
        return None
    # Long shape: [FF*4][compressedLen][stackLen][bytecodeLen][FF*8][name]\0[compressed stack][bytecode].
    if offset+24<=len(data) and data[offset+16:offset+24]==FF8:
        bytecode_len=int.from_bytes(data[offset+12:offset+16],"big")
        if bytecode_len>MAX_DECOMPRESSED_LEN:
            return None
        parsed=_read_name(data,offset+24)
        if parsed:
            name,name_end=parsed
            body_start=name_end+1
            if body_start+comp+bytecode_len<=len(data):
                return "long",comp,dec,bytecode_len,name,body_start
    # Short shape: [FF*4][compressedLen][decompressedLen][FF*4][name]\0[zlib payload].
    if data[offset+12:offset+16]==FF4:
        parsed=_read_name(data,offset+16)
        if parsed:
            name,name_end=parsed
            body_start=name_end+1
            if body_start+comp<=len(data):
                return "short",comp,dec,0,name,body_start
    return None

def _inflate_exact(payload:bytes,expected:int)->bytes|None:
    decoder=zlib.decompressobj()
    try:
        output=decoder.decompress(payload,expected+1)
    except zlib.error:
        return None
    if len(output)!=expected or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        return None
    return output

def _gsc_tool_wrapper(compressed_stack:bytes,stack_len:int,bytecode:bytes)->bytes:
    """Build the documented GSC Tool ScriptFile wrapper from the raw FF asset."""
    wrapper=b"GSC\0"+struct.pack("<III",len(compressed_stack),stack_len,len(bytecode))+compressed_stack+bytecode
    if len(wrapper)!=16+len(compressed_stack)+len(bytecode):
        raise ZoneScanError("internal GSC wrapper length mismatch")
    compressed_len,stored_stack_len,bytecode_len=struct.unpack_from("<III",wrapper,4)
    if compressed_len!=len(compressed_stack) or stored_stack_len!=stack_len or bytecode_len!=len(bytecode):
        raise ZoneScanError("internal GSC wrapper header mismatch")
    if _inflate_exact(wrapper[16:16+compressed_len],stored_stack_len) is None:
        raise ZoneScanError("reconstructed GSC stack failed zlib round-trip")
    return wrapper

def scan_zone(data:bytes)->list[tuple[AssetRecord,bytes]]:
    """Find long compiled-script candidates and short rawfile assets."""
    if len(data)<0x40:
        raise ZoneScanError("zone is too small to contain an IW6 XFile header")
    found=[]
    offset=0x40
    while offset<=len(data)-16:
        offset=data.find(FF4,offset)
        if offset<0:
            break
        parsed=_parse_header(data,offset)
        if parsed is None:
            offset+=1
            continue
        shape,comp,dec,bytecode_len,name,body_start=parsed
        compressed_stack=data[body_start:body_start+comp]
        stack=_inflate_exact(compressed_stack,dec)
        if stack is None:
            offset+=1
            continue
        if shape=="long":
            bytecode=data[body_start+comp:body_start+comp+bytecode_len]
            payload=_gsc_tool_wrapper(compressed_stack,dec,bytecode)
            suffix=Path(name).suffix.lower()
            if suffix==".csc":
                output_name=str(Path(name).with_suffix(".cscbin"))
            elif suffix==".gsc":
                output_name=str(Path(name).with_suffix(".gscbin"))
            elif suffix in (".gscbin",".cscbin"):
                output_name=name
            else:
                output_name=name+".gscbin"
            kind="compiled_gsc_candidate"
            body_size=comp+bytecode_len
        else:
            payload=stack
            suffix=Path(name).suffix.lower()
            kind="gsc_source" if suffix in (".gsc",".csc") else "rawfile"
            output_name=name
            body_size=comp
        record=AssetRecord(
            header_offset=offset,body_offset=body_start,name=name,shape=shape,
            compressed_len=comp,decompressed_len=dec,bytecode_len=bytecode_len,
            kind=kind,output_name=output_name,output_size=len(payload),
            sha256=hashlib.sha256(payload).hexdigest(),
        )
        found.append((record,payload))
        # Skip the validated compressed stack and, for long assets, bytecode.
        offset=body_start+body_size
    return found

def _safe_target(root:Path,name:str,used:set[str],offset:int)->Path:
    normalized=name.replace("\\","/")
    parts=[part for part in PurePosixPath(normalized).parts if part not in ("",".")]
    if not parts or any(part==".." for part in parts):
        raise ZoneScanError(f"unsafe asset path rejected: {name!r}")
    target=root.joinpath(*parts)
    try:
        target.resolve().relative_to(root.resolve())
    except ValueError as exc:
        raise ZoneScanError(f"asset path escapes output directory: {name!r}") from exc
    key=str(target.relative_to(root)).lower()
    if key in used or target.exists():
        target=target.with_name(f"{target.stem}_{offset:08X}{target.suffix}")
        key=str(target.relative_to(root)).lower()
    used.add(key)
    return target

def export_zone(zone_path:Path,output_dir:Path)->dict[str,Any]:
    try:
        data=zone_path.read_bytes()
    except OSError as exc:
        raise ZoneScanError(f"cannot read zone: {exc}") from exc
    assets=scan_zone(data)
    used:set[str]=set()
    entries=[]
    for record,payload in assets:
        category="rawfiles" if record.kind in ("rawfile","gsc_source") else "gscbin"
        root=output_dir/category
        target=_safe_target(root,record.output_name,used,record.header_offset)
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(payload)
        row=asdict(record)
        row["exported_path"]=str(target)
        entries.append(row)
    return {
        "zone":str(zone_path),
        "zone_size":len(data),
        "asset_count":len(entries),
        "compiled_gsc_candidates":sum(row["kind"]=="compiled_gsc_candidate" for row in entries),
        "rawfile_count":sum(row["kind"]=="rawfile" for row in entries),
        "gsc_source_count":sum(row["kind"]=="gsc_source" for row in entries),
        "assets":entries,
    }

def main(argv:list[str]|None=None)->int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("zone",type=Path,help="outer-decompressed raw .zone; inner asset zlib streams remain intact")
    parser.add_argument("output_dir",type=Path,help="directory for GSC Tool .gscbin candidates and rawfiles")
    parser.add_argument("--report-json",type=Path,help="write the asset inventory as JSON")
    args=parser.parse_args(argv)
    try:
        report=export_zone(args.zone,args.output_dir)
        rendered=json.dumps(report,indent=2,sort_keys=True)
        print(rendered)
        if args.report_json:
            args.report_json.parent.mkdir(parents=True,exist_ok=True)
            args.report_json.write_text(rendered+"\n",encoding="utf-8")
        return 0
    except (ZoneScanError,OSError,ValueError) as exc:
        print(f"error: {exc}",file=sys.stderr)
        return 1

if __name__=="__main__":
    raise SystemExit(main())
