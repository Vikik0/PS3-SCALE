#!/usr/bin/env python3
"""Compare Ghidra xrefs to IW6 split-screen builtins across Campaign/Multiplayer."""
from __future__ import annotations
import argparse, csv, hashlib, json, re, sys
from collections import defaultdict
from pathlib import Path

XREF_FIELDS = {"program","builtin","string_address","reference_address","reference_type","source_type","caller_name","caller_entry","caller_body_start","caller_body_end","instruction"}
INVENTORY_FIELDS = {"name","entry","fingerprint"}
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")

class BuiltinXrefError(ValueError):
    """Malformed Ghidra xref or PPU inventory input."""

def address_key(value, required=False):
    token = (value or "").strip()
    if not token:
        if required: raise BuiltinXrefError("required address is empty")
        return None
    if ":" in token: token = token.rsplit(":", 1)[-1]
    if token.lower().startswith("0x"): token = token[2:]
    if not re.fullmatch(r"[0-9a-fA-F]+", token):
        raise BuiltinXrefError(f"invalid hexadecimal address: {value!r}")
    return f"{int(token, 16):016x}"

def read_inventory(path):
    path = Path(path)
    if not path.is_file(): raise BuiltinXrefError(f"function inventory does not exist: {path}")
    with path.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        missing = sorted(INVENTORY_FIELDS - set(reader.fieldnames or []))
        if missing: raise BuiltinXrefError(f"{path}: function inventory missing columns: {', '.join(missing)}")
        result = {}
        for line, row in enumerate(reader, 2):
            entry = address_key(row.get("entry", ""), True)
            name, fingerprint = (row.get("name") or "").strip(), (row.get("fingerprint") or "").strip().lower()
            if not name: raise BuiltinXrefError(f"{path}:{line}: function name is empty")
            if not SHA256_RE.fullmatch(fingerprint): raise BuiltinXrefError(f"{path}:{line}: invalid function fingerprint")
            if entry in result: raise BuiltinXrefError(f"{path}:{line}: duplicate function entry {row.get('entry')}")
            result[entry] = {"name": name, "entry": row["entry"].strip(), "fingerprint": fingerprint}
    if not result: raise BuiltinXrefError(f"{path}: function inventory contains no records")
    return result

def read_xrefs(path, inventory):
    path = Path(path)
    if not path.is_file(): raise BuiltinXrefError(f"Ghidra xref CSV does not exist: {path}")
    with path.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        missing = sorted(XREF_FIELDS - set(reader.fieldnames or []))
        if missing: raise BuiltinXrefError(f"{path}: Ghidra xref CSV missing columns: {', '.join(missing)}")
        rows, seen = [], set()
        for line, raw in enumerate(reader, 2):
            builtin, program = (raw.get("builtin") or "").strip(), (raw.get("program") or "").strip()
            string_addr = address_key(raw.get("string_address", ""), True)
            ref_addr = address_key(raw.get("reference_address", ""))
            caller_addr = address_key(raw.get("caller_entry", ""))
            if not builtin or not program: raise BuiltinXrefError(f"{path}:{line}: builtin and program are required")
            key = (builtin, string_addr, ref_addr or "")
            if key in seen: raise BuiltinXrefError(f"{path}:{line}: duplicate xref row for {builtin} at {ref_addr}")
            seen.add(key)
            fn = inventory.get(caller_addr or "")
            rows.append({
                "program": program, "builtin": builtin, "string_address": raw["string_address"].strip(),
                "reference_address": raw.get("reference_address","").strip(),
                "reference_type": (raw.get("reference_type") or "").strip(),
                "source_type": (raw.get("source_type") or "").strip(),
                "caller_name": (raw.get("caller_name") or "").strip(),
                "caller_entry": raw.get("caller_entry","").strip(),
                "caller_entry_key": caller_addr, "caller_fingerprint": fn["fingerprint"] if fn else None,
                "caller_inventory_name": fn["name"] if fn else None,
                "caller_inventory_match": fn is not None,
                "reference_kind": "function_xref" if caller_addr else ("data_or_unmapped_xref" if ref_addr else "string_without_recorded_xref")
            })
    return rows

def caller_summary(rows):
    grouped = {}
    for row in rows:
        fp = row["caller_fingerprint"]
        if not fp: continue
        key = (fp, row["caller_entry_key"])
        item = grouped.setdefault(key, {"name": row["caller_inventory_name"], "entry": row["caller_entry"], "fingerprint": fp, "xref_count": 0})
        item["xref_count"] += 1
    return sorted(grouped.values(), key=lambda x: (x["name"].lower(), x["entry"]))

