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


def row(name, entry, fingerprint, calls=""):
    return {
        "program": "Ghosts",
        "name": name,
        "entry": entry,
        "body_address_count": "64",
        "instruction_count": "16",
        "fingerprint": fingerprint,
        "call_targets": calls,
    }


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
