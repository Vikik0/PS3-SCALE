import csv
import tempfile
import unittest
from pathlib import Path
from tools.compare_splitscreen_builtin_xrefs import BuiltinXrefError, compare, compare_paths, read_inventory, read_xrefs

XF=["program","builtin","string_address","reference_address","reference_type","source_type","caller_name","caller_entry","caller_body_start","caller_body_end","instruction"]
IF=["program","name","entry","body_address_count","instruction_count","fingerprint"]

def write(path, fields, rows):
    with path.open("w",encoding="utf-8",newline="") as stream:
        writer=csv.DictWriter(stream,fieldnames=fields); writer.writeheader(); writer.writerows(rows)

def inv(name,entry,fp):
    return {"program":"Ghosts","name":name,"entry":entry,"body_address_count":"8","instruction_count":"4","fingerprint":fp}

def xref(builtin,saddr,raddr,entry="",name=""):
    return {"program":"Ghosts","builtin":builtin,"string_address":saddr,"reference_address":raddr,"reference_type":"DATA","source_type":"ANALYSIS","caller_name":name,"caller_entry":entry,"caller_body_start":"","caller_body_end":"","instruction":""}

class BuiltinXrefTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(); self.root=Path(self.tmp.name)
        self.ci=self.root/"campaign.csv"; self.mi=self.root/"multiplayer.csv"
        self.cx=self.root/"campaign-xrefs.csv"; self.mx=self.root/"multiplayer-xrefs.csv"
    def tearDown(self): self.tmp.cleanup()

    def test_address_normalization_and_shared_fingerprint(self):
        fp="a"*64
        write(self.ci,IF,[inv("CampaignFn","0x1000",fp)])
        write(self.mi,IF,[inv("MultiplayerFn","ram:00009000",fp)])
        write(self.cx,XF,[xref("isSplitscreenPlayer","0x3000","0x1001","0x1000")])
        write(self.mx,XF,[xref("isSplitscreenPlayer","ram:00004000","ram:00009001","0x9000")])
        result=compare_paths(self.cx,self.mx,self.ci,self.mi)
        self.assertEqual(result["builtin_comparisons"][0]["shared_caller_fingerprints"],[fp])
        self.assertEqual(result["multiplayer_only_leads"],[])

    def test_multiplayer_only_caller_is_reported_by_fingerprint(self):
        cfp,mfp="a"*64,"b"*64
        ci={"0000000000001000":{"name":"campaign","entry":"0x1000","fingerprint":cfp}}
        mi={"0000000000009000":{"name":"SV_InitLocalClient","entry":"0x9000","fingerprint":mfp}}
        cx=[{"builtin":"isSplitScreen","reference_kind":"function_xref","caller_fingerprint":cfp,"caller_inventory_match":True,"caller_entry_key":"0000000000001000","caller_inventory_name":"campaign","caller_entry":"0x1000"}]
        mx=[{"builtin":"isSplitScreen","reference_kind":"function_xref","caller_fingerprint":mfp,"caller_inventory_match":True,"caller_entry_key":"0000000000009000","caller_inventory_name":"SV_InitLocalClient","caller_entry":"0x9000"}]
        lead=compare(cx,mx,ci,mi)["multiplayer_only_leads"][0]
        self.assertEqual(lead["priority"],"MP_ONLY_CALLER_FINGERPRINT")
        self.assertEqual(lead["multiplayer_only_callers"][0]["name"],"SV_InitLocalClient")

    def test_mp_only_builtin_does_not_invent_a_caller(self):
        ci={"1":{"name":"c","entry":"1","fingerprint":"a"*64}}
        mi={"2":{"name":"m","entry":"2","fingerprint":"b"*64}}
        mx=[{"builtin":"PlayerCMD_Spawn","reference_kind":"data_or_unmapped_xref","caller_fingerprint":None,"caller_inventory_match":False,"caller_entry_key":None}]
        lead=compare([],mx,ci,mi)["multiplayer_only_leads"][0]
        self.assertEqual(lead["priority"],"MP_ONLY_BUILTIN_XREF")
        self.assertEqual(lead["multiplayer_only_caller_count"],0)

    def test_xref_kinds_are_distinguished(self):
        invs={"0000000000001000":{"name":"caller","entry":"0x1000","fingerprint":"a"*64}}
        path=self.root/"x.csv"
        write(path,XF,[xref("isSplitScreen","0x3000","0x1001","0x1000"),xref("PlayerCMD_Spawn","0x4000","0x5000"),xref("Clone","0x6000","")])
        rows=read_xrefs(path,invs)
        self.assertEqual([r["reference_kind"] for r in rows],["function_xref","data_or_unmapped_xref","string_without_recorded_xref"])

    def test_unknown_caller_is_not_guessed(self):
        invs={"0000000000001000":{"name":"known","entry":"0x1000","fingerprint":"a"*64}}
        path=self.root/"x.csv"; write(path,XF,[xref("isSplitScreen","0x3000","0x9001","0x9000","unknown")])
        row=read_xrefs(path,invs)[0]
        self.assertFalse(row["caller_inventory_match"]); self.assertIsNone(row["caller_fingerprint"])

    def test_duplicate_inventory_entry_rejected(self):
        path=self.root/"i.csv"; write(path,IF,[inv("one","0x1000","a"*64),inv("two","ram:00001000","b"*64)])
        with self.assertRaisesRegex(BuiltinXrefError,"duplicate function entry"): read_inventory(path)

    def test_duplicate_xref_rejected(self):
        invs={"0000000000001000":{"name":"c","entry":"0x1000","fingerprint":"a"*64}}
        path=self.root/"x.csv"; row=xref("isSplitScreen","0x3000","0x1001","0x1000")
        write(path,XF,[row,row])
        with self.assertRaisesRegex(BuiltinXrefError,"duplicate xref row"): read_xrefs(path,invs)

    def test_bad_columns_and_addresses_rejected(self):
        invs={}
        path=self.root/"x.csv"; write(path,["program","builtin"],[{"program":"Ghosts","builtin":"isSplitScreen"}])
        with self.assertRaisesRegex(BuiltinXrefError,"missing columns"): read_xrefs(path,invs)
        write(path,XF,[xref("isSplitScreen","bad-address","0x1000")])
        with self.assertRaisesRegex(BuiltinXrefError,"invalid hexadecimal address"): read_xrefs(path,invs)

    def test_ghidra_source_exports_references_and_function_context(self):
        source=(Path(__file__).resolve().parents[1]/"tools"/"ghidra"/"FindGhostsSplitScreenBuiltins.java").read_text()
        for token in ("getReferencesTo(stringAddress)","getFunctionContaining(from)","getEntryPoint().toString()","getReferenceType().toString()","caller_body_start","caller_body_end","PlayerCMD_Spawn","CL_InitLocalClients"):
            with self.subTest(token=token): self.assertIn(token,source)
        self.assertIn("A string xref may be a registration/data reference",source)

    def test_hashes_all_four_inputs(self):
        fp="a"*64
        write(self.ci,IF,[inv("c","0x1000",fp)]); write(self.mi,IF,[inv("m","0x9000",fp)])
        write(self.cx,XF,[xref("isSplitScreen","0x3000","0x1001","0x1000")])
        write(self.mx,XF,[xref("isSplitScreen","0x4000","0x9001","0x9000")])
        result=compare_paths(self.cx,self.mx,self.ci,self.mi)
        self.assertEqual(set(result["input_sha256"]),{"campaign_xrefs","multiplayer_xrefs","campaign_inventory","multiplayer_inventory"})

if __name__=="__main__": unittest.main()
