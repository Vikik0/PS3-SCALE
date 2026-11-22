#!/usr/bin/env python3
"""Experimental read-only IW6 PS3 MapEnts/entity-string and spawn-list candidate scanner.

The 32-bit PS3 layout is based on the retail IW6 PS3 MapConverter research:
MapEnts header 0x80 bytes, entity-string length at +8, entity text at +0x80,
SpawnList count at +0x70, and 0x20-byte SpawnPointEntityRecord entries.
This is a candidate scanner, not a generic MapEnts decoder.
"""
from __future__ import annotations
import argparse,csv,hashlib,json,math,re,struct,sys
from collections import Counter
from pathlib import Path
from typing import Any

HEADER_SIZE=128
MAX_ENTITY_CHARS=16*1024*1024
MAX_SPAWNS=8192
FIELD_NAMES={97:"classname",47:"angles",430:"origin",392:"model",640:"target",642:"targetname",158:"dmg",542:"spawnflags",476:"radius",272:"height",27614:"script_gameobjectname",27658:"script_label",27477:"script_bombmode_original",27562:"script_exploder",27417:"script_accel",27428:"script_airspeed",755:"script_delay"}
SPAWN_CLASSES={"info_player_start","mp_dm_spawn","mp_tdm_spawn","mp_tdm_spawn_allies_start","mp_tdm_spawn_axis_start","mp_dom_spawn","mp_dom_spawn_allies_start","mp_dom_spawn_axis_start","mp_sd_spawn_attacker","mp_sd_spawn_defender","mp_global_intermission"}

class MapEntsError(ValueError):
    """Raised when the PS3 MapEnts candidate scan cannot be performed safely."""

def u16(data:bytes,offset:int)->int:
    return struct.unpack_from(">H",data,offset)[0]

def i32(data:bytes,offset:int)->int:
    return struct.unpack_from(">i",data,offset)[0]

def f32(data:bytes,offset:int)->float:
    return struct.unpack_from(">f",data,offset)[0]

def cstring(data:bytes,offset:int)->tuple[bytes,int]|None:
    end=data.find(b"\0",offset)
    if end<0:
        return None
    return data[offset:end],end+1

def parse_script_strings(zone:bytes)->dict[str,Any]:
    """Decode the PS3 inline script-string table: 0 = empty, -1 = next C string."""
    if len(zone)<0x3c:
        raise MapEntsError("zone too small for script-string table")
    count=i32(zone,0x28)
    if count<0 or count>100_000:
        raise MapEntsError(f"invalid script-string count {count}")
    table=0x3c
    cursor=table+count*4
    if cursor>len(zone):
        raise MapEntsError("script-string pointer table outside zone")
    strings=[]
    unresolved=[]
    for index in range(count):
        pointer=i32(zone,table+index*4)
        if pointer==0:
            strings.append("")
        elif pointer==-1:
            item=cstring(zone,cursor)
            if item is None:
                raise MapEntsError(f"unterminated script string at {cursor:#x}")
            raw,cursor=item
            strings.append(raw.decode("ascii","replace"))
        else:
            unresolved.append({"index":index,"pointer":pointer})
            strings.append("")
    if unresolved:
        raise MapEntsError(f"script-string table contains {len(unresolved)} non-inline pointers")
    return {"count":count,"strings":strings,"strings_end":cursor}

def parse_entity_text(text:bytes)->dict[str,Any]:
    decoded=text.decode("ascii","replace")
    blocks=re.findall(r"\{\n([^{}]*)\}",decoded)
    entities=[]
    for index,block in enumerate(blocks):
        fields={int(match.group(1)):match.group(2) for match in re.finditer(r'(?m)^(\d+) "([^"\r\n]*)"$',block)}
        mapped={FIELD_NAMES[key]:value for key,value in fields.items() if key in FIELD_NAMES}
        if mapped:
            mapped["entity_index"]=index
            entities.append(mapped)
    classes=Counter(entity.get("classname","<missing>") for entity in entities)
    spawn_entities=[entity for entity in entities if entity.get("classname") in SPAWN_CLASSES or "spawn" in entity.get("classname","")]
    return {"entity_block_count":len(blocks),"parsed_entity_count":len(entities),"classname_counts":dict(classes),"spawn_like_entities":spawn_entities,"entities":entities}

def validate_spawn_table(zone:bytes,offset:int,count:int,strings:list[str])->list[dict[str,Any]]|None:
    if count<=0 or count>MAX_SPAWNS or offset<0 or offset+count*32>len(zone):
        return None
    rows=[]
    for index in range(count):
        pos=offset+index*32
        record_index=u16(zone,pos)
        name_index=u16(zone,pos+2)
        target_index=u16(zone,pos+4)
        note_index=u16(zone,pos+6)
        if record_index!=index or max(name_index,target_index,note_index)>=len(strings):
            return None
        values=[f32(zone,pos+8+j*4) for j in range(6)]
        if not all(math.isfinite(value) for value in values):
            return None
        if any(abs(value)>100000 for value in values[:3]) or any(abs(value)>3600 for value in values[3:]):
            return None
        if not strings[name_index]:
            return None
        rows.append({"index":record_index,"classname":strings[name_index],"target":strings[target_index],"script_noteworthy":strings[note_index],"origin":values[:3],"angles":values[3:]})
    return rows