def compare(campaign_xrefs, multiplayer_xrefs, campaign_inventory, multiplayer_inventory, input_paths=None):
    by_mode = []
    for rows in (campaign_xrefs, multiplayer_xrefs):
        grouped = defaultdict(list)
        for row in rows: grouped[row["builtin"]].append(row)
        by_mode.append(grouped)
    c_by, m_by = by_mode
    reports, leads = [], []
    for builtin in sorted(set(c_by) | set(m_by)):
        crows, mrows = c_by.get(builtin, []), m_by.get(builtin, [])
        ccallers, mcallers = caller_summary(crows), caller_summary(mrows)
        cfp = {x["fingerprint"] for x in ccallers}
        mfp = {x["fingerprint"] for x in mcallers}
        shared, mp_only = sorted(cfp & mfp), sorted(mfp - cfp)
        item = {
            "builtin": builtin,
            "campaign": {"xref_rows":len(crows),"function_xref_rows":sum(x["reference_kind"]=="function_xref" for x in crows),"data_or_unmapped_xref_rows":sum(x["reference_kind"]!="function_xref" for x in crows),"caller_inventory_misses":sum(x["caller_entry_key"] is not None and not x["caller_inventory_match"] for x in crows),"callers":ccallers},
            "multiplayer": {"xref_rows":len(mrows),"function_xref_rows":sum(x["reference_kind"]=="function_xref" for x in mrows),"data_or_unmapped_xref_rows":sum(x["reference_kind"]!="function_xref" for x in mrows),"caller_inventory_misses":sum(x["caller_entry_key"] is not None and not x["caller_inventory_match"] for x in mrows),"callers":mcallers},
            "shared_caller_fingerprints":shared,"multiplayer_only_caller_fingerprints":mp_only,
            "campaign_only_caller_fingerprints":sorted(cfp-mfp),
            "interpretation":"A string xref may be a registration/data reference, not an executable call; inspect the caller and surrounding disassembly in Ghidra."
        }
        reports.append(item)
        if mrows and (not crows or mp_only):
            leads.append({
                "builtin":builtin,"campaign_xref_rows":len(crows),"multiplayer_xref_rows":len(mrows),
                "multiplayer_only_caller_count":len(mp_only),
                "multiplayer_only_callers":[x for x in mcallers if x["fingerprint"] in set(mp_only)],
                "shared_caller_count":len(shared),
                "priority":"MP_ONLY_BUILTIN_XREF" if not crows else "MP_ONLY_CALLER_FINGERPRINT"
            })
    leads.sort(key=lambda x:(0 if x["priority"]=="MP_ONLY_BUILTIN_XREF" else 1,-x["multiplayer_only_caller_count"],-x["multiplayer_xref_rows"],x["builtin"].lower()))
    result = {
        "schema_version":1,"status":"SPLITSCREEN_BUILTIN_XREF_COMPARISON_COMPLETED",
        "campaign":{"xref_rows":len(campaign_xrefs),"inventory_functions":len(campaign_inventory),"distinct_builtins":len(c_by),"caller_inventory_misses":sum(x["caller_entry_key"] is not None and not x["caller_inventory_match"] for x in campaign_xrefs)},
        "multiplayer":{"xref_rows":len(multiplayer_xrefs),"inventory_functions":len(multiplayer_inventory),"distinct_builtins":len(m_by),"caller_inventory_misses":sum(x["caller_entry_key"] is not None and not x["caller_inventory_match"] for x in multiplayer_xrefs)},
        "builtin_comparisons":reports,"multiplayer_only_leads":leads,
        "limitations":[
            "The tool joins Ghidra string xrefs to same-build function inventory entries; it does not resolve indirect calls.",
            "A string reference may point to a command-registration table or other data, not to an executable call site.",
            "Matching instruction fingerprints are navigation evidence, not proof of identical semantics or runtime behavior.",
            "No address is approved as a hook; verify exact executable, function boundary, TOC, calling convention, globals and lifecycle.",
            "A Multiplayer-only builtin reference does not prove Campaign can initialize a second local client."
        ]
    }
    if input_paths:
        result["input_sha256"] = {key:hashlib.sha256(Path(path).read_bytes()).hexdigest() for key,path in sorted(input_paths.items())}
    return result

def compare_paths(cx, mx, ci, mi):
    c_inv, m_inv = read_inventory(ci), read_inventory(mi)
    c_x, m_x = read_xrefs(cx,c_inv), read_xrefs(mx,m_inv)
    return compare(c_x,m_x,c_inv,m_inv,{"campaign_xrefs":cx,"multiplayer_xrefs":mx,"campaign_inventory":ci,"multiplayer_inventory":mi})

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--campaign-xrefs", required=True, type=Path)
    parser.add_argument("--multiplayer-xrefs", required=True, type=Path)
    parser.add_argument("--campaign-inventory", required=True, type=Path)
    parser.add_argument("--multiplayer-inventory", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        report = compare_paths(args.campaign_xrefs,args.multiplayer_xrefs,args.campaign_inventory,args.multiplayer_inventory)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2,sort_keys=True)+"\n",encoding="utf-8")
        print(json.dumps({"status":report["status"],"campaign":report["campaign"],"multiplayer":report["multiplayer"],"multiplayer_only_leads":report["multiplayer_only_leads"]},ensure_ascii=False,indent=2))
        return 0
    except (OSError,BuiltinXrefError,ValueError) as exc:
        print(f"error: {exc}",file=sys.stderr)
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
