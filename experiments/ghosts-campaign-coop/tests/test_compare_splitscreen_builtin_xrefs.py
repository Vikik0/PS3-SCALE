import csv
import tempfile
import unittest
from pathlib import Path

from tools.compare_splitscreen_builtin_xrefs import (
    BuiltinXrefError,
    compare,
    compare_paths,
    read_xrefs,
)

XF = [
    "program", "builtin", "string_address", "reference_address", "reference_type",
    "is_call", "is_data", "source_type", "caller_name", "caller_entry",
    "caller_body_start", "caller_body_end", "instruction",
]
IF = ["program", "name", "entry", "body_address_count", "instruction_count", "fingerprint"]


def write(path, fields, rows):
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def inv(name, entry, fingerprint):
    return {
        "program": "Ghosts",
        "name": name,
        "entry": entry,
        "body_address_count": "8",
        "instruction_count": "4",
        "fingerprint": fingerprint,
    }


def xref(builtin, string_address, reference_address, entry="", name="", *, is_call=False, is_data=True):
    return {
        "program": "Ghosts",
        "builtin": builtin,
        "string_address": string_address,
        "reference_address": reference_address,
        "reference_type": "UNCONDITIONAL_CALL" if is_call else ("DATA" if is_data else "READ"),
        "is_call": str(is_call).lower(),
        "is_data": str(is_data).lower(),
        "source_type": "ANALYSIS",
        "caller_name": name,
        "caller_entry": entry,
        "caller_body_start": "",
        "caller_body_end": "",
        "instruction": "",
    }


def manual_call_row(fingerprint, entry, name):
    return {
        "builtin": "isSplitScreen",
        "is_call": True,
        "reference_kind": "call_xref",
        "caller_fingerprint": fingerprint,
        "caller_inventory_match": True,
        "caller_entry_key": entry,
        "caller_inventory_name": name,
        "caller_entry": entry,
    }


class BuiltinXrefTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.campaign_inventory = self.root / "campaign.csv"
        self.multiplayer_inventory = self.root / "multiplayer.csv"
        self.campaign_xrefs = self.root / "campaign-xrefs.csv"
        self.multiplayer_xrefs = self.root / "multiplayer-xrefs.csv"

    def tearDown(self):
        self.temp.cleanup()

    def test_address_normalization_and_shared_call_fingerprint(self):
        fingerprint = "a" * 64
        write(self.campaign_inventory, IF, [inv("CampaignFn", "0x1000", fingerprint)])
        write(self.multiplayer_inventory, IF, [inv("MultiplayerFn", "ram:00009000", fingerprint)])
        write(self.campaign_xrefs, XF, [
            xref("isSplitscreenPlayer", "0x3000", "0x1001", "0x1000", is_call=True, is_data=False)
        ])
        write(self.multiplayer_xrefs, XF, [
            xref("isSplitscreenPlayer", "ram:00004000", "ram:00009001", "0x9000", is_call=True, is_data=False)
        ])
        result = compare_paths(
            self.campaign_xrefs, self.multiplayer_xrefs,
            self.campaign_inventory, self.multiplayer_inventory,
        )
        self.assertEqual(result["builtin_comparisons"][0]["shared_caller_fingerprints"], [fingerprint])
        self.assertEqual(result["multiplayer_only_leads"], [])
        self.assertEqual(result["campaign"]["call_xref_rows"], 1)

    def test_multiplayer_only_call_caller_is_reported_by_fingerprint(self):
        campaign_fp, multiplayer_fp = "a" * 64, "b" * 64
        campaign_inventory = {
            "0000000000001000": {"name": "campaign", "entry": "0x1000", "fingerprint": campaign_fp}
        }
        multiplayer_inventory = {
            "0000000000009000": {
                "name": "SV_InitLocalClient", "entry": "0x9000", "fingerprint": multiplayer_fp
            }
        }
        campaign_xrefs = [manual_call_row(campaign_fp, "0000000000001000", "campaign")]
        multiplayer_xrefs = [manual_call_row(multiplayer_fp, "0000000000009000", "SV_InitLocalClient")]
        lead = compare(campaign_xrefs, multiplayer_xrefs, campaign_inventory, multiplayer_inventory)[
            "multiplayer_only_leads"
        ][0]
        self.assertEqual(lead["priority"], "MP_ONLY_CALLER_FINGERPRINT")
        self.assertEqual(lead["multiplayer_only_callers"][0]["name"], "SV_InitLocalClient")

    def test_mp_only_data_reference_does_not_create_a_call_path_lead(self):
        campaign_inventory = {"1": {"name": "c", "entry": "1", "fingerprint": "a" * 64}}
        multiplayer_inventory = {"2": {"name": "m", "entry": "2", "fingerprint": "b" * 64}}
        data_row = {
            "builtin": "PlayerCMD_Spawn",
            "is_call": False,
            "is_data": True,
            "reference_kind": "data_xref",
            "caller_fingerprint": None,
            "caller_inventory_match": False,
            "caller_entry_key": None,
        }
        result = compare([], [data_row], campaign_inventory, multiplayer_inventory)
        self.assertEqual(result["multiplayer_only_leads"], [])
        self.assertEqual(result["multiplayer"]["data_xref_rows"], 1)

    def test_mp_only_call_reference_without_mapped_caller_is_reported_cautiously(self):
        campaign_inventory = {"1": {"name": "c", "entry": "1", "fingerprint": "a" * 64}}
        multiplayer_inventory = {"2": {"name": "m", "entry": "2", "fingerprint": "b" * 64}}
        call_row = {
            "builtin": "PlayerCMD_Spawn",
            "is_call": True,
            "is_data": False,
            "reference_kind": "call_xref",
            "caller_fingerprint": None,
            "caller_inventory_match": False,
            "caller_entry_key": None,
        }
        lead = compare([], [call_row], campaign_inventory, multiplayer_inventory)[
            "multiplayer_only_leads"
        ][0]
        self.assertEqual(lead["priority"], "MP_ONLY_CALL_REFERENCE")
        self.assertEqual(lead["multiplayer_only_caller_count"], 0)

    def test_xref_kinds_distinguish_calls_data_and_unreferenced_strings(self):
        inventory = {
            "0000000000001000": {"name": "caller", "entry": "0x1000", "fingerprint": "a" * 64}
        }
        path = self.root / "xrefs.csv"
        write(path, XF, [
            xref("isSplitScreen", "0x3000", "0x1001", "0x1000", is_call=True, is_data=False),
            xref("PlayerCMD_Spawn", "0x4000", "0x5000", is_call=False, is_data=True),
            xref("Clone", "0x6000", "", is_call=False, is_data=False),
        ])
        rows = read_xrefs(path, inventory)
        self.assertEqual(
            [row["reference_kind"] for row in rows],
            ["call_xref", "data_xref", "string_without_recorded_xref"],
        )

    def test_unknown_call_caller_is_not_guessed(self):
        inventory = {
            "0000000000001000": {"name": "known", "entry": "0x1000", "fingerprint": "a" * 64}
        }
        path = self.root / "xrefs.csv"
        write(path, XF, [
            xref("isSplitScreen", "0x3000", "0x9001", "0x9000", "unknown", is_call=True, is_data=False)
        ])
        row = read_xrefs(path, inventory)[0]
        self.assertFalse(row["caller_inventory_match"])
        self.assertIsNone(row["caller_fingerprint"])
        self.assertTrue(row["is_call"])

    def test_duplicate_inventory_entry_rejected(self):
        path = self.root / "inventory.csv"
        write(path, IF, [
            inv("one", "0x1000", "a" * 64),
            inv("two", "ram:00001000", "b" * 64),
        ])
        # Inventory parsing is also exercised through compare_paths.
        with self.assertRaisesRegex(BuiltinXrefError, "duplicate function entry"):
            from tools.compare_splitscreen_builtin_xrefs import read_inventory
            read_inventory(path)

    def test_duplicate_xref_rejected(self):
        inventory = {
            "0000000000001000": {"name": "caller", "entry": "0x1000", "fingerprint": "a" * 64}
        }
        path = self.root / "xrefs.csv"
        row = xref("isSplitScreen", "0x3000", "0x1001", "0x1000", is_call=True, is_data=False)
        write(path, XF, [row, row])
        with self.assertRaisesRegex(BuiltinXrefError, "duplicate xref row"):
            read_xrefs(path, inventory)

    def test_bad_columns_addresses_and_boolean_flags_are_rejected(self):
        inventory = {}
        path = self.root / "xrefs.csv"
        write(path, ["program", "builtin"], [{"program": "Ghosts", "builtin": "isSplitScreen"}])
        with self.assertRaisesRegex(BuiltinXrefError, "missing columns"):
            read_xrefs(path, inventory)
        write(path, XF, [xref("isSplitScreen", "bad-address", "0x1000")])
        with self.assertRaisesRegex(BuiltinXrefError, "invalid hexadecimal address"):
            read_xrefs(path, inventory)
        bad_flag = xref("isSplitScreen", "0x3000", "0x1001")
        bad_flag["is_call"] = "maybe"
        write(path, XF, [bad_flag])
        with self.assertRaisesRegex(BuiltinXrefError, "is_call must be true or false"):
            read_xrefs(path, inventory)

    def test_conflicting_reference_flags_are_rejected(self):
        inventory = {}
        path = self.root / "xrefs.csv"
        row = xref("isSplitScreen", "0x3000", "0x1001")
        row["is_call"] = "true"
        row["is_data"] = "true"
        write(path, XF, [row])
        with self.assertRaisesRegex(BuiltinXrefError, "both call and data"):
            read_xrefs(path, inventory)

    def test_ghidra_source_exports_reference_type_classification(self):
        source = (
            Path(__file__).resolve().parents[1] / "tools" / "ghidra"
            / "FindGhostsSplitScreenBuiltins.java"
        ).read_text()
        for token in (
            "getReferencesTo(stringAddress)",
            "getFunctionContaining(from)",
            "getEntryPoint().toString()",
            "getReferenceType().toString()",
            "getReferenceType().isCall()",
            "getReferenceType().isData()",
            "is_call,is_data",
            "PlayerCMD_Spawn",
            "CL_InitLocalClients",
        ):
            with self.subTest(token=token):
                self.assertIn(token, source)
        self.assertIn("A string xref may be a registration/data reference", source)

    def test_hashes_all_four_inputs(self):
        fingerprint = "a" * 64
        write(self.campaign_inventory, IF, [inv("c", "0x1000", fingerprint)])
        write(self.multiplayer_inventory, IF, [inv("m", "0x9000", fingerprint)])
        write(self.campaign_xrefs, XF, [
            xref("isSplitScreen", "0x3000", "0x1001", "0x1000", is_call=True, is_data=False)
        ])
        write(self.multiplayer_xrefs, XF, [
            xref("isSplitScreen", "0x4000", "0x9001", "0x9000", is_call=True, is_data=False)
        ])
        result = compare_paths(
            self.campaign_xrefs, self.multiplayer_xrefs,
            self.campaign_inventory, self.multiplayer_inventory,
        )
        self.assertEqual(set(result["input_sha256"]), {
            "campaign_xrefs", "multiplayer_xrefs", "campaign_inventory", "multiplayer_inventory"
        })


if __name__ == "__main__":
    unittest.main()