def find_spawn_table(zone:bytes,count:int,strings:list[str],hint:int|None=None)->list[dict[str,Any]]:
    if count<=0:
        return []
    if hint is not None:
        hinted=validate_spawn_table(zone,hint,count,strings)
        if hinted is not None:
            return hinted
    candidates=[]
    pos=0
    while True:
        pos=zone.find(b"\0\0",pos)
        if pos<0:
            break
        if pos+count*32<=len(zone) and u16(zone,pos)==0:
            rows=validate_spawn_table(zone,pos,count,strings)
            if rows is not None:
                candidates.append((pos,rows))
        pos+=1
        if len(candidates)>100:
            break
    if len(candidates)==1:
        return candidates[0][1]
    return []

def find_mapents(zone:bytes,script_strings:dict[str,Any])->list[dict[str,Any]]:
    strings=script_strings["strings"]
    candidates=[]
    pos=HEADER_SIZE
    while True:
        pos=zone.find(b"{\n",pos)
        if pos<0:
            break
        header=pos-HEADER_SIZE
        if header>=0 and header+HEADER_SIZE<=len(zone):
            num_chars=i32(zone,header+8)
            if 0<num_chars<=MAX_ENTITY_CHARS and pos+num_chars<=len(zone):
                end=zone.find(b"\0",pos,min(len(zone),pos+num_chars+2))
                if end>=0 and end+1-pos==num_chars:
                    text=zone[pos:end]
                    if b'97 "' in text and b'430 "' in text and text.count(b"{\n")>=5:
                        spawn_count=u16(zone,header+112)
                        spawn_pointer=i32(zone,header+116)
                        parsed=parse_entity_text(text)
                        spawn_rows=find_spawn_table(zone,spawn_count,strings,spawn_pointer if 0<=spawn_pointer<len(zone) else None)
                        candidates.append({
                            "mapents_header_offset":header,
                            "entity_string_offset":pos,
                            "num_entity_chars":num_chars,
                            "entity_text_sha256":hashlib.sha256(text).hexdigest(),
                            "entity_text_prefix":text[:96].decode("ascii","replace"),
                            "spawn_count":spawn_count,
                            "spawn_list_pointer_field":f"0x{spawn_pointer & 0xffffffff:08X}",
                            "spawn_table_found":len(spawn_rows)==spawn_count and spawn_count>0,
                            "spawn_records":spawn_rows,
                            **parsed,
                        })
        pos+=2
    return candidates

def main(argv:list[str]|None=None)->int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("zone",type=Path,help="fully inner-inflated zone from ps3_fastfile_inspect.py --output-zone")
    parser.add_argument("--report-json",type=Path)
    parser.add_argument("--spawn-csv",type=Path)
    args=parser.parse_args(argv)
    try:
        zone=args.zone.read_bytes()
        script_strings=parse_script_strings(zone)
        candidates=find_mapents(zone,script_strings)
        report={"zone":str(args.zone),"zone_size":len(zone),"script_string_count":script_strings["count"],"script_strings_end":script_strings["strings_end"],"mapents_candidate_count":len(candidates),"candidates":candidates}
        rendered=json.dumps(report,indent=2,ensure_ascii=False,sort_keys=True)
        print(rendered)
        if args.report_json:
            args.report_json.parent.mkdir(parents=True,exist_ok=True)
            args.report_json.write_text(rendered+"\n",encoding="utf-8")
        if args.spawn_csv:
            args.spawn_csv.parent.mkdir(parents=True,exist_ok=True)
            with args.spawn_csv.open("w",newline="",encoding="utf-8") as f:
                writer=csv.DictWriter(f,fieldnames=["mapents_header_offset","index","classname","target","script_noteworthy","origin","angles"])
                writer.writeheader()
                for candidate in candidates:
                    for row in candidate["spawn_records"]:
                        writer.writerow({"mapents_header_offset":hex(candidate["mapents_header_offset"]),"index":row["index"],"classname":row["classname"],"target":row["target"],"script_noteworthy":row["script_noteworthy"],"origin":" ".join(map(str,row["origin"])),"angles":" ".join(map(str,row["angles"]))})
        return 0
    except (OSError,MapEntsError,ValueError,struct.error) as exc:
        print(f"error: {exc}",file=sys.stderr)
        return 1

if __name__=="__main__":
    raise SystemExit(main())
