import csv
import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from tools.compare_ppu_function_inventories import (
    InventoryError,
    compare,
    read_inventory,
    validate_inventory_manifest,
    _opd_toc_evidence,
)


FIELDS = [
    "program", "name", "entry", "body_address_count",
    "instruction_count", "fingerprint", "call_targets",
]


OPD_BASE_FIELDS = ["opd_descriptor_addresses", "opd_toc_mappings", "opd_descriptor_count"]
OPD_RESOLUTION_FIELDS = [
    "opd_exact_entry_descriptor_count",
    "opd_containing_function_descriptor_count",
]
OPD_REGION_FIELDS = ["opd_toc_region_evidence"]
OPD_FIELDS = OPD_BASE_FIELDS + OPD_RESOLUTION_FIELDS + OPD_REGION_FIELDS


def write_inventory(path: Path, rows, fields=FIELDS):
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def row(
    name, entry, fingerprint, calls="", symbols=None, entries=None,
    opd_addresses=None, opd_mappings=None, opd_count=None,
    opd_exact_count=None, opd_containing_count=0, opd_regions=None,
):
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
    if entries is not None:
        result["call_target_entries"] = entries
    if opd_addresses is not None:
        result["opd_descriptor_addresses"] = opd_addresses
    if opd_mappings is not None:
        result["opd_toc_mappings"] = opd_mappings
    if opd_count is not None:
        result["opd_descriptor_count"] = opd_count
        result["opd_exact_entry_descriptor_count"] = (
            opd_count if opd_exact_count is None else opd_exact_count
        )
        result["opd_containing_function_descriptor_count"] = opd_containing_count
        if opd_regions is None:
            opd_regions = []
            for mapping in (opd_mappings or "").split(";"):
                if not mapping:
                    continue
                address, status = mapping.split(":", 1)
                opd_regions.append({
                    "toc": address,
                    "status": status,
                    "block_name": ".got" if status == "MAPPED" else "",
                    "permissions": "rw-" if status == "MAPPED" else "",
                    "block_offset": "0x00000000" if status == "MAPPED" else "",
                })
        result["opd_toc_region_evidence"] = json.dumps(opd_regions)
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
        self.assertIn("call_target_entries", source)
        self.assertIn("opd_descriptor_addresses", source)
        self.assertIn("opd_toc_mappings", source)
        self.assertIn("opd_descriptor_count", source)
        self.assertIn("collectOpdFunctionEvidence", source)
        self.assertIn("getFunctionContaining(target)", source)
        self.assertIn("getEntryPoint().toString()", source)
        self.assertIn('"UNRESOLVED"', source)

    def test_combined_call_target_evidence_affects_candidate_ranking(self):
        common = "a" * 64
        target_lead = "b" * 64
        named_lead = "c" * 64
        campaign = [row("FUN_shared", "0x1000", common)]
        multiplayer = [
            row("FUN_shared_mp", "0x9000", common),
            row(
                "FUN_opaque_target_lead",
                "0x9100",
                target_lead,
                symbols=(
                    "0x1234:PlayerCMD_Spawn;"
                    "0x5678:SV_InitLocalClient;"
                    "0x9abc:CL_SetSplitScreen"
                ),
            ),
            row("SV_InitLocalClient", "0x9200", named_lead),
        ]
        result = compare(campaign, multiplayer)
        self.assertEqual(
            result["priority_multiplayer_only"][0]["function"]["name"],
            "FUN_opaque_target_lead",
        )

    def test_call_graph_ranks_opaque_candidate_from_incoming_caller(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            campaign_path = root / "campaign.csv"
            multiplayer_path = root / "multiplayer.csv"
            fields = FIELDS + ["call_target_entries"]
            shared = "a" * 64
            campaign_target = "b" * 64
            multiplayer_target = "c" * 64
            candidate = "d" * 64
            caller = "e" * 64
            write_inventory(campaign_path, [
                row("FUN_shared", "0x1000", shared, entries="0x2000"),
                row("FUN_campaign_target", "0x2000", campaign_target, entries=""),
            ], fields=fields)
            write_inventory(multiplayer_path, [
                row("FUN_shared_mp", "0x9000", shared, entries="0x9100"),
                row("FUN_mp_target", "0x9100", multiplayer_target, entries=""),
                row("FUN_opaque_9200", "0x9200", candidate, entries="0x9100"),
                row("SV_InitLocalClient", "0x9300", caller, entries="0x9200"),
            ], fields=fields)

            result = compare(
                read_inventory(campaign_path),
                read_inventory(multiplayer_path),
            )
            lead = next(
                item for item in result["priority_multiplayer_only"]
                if item["function"]["name"] == "FUN_opaque_9200"
            )
            self.assertIn("local", lead["incoming_caller_keyword_hits"])
            self.assertIn("client", lead["incoming_caller_keyword_hits"])
            self.assertEqual(lead["incoming_callers"][0]["name"], "SV_InitLocalClient")
            self.assertEqual(lead["outgoing_target_fingerprints"], [multiplayer_target])
            self.assertEqual(
                lead["outgoing_target_functions"][0]["name"],
                "FUN_mp_target",
            )
            self.assertFalse(
                lead["outgoing_target_functions"][0]["fingerprint_present_in_campaign"]
            )
            self.assertEqual(
                lead["outgoing_multiplayer_only_target_fingerprints"],
                [multiplayer_target],
            )
            self.assertEqual(
                result["summary"]["priority_multiplayer_only_incoming_caller_matches"],
                1,
            )
            self.assertEqual(
                result["summary"]["multiplayer_resolved_internal_call_edges"],
                3,
            )

    def test_matching_fingerprint_reports_changed_callee_fingerprints(self):
        shared = "a" * 64
        campaign_callee = "b" * 64
        multiplayer_callee = "c" * 64
        campaign = [
            row("FUN_common_campaign", "0x1000", shared, entries="0x2000"),
            row("FUN_campaign_target", "0x2000", campaign_callee, entries=""),
        ]
        multiplayer = [
            row("FUN_common_mp", "0x9000", shared, entries="0x9100"),
            row("FUN_mp_target", "0x9100", multiplayer_callee, entries=""),
        ]
        result = compare(campaign, multiplayer)
        self.assertEqual(
            result["summary"]["matching_fingerprint_groups_with_call_graph_delta"],
            1,
        )
        delta = result["call_graph_deltas"][0]
        self.assertEqual(delta["caller_fingerprint"], shared)
        self.assertEqual(delta["campaign_only_callee_fingerprints"], [campaign_callee])
        self.assertEqual(
            delta["campaign_callee_functions"][0]["name"],
            "FUN_campaign_target",
        )
        self.assertEqual(
            delta["multiplayer_only_callee_fingerprints"],
            [multiplayer_callee],
        )
        self.assertEqual(
            delta["multiplayer_callee_functions"][0]["name"],
            "FUN_mp_target",
        )

    def test_legacy_inventory_without_call_target_symbols_remains_supported(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "legacy.csv"
            write_inventory(path, [row("FUN_legacy", "0x1000", "a" * 64)])
            rows = read_inventory(path)
            self.assertEqual(rows[0]["call_target_symbols"], [])
            self.assertEqual(rows[0]["call_target_entries"], [])
            self.assertEqual(rows[0]["opd_descriptor_count"], 0)
            self.assertEqual(rows[0]["opd_toc_mappings"], [])

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

    def _write_manifest_pair(self, root: Path):
        campaign_path = root / "campaign_ppu_inventory.csv"
        multiplayer_path = root / "multiplayer_ppu_inventory.csv"
        write_inventory(campaign_path, [row("FUN_campaign", "0x1000", "a" * 64)])
        write_inventory(multiplayer_path, [row("FUN_multiplayer", "0x9000", "b" * 64)])
        campaign_rows = read_inventory(campaign_path)
        multiplayer_rows = read_inventory(multiplayer_path)

        def digest(path):
            return hashlib.sha256(path.read_bytes()).hexdigest()

        manifest = {
            "schema_version": 1,
            "operator_declared_profile": {
                "title_id": "BLUS31270",
                "title_update": "1.16",
                "analysis_language": "PowerPC:BE:64:default",
            },
            "campaign": {
                "inventory_file": campaign_path.name,
                "inventory_sha256": digest(campaign_path),
                "elf_sha256": "c" * 64,
                "function_records": 1,
            },
            "multiplayer": {
                "inventory_file": multiplayer_path.name,
                "inventory_sha256": digest(multiplayer_path),
                "elf_sha256": "d" * 64,
                "function_records": 1,
            },
        }
        manifest_path = root / "ppu_inventory_manifest.json"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        return manifest_path, campaign_path, multiplayer_path, campaign_rows, multiplayer_rows, manifest

    def test_validates_inventory_manifest_hashes_and_profile(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest_path, campaign_path, multiplayer_path, campaign_rows, multiplayer_rows, _ = (
                self._write_manifest_pair(root)
            )
            result = validate_inventory_manifest(
                manifest_path,
                campaign_path,
                multiplayer_path,
                campaign_rows,
                multiplayer_rows,
            )
            self.assertEqual(
                result["status"],
                "VALIDATED_HASHES_AND_OPERATOR_DECLARED_PROFILE",
            )
            self.assertEqual(result["campaign"]["function_records"], 1)
            self.assertEqual(result["multiplayer"]["function_records"], 1)

    def test_rejects_manifest_when_inventory_csv_hash_changes(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest_path, campaign_path, multiplayer_path, campaign_rows, multiplayer_rows, _ = (
                self._write_manifest_pair(root)
            )
            campaign_path.write_text(
                campaign_path.read_text(encoding="utf-8") + "\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(InventoryError, "SHA-256 does not match"):
                validate_inventory_manifest(
                    manifest_path,
                    campaign_path,
                    multiplayer_path,
                    campaign_rows,
                    multiplayer_rows,
                )

    def test_rejects_manifest_for_wrong_operator_declared_profile(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            manifest_path, campaign_path, multiplayer_path, campaign_rows, multiplayer_rows, manifest = (
                self._write_manifest_pair(root)
            )
            manifest["operator_declared_profile"]["title_update"] = "1.15"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(InventoryError, "title_update must be 1.16"):
                validate_inventory_manifest(
                    manifest_path,
                    campaign_path,
                    multiplayer_path,
                    campaign_rows,
                    multiplayer_rows,
                )

    def test_rejects_duplicate_function_entries_as_ambiguous_call_graph_nodes(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "duplicate-entry.csv"
            write_inventory(path, [
                row("FUN_first", "0x1000", "a" * 64),
                row("FUN_second", "0x1000", "b" * 64),
            ])
            with self.assertRaisesRegex(InventoryError, "duplicate function entry"):
                read_inventory(path)

    def test_rejects_empty_inventory(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "empty.csv"
            write_inventory(path, [])
            with self.assertRaises(InventoryError):
                read_inventory(path)


    def test_ghidra_inventory_exporter_writes_utf8_csv(self):
        source = (
            Path(__file__).resolve().parents[1] / "tools" / "ghidra"
            / "ExportGhostsPpuFunctionInventory.java"
        ).read_text()
        self.assertIn("Files.newBufferedWriter", source)
        self.assertIn("StandardCharsets.UTF_8", source)

    def test_reads_per_function_opd_descriptor_and_toc_mapping_evidence(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "opd.csv"
            write_inventory(path, [
                row(
                    "SV_InitLocalClient", "0x9000", "a" * 64,
                    opd_addresses="ram:00002000;ram:00002008",
                    opd_mappings="0x00003000:MAPPED;0x00004000:UNMAPPED",
                    opd_count="2",
                )
            ], fields=FIELDS + OPD_FIELDS)
            parsed = read_inventory(path)
            self.assertEqual(parsed[0]["opd_descriptor_count"], 2)
            self.assertEqual(parsed[0]["opd_exact_entry_descriptor_count"], 2)
            self.assertEqual(parsed[0]["opd_containing_function_descriptor_count"], 0)
            self.assertEqual(parsed[0]["opd_toc_mappings"], [
                "0x00003000:MAPPED", "0x00004000:UNMAPPED"
            ])
            result = compare([], parsed)
            evidence = result["priority_multiplayer_only"][0]["opd_toc_evidence"]
            self.assertEqual(evidence["status"], "UNMAPPED_TOC_PRESENT")
            self.assertEqual(evidence["descriptor_count"], 2)
            self.assertEqual(result["summary"]["priority_multiplayer_only_candidates_with_unmapped_toc"], 1)

    def test_rejects_invalid_or_inconsistent_opd_toc_mappings(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bad-opd.csv"
            write_inventory(path, [
                row(
                    "FUN_bad", "0x1000", "a" * 64,
                    opd_addresses="ram:00002000",
                    opd_mappings="0x00000000:MAPPED",
                    opd_count="1",
                )
            ], fields=FIELDS + OPD_FIELDS)
            with self.assertRaisesRegex(InventoryError, "status contradicts"):
                read_inventory(path)

    def test_rejects_partial_optional_opd_columns(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "partial-opd.csv"
            write_inventory(path, [row("FUN", "0x1000", "a" * 64)],
                            fields=FIELDS + ["opd_descriptor_count"])
            with self.assertRaisesRegex(InventoryError, "incomplete OPD evidence columns"):
                read_inventory(path)



    def test_reports_containing_function_only_opd_matches_separately(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "containing-only.csv"
            write_inventory(path, [
                row(
                    "FUN_inside", "0x1000", "a" * 64,
                    opd_addresses="ram:00002000",
                    opd_mappings="0x00003000:MAPPED",
                    opd_count=1,
                    opd_exact_count=0,
                    opd_containing_count=1,
                )
            ], fields=FIELDS + OPD_FIELDS)
            parsed = read_inventory(path)
            evidence = _opd_toc_evidence(parsed[0])
            self.assertEqual(
                evidence["code_pointer_resolution"]["status"],
                "CONTAINING_FUNCTION_ONLY",
            )
            self.assertEqual(
                evidence["code_pointer_resolution"]["containing_function_descriptor_count"],
                1,
            )

    def test_rejects_opd_resolution_counts_that_do_not_reconcile(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bad-resolution.csv"
            write_inventory(path, [
                row(
                    "FUN_bad", "0x1000", "a" * 64,
                    opd_addresses="ram:00002000",
                    opd_mappings="0x00003000:MAPPED",
                    opd_count=1,
                    opd_exact_count=1,
                    opd_containing_count=1,
                )
            ], fields=FIELDS + OPD_FIELDS)
            with self.assertRaisesRegex(InventoryError, "resolution counts do not match"):
                read_inventory(path)

    def test_legacy_opd_columns_remain_supported_without_resolution_classification(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "legacy-opd.csv"
            legacy = row(
                "SV_InitLocalClient", "0x9000", "a" * 64,
                opd_addresses="ram:00002000",
                opd_mappings="0x00003000:MAPPED",
                opd_count=1,
            )
            legacy.pop("opd_exact_entry_descriptor_count")
            legacy.pop("opd_containing_function_descriptor_count")
            legacy.pop("opd_toc_region_evidence")
            write_inventory(path, [legacy], fields=FIELDS + OPD_BASE_FIELDS)
            parsed = read_inventory(path)
            self.assertIsNone(parsed[0]["opd_exact_entry_descriptor_count"])
            self.assertIsNone(parsed[0]["opd_containing_function_descriptor_count"])
            self.assertEqual(
                _opd_toc_evidence(parsed[0])["code_pointer_resolution"]["status"],
                "NOT_RECORDED",
            )


    def test_reads_toc_memory_block_permissions_and_relative_offset(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "toc-region.csv"
            write_inventory(path, [
                row(
                    "SV_InitLocalClient", "0x9000", "a" * 64,
                    opd_addresses="ram:00002000",
                    opd_mappings="0x00003020:MAPPED",
                    opd_count=1,
                    opd_exact_count=1,
                    opd_regions=[{
                        "toc": "0x00003020",
                        "status": "MAPPED",
                        "block_name": ".got",
                        "permissions": "rw-",
                        "block_offset": "0x00000020",
                    }],
                )
            ], fields=FIELDS + OPD_FIELDS)
            parsed = read_inventory(path)
            result = compare([], parsed)
            evidence = result["priority_multiplayer_only"][0]["opd_toc_evidence"]
            self.assertEqual(evidence["toc_region_evidence"][0]["block_name"], ".got")
            self.assertEqual(evidence["toc_region_evidence"][0]["permissions"], "rw-")
            self.assertEqual(evidence["toc_region_evidence"][0]["block_offset"], "0x00000020")
            self.assertEqual(
                result["summary"]["priority_multiplayer_only_candidates_with_writable_toc_block"],
                1,
            )

    def test_rejects_toc_region_status_that_disagrees_with_mapping(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bad-toc-region.csv"
            write_inventory(path, [
                row(
                    "FUN_bad", "0x1000", "a" * 64,
                    opd_addresses="ram:00002000",
                    opd_mappings="0x00003000:MAPPED",
                    opd_count=1,
                    opd_regions=[{
                        "toc": "0x00003000",
                        "status": "UNMAPPED",
                        "block_name": "",
                        "permissions": "",
                        "block_offset": "",
                    }],
                )
            ], fields=FIELDS + OPD_FIELDS)
            with self.assertRaisesRegex(InventoryError, "regions do not match"):
                read_inventory(path)

    def test_rejects_mapped_toc_region_without_valid_memory_permissions(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "bad-toc-permissions.csv"
            write_inventory(path, [
                row(
                    "FUN_bad", "0x1000", "a" * 64,
                    opd_addresses="ram:00002000",
                    opd_mappings="0x00003000:MAPPED",
                    opd_count=1,
                    opd_regions=[{
                        "toc": "0x00003000",
                        "status": "MAPPED",
                        "block_name": ".got",
                        "permissions": "read-write",
                        "block_offset": "0x00000000",
                    }],
                )
            ], fields=FIELDS + OPD_FIELDS)
            with self.assertRaisesRegex(InventoryError, "lacks block permissions"):
                read_inventory(path)

if __name__ == "__main__":
    unittest.main()
