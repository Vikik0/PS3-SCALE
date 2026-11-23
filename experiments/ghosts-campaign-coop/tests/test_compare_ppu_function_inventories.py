import csv
import tempfile
import unittest
from pathlib import Path

from tools.compare_ppu_function_inventories import (
    InventoryError,
    compare,
    read_inventory,
)


FIELDS = [
    "program", "name", "entry", "body_address_count",
    "instruction_count", "fingerprint", "call_targets",
]


def write_inventory(path: Path, rows, fields=FIELDS):
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def row(name, entry, fingerprint, calls="", symbols=None):
    result = {
        "program": "Ghosts",
        "name": name,
        "entry": entry,
        "body_address_count": "64",
        "instruction_count": "16",
        "fingerprint": fingerprint,
        "call_targets": calls,
    }
    if symbols is not None:
        result["call_target_symbols"] = symbols
    return result


class ComparePpuFunctionInventoriesTests(unittest.TestCase):
    def test_matches_shared_code_even_when_entry_addresses_differ(self):
        common = "a" * 64
        campaign = [row("FUN_campaign_1000", "0x1000", common)]
        multiplayer = [row("FUN_mp_9000", "0x9000", common)]
        result = compare(campaign, multiplayer)
        self.assertEqual(result["summary"]["matching_fingerprint_groups"], 1)
        self.assertEqual(result["summary"]["multiplayer_only_fingerprint_groups"], 0)
        self.assertEqual(
            result["matched_fingerprints"][0]["multiplayer_functions"][0]["entry"],
            "0x9000",
        )

    def test_ranks_multiplayer_only_split_screen_and_client_names(self):
        common = "a" * 64
        mp_split = "b" * 64
        mp_other = "c" * 64
        campaign = [row("FUN_shared", "0x1000", common)]
        multiplayer = [
            row("FUN_shared_mp", "0x9000", common),
            row("SV_InitLocalClient", "0x9100", mp_split),
            row("G_ClientThink", "0x9200", mp_other),
        ]
        result = compare(campaign, multiplayer)
        self.assertEqual(result["summary"]["multiplayer_only_fingerprint_groups"], 2)
        self.assertEqual(result["summary"]["priority_multiplayer_only_name_matches"], 2)
        names = {item["function"]["name"] for item in result["priority_multiplayer_only"]}
        self.assertEqual(names, {"SV_InitLocalClient", "G_ClientThink"})

    def test_ranks_multiplayer_only_functions_by_resolved_call_target_symbols(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign_path = root / "campaign.csv"
            multiplayer_path = root / "multiplayer.csv"
            fields = FIELDS + ["call_target_symbols"]
            common = "a" * 64
            lead = "f" * 64
            write_inventory(campaign_path, [row("FUN_shared", "0x1000", common)], fields=fields)
            write_inventory(multiplayer_path, [
                row("FUN_shared_mp", "0x9000", common),
                row(
                    "FUN_opaque_9100",
                    "0x9100",
                    lead,
                    calls="0x1234;0x5678",
                    symbols="0x1234:PlayerCMD_Spawn;0x5678:SV_InitLocalClient",
                ),
            ], fields=fields)

            campaign = read_inventory(campaign_path)
            multiplayer = read_inventory(multiplayer_path)
            result = compare(campaign, multiplayer)

            self.assertEqual(result["summary"]["priority_multiplayer_only_name_matches"], 0)
            self.assertEqual(result["summary"]["priority_multiplayer_only_call_target_matches"], 1)
            self.assertEqual(result["summary"]["priority_multiplayer_only_leads"], 1)
            lead_result = result["priority_multiplayer_only"][0]
            self.assertEqual(lead_result["function"]["name"], "FUN_opaque_9100")
            self.assertIn("spawn", lead_result["call_target_keyword_hits"])
            self.assertIn("client", lead_result["call_target_keyword_hits"])

    def test_ghidra_exporter_emits_resolved_call_target_symbols(self):
        exporter = (
            Path(__file__).resolve().parents[1]
            / "tools"
            / "ghidra"
            / "ExportGhostsPpuFunctionInventory.java"
        )
        source = exporter.read_text(encoding="utf-8")
        self.assertIn("call_target_symbols", source)
        self.assertIn("getFunctionAt(target)", source)
        self.assertIn('"UNRESOLVED"', source)

    def test_legacy_inventory_without_call_target_symbols_remains_supported(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "legacy.csv"
            write_inventory(path, [row("FUN_legacy", "0x1000", "a" * 64)])
            rows = read_inventory(path)
            self.assertEqual(rows[0]["call_target_symbols"], [])

    def test_preserves_duplicate_fingerprint_multiplicity(self):
        duplicate = "d" * 64
        campaign = [
            row("FUN_c1", "0x1000", duplicate),
            row("FUN_c2", "0x1100", duplicate),
        ]
        multiplayer = [row("FUN_mp", "0x9000", duplicate)]
        result = compare(campaign, multiplayer)
        group = result["matched_fingerprints"][0]
        self.assertEqual(group["campaign_count"], 2)
        self.assertEqual(group["multiplayer_count"], 1)
        self.assertEqual(group["multiplicity_delta"], -1)

    def test_rejects_missing_columns(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bad.csv"
            write_inventory(path, [], fields=["name", "entry"])
            with self.assertRaises(InventoryError):
                read_inventory(path)

    def test_rejects_invalid_fingerprint_and_nonpositive_counts(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bad.csv"
            write_inventory(path, [row("FUN_bad", "0x1000", "not-a-hash")])
            with self.assertRaises(InventoryError):
                read_inventory(path)
            write_inventory(path, [{
                **row("FUN_bad", "0x1000", "e" * 64),
                "instruction_count": "0",
            }])
            with self.assertRaises(InventoryError):
                read_inventory(path)

    def test_rejects_empty_inventory(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "empty.csv"
            write_inventory(path, [])
            with self.assertRaises(InventoryError):
                read_inventory(path)


if __name__ == "__main__":
    unittest.main()
