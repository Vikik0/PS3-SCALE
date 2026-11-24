import csv
import hashlib
import json
import subprocess
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import call, patch

from tools.ps3_elf_inspect import inspect_file
from tools.run_ghidra_ppu_inventory import (
    EXPORT_SCRIPT,
    XREF_SCRIPT,
    PROCESSOR,
    SEED_SCRIPT,
    _validate_opd_pair,
    _validate_opd_tuple,
    generate_inventories,
    _read_elf_inspector_opd_report,
)


CSV_FIELDS = [
    "program",
    "name",
    "entry",
    "body_address_count",
    "instruction_count",
    "fingerprint",
    "call_targets",
    "call_target_symbols",
    "call_target_entries",
    "opd_descriptor_addresses",
    "opd_toc_mappings",
    "opd_descriptor_count",
    "opd_exact_entry_descriptor_count",
    "opd_containing_function_descriptor_count",
    "opd_toc_region_evidence",
]


class RunGhidraPpuInventoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.headless = self.root / "analyzeHeadless"
        self.headless.write_text("fake executable", encoding="utf-8")
        self.script_dir = self.root / "ghidra-scripts"
        self.script_dir.mkdir()
        (self.script_dir / EXPORT_SCRIPT).write_text("// export", encoding="utf-8")
        (self.script_dir / XREF_SCRIPT).write_text("// xrefs", encoding="utf-8")
        (self.script_dir / SEED_SCRIPT).write_text("// seed", encoding="utf-8")
        self.campaign_elf = self.root / "campaign.elf"
        self.multiplayer_elf = self.root / "multiplayer.elf"
        self.campaign_elf.write_bytes(b"campaign-elf-fixture")
        self.multiplayer_elf.write_bytes(b"multiplayer-elf-fixture")
        self.output_dir = self.root / "output"

    def tearDown(self):
        self.temp.cleanup()

    def _write_inventory(self, path, name, fingerprint, *, opd=False):
        with path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=CSV_FIELDS)
            writer.writeheader()
            writer.writerow({
                "program": name,
                "name": "FUN_fixture",
                "entry": "0x1000",
                "body_address_count": "8",
                "instruction_count": "2",
                "fingerprint": fingerprint,
                "call_targets": "",
                "call_target_symbols": "",
                "call_target_entries": "",
                "opd_descriptor_addresses": "ram:00001000" if opd else "",
                "opd_toc_mappings": "0x00003000:UNMAPPED" if opd else "",
                "opd_descriptor_count": "1" if opd else "0",
                "opd_exact_entry_descriptor_count": "1" if opd else "0",
                "opd_containing_function_descriptor_count": "0",
                "opd_toc_region_evidence": json.dumps([{
                    "toc": "0x00003000",
                    "status": "UNMAPPED",
                    "block_name": "",
                    "permissions": "",
                    "block_offset": "",
                }] if opd else []),
            })

    def _write_xrefs(self, path, program):
        fields = [
            "program", "builtin", "string_address", "reference_address",
            "reference_type", "is_call", "is_data", "is_computed", "source_type", "caller_name", "caller_entry",
            "caller_body_start", "caller_body_end", "instruction",
        ]
        with path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=fields)
            writer.writeheader()
            writer.writerow({
                "program": program, "builtin": "isSplitScreen",
                "string_address": "0x5000", "reference_address": "0x1001",
                "reference_type": "UNCONDITIONAL_CALL", "is_call": "true", "is_data": "false", "is_computed": "false",
                "source_type": "ANALYSIS", "caller_name": "FUN_fixture", "caller_entry": "0x1000",
                "caller_body_start": "0x1000", "caller_body_end": "0x1010",
                "instruction": "load r3, 0x5000",
            })


    def write_inspector_report(self, path, elf_path, *, valid=True):
        ident = bytearray(b"\x7fELF" + bytes([2, 2, 1, 0, 0]) + bytes(7))
        header = struct.pack(
            ">16sHHIQQQIHHHHHH", bytes(ident), 2, 21, 1,
            0x2000, 64, 0x300, 0, 64, 56, 2, 64, 2, 0,
        )
        code_segment = struct.pack(
            ">IIQQQQQQ", 1, 5, 0x100, 0x1000,
            0x1000, 0x80, 0x80, 0x1000,
        )
        data_segment = struct.pack(
            ">IIQQQQQQ", 1, 4, 0x200, 0x2000,
            0x2000, 0x10, 0x10, 8,
        )
        null_section = bytes(64)
        opd_section = struct.pack(
            ">IIQQQQIIQQ", 0, 1, 0, 0x2000, 0x200, 0x10, 0, 0, 8, 8,
        )
        elf_bytes = bytearray(0x380)
        elf_bytes[:64] = header
        elf_bytes[64:120] = code_segment
        elf_bytes[120:176] = data_segment
        elf_bytes[0x100:0x180] = bytes(range(0x80))
        elf_bytes[0x200:0x210] = struct.pack(">IIII", 0x1010, 0x3000, 0x1020, 0x3000)
        elf_bytes[0x300:0x340] = null_section
        elf_bytes[0x340:0x380] = opd_section
        Path(elf_path).write_bytes(elf_bytes)
        report = inspect_file(Path(elf_path), "BLUS31270")
        if not valid:
            report["elf"]["opd"]["section_alignment_valid"] = False
        report_path = Path(path)
        report_path.write_text(json.dumps(report), encoding="utf-8")
        return report_path

    def _fake_run(self, command, check):
        export_index = command.index(EXPORT_SCRIPT)
        xref_index = command.index(XREF_SCRIPT)
        output_csv = Path(command[export_index + 1])
        output_xref_csv = Path(command[xref_index + 1])
        export_opd = (
            export_index + 3 < len(command)
            and command[export_index + 2] != "-postScript"
        )
        if "Campaign" in command[2]:
            self._write_inventory(output_csv, "Campaign", "a" * 64, opd=export_opd)
            self._write_xrefs(output_xref_csv, "Campaign")
        else:
            self._write_inventory(output_csv, "Multiplayer", "b" * 64, opd=export_opd)
            self._write_xrefs(output_xref_csv, "Multiplayer")
        if export_opd:
            resolution_path = Path(command[export_index + 4])
            slots = int(command[export_index + 3], 0) // 8
            nonempty = min(4, slots)
            valid = min(3, nonempty)
            exact = min(1, valid)
            containing = 0
            resolution_path.write_text(json.dumps({
                "schema_version": 1,
                "status": "OPD_FUNCTION_RESOLUTION_COMPLETED",
                "opd_va": command[export_index + 2],
                "opd_size": command[export_index + 3],
                "descriptor_slots_scanned": slots,
                "empty_descriptors": slots - nonempty,
                "nonempty_descriptors": nonempty,
                "unreadable_descriptors": 0,
                "valid_executable_code_pointers": valid,
                "invalid_code_pointers": nonempty - valid,
                "exact_entry_matches": exact,
                "containing_function_matches": containing,
                "unresolved_executable_pointers": valid - exact - containing,
                "functions_with_opd_descriptors": 1 if exact + containing else 0,
            }), encoding="utf-8")
        if SEED_SCRIPT in command:
            seed_index = command.index(SEED_SCRIPT)
            summary_path = Path(command[seed_index + 3])
            slots = int(command[seed_index + 2], 0) // 8
            nonempty = min(4, slots)
            valid = min(3, nonempty)
            zero_toc = min(1, nonempty)
            distinct_toc = max(0, nonempty - zero_toc - 1)
            created = min(2, valid)
            summary_path.write_text(json.dumps({
                "schema_version": 2,
                "status": "OPD_SEEDING_COMPLETED",
                "opd_va": command[seed_index + 1],
                "opd_size": command[seed_index + 2],
                "descriptor_slots_scanned": slots,
                "empty_descriptors": slots - nonempty,
                "nonempty_descriptors": nonempty,
                "unreadable_descriptors": 0,
                "valid_code_pointers": valid,
                "invalid_code_pointers": nonempty - valid,
                "functions_created": created,
                "functions_already_present": valid - created,
                "functions_failed_to_create": 0,
                "distinct_toc_values": distinct_toc,
                "mapped_toc_values": distinct_toc,
                "unmapped_toc_values": 0,
                "zero_toc_descriptors": zero_toc,
            }), encoding="utf-8")
        return subprocess.CompletedProcess(command, 0)

    def test_runs_both_builds_and_writes_hash_manifest_atomically(self):
        with patch("tools.run_ghidra_ppu_inventory.subprocess.run", side_effect=self._fake_run) as run:
            manifest = generate_inventories(
                self.headless,
                self.campaign_elf,
                self.multiplayer_elf,
                self.output_dir,
                script_dir=self.script_dir,
                campaign_opd=(0x1000, 0x40),
            )

        self.assertEqual(run.call_count, 2)
        campaign_command = run.call_args_list[0].args[0]
        multiplayer_command = run.call_args_list[1].args[0]
        self.assertIn("-processor", campaign_command)
        self.assertEqual(campaign_command[campaign_command.index("-processor") + 1], PROCESSOR)
        self.assertIn(SEED_SCRIPT, campaign_command)
        self.assertEqual(campaign_command[campaign_command.index(SEED_SCRIPT) + 1], "0x1000")
        self.assertEqual(campaign_command[campaign_command.index(SEED_SCRIPT) + 2], "0x40")
        self.assertTrue(campaign_command[campaign_command.index(SEED_SCRIPT) + 3].endswith("campaign_opd_seed_summary.json"))
        export_index = campaign_command.index(EXPORT_SCRIPT)
        self.assertEqual(campaign_command[export_index + 2], "0x1000")
        self.assertEqual(campaign_command[export_index + 3], "0x40")
        self.assertTrue(campaign_command[export_index + 4].endswith("campaign_opd_function_resolution.json"))
        self.assertNotIn(SEED_SCRIPT, multiplayer_command)
        self.assertIn(EXPORT_SCRIPT, campaign_command)
        self.assertIn(EXPORT_SCRIPT, multiplayer_command)
        self.assertIn(XREF_SCRIPT, campaign_command)
        self.assertIn(XREF_SCRIPT, multiplayer_command)
        self.assertIn("-deleteProject", campaign_command)
        self.assertIn("-deleteProject", multiplayer_command)

        campaign_hash = hashlib.sha256(self.campaign_elf.read_bytes()).hexdigest()
        multiplayer_hash = hashlib.sha256(self.multiplayer_elf.read_bytes()).hexdigest()
        self.assertEqual(manifest["campaign"]["elf_sha256"], campaign_hash)
        self.assertEqual(manifest["multiplayer"]["elf_sha256"], multiplayer_hash)
        self.assertTrue(manifest["campaign"]["opd_seeding"]["enabled"])
        self.assertEqual(manifest["campaign"]["opd_seeding"]["status"], "OPD_SEEDING_COMPLETED")
        self.assertEqual(manifest["campaign"]["opd_seeding"]["summary"]["valid_code_pointers"], 3)
        self.assertEqual(manifest["campaign"]["opd_seeding"]["summary"]["functions_created"], 2)
        self.assertRegex(manifest["campaign"]["opd_seeding"]["summary_sha256"], r"^[0-9a-f]{64}$")
        self.assertTrue(manifest["campaign"]["opd_function_resolution"]["enabled"])
        self.assertEqual(
            manifest["campaign"]["opd_function_resolution"]["summary"]["unresolved_executable_pointers"],
            2,
        )
        self.assertEqual(
            manifest["campaign"]["opd_function_resolution"]["summary"]["exact_entry_matches"],
            1,
        )
        self.assertRegex(
            manifest["campaign"]["opd_function_resolution"]["summary_sha256"],
            r"^[0-9a-f]{64}$",
        )
        self.assertFalse(manifest["multiplayer"]["opd_seeding"]["enabled"])

        saved = json.loads(
            (self.output_dir / "ppu_inventory_manifest.json").read_text(encoding="utf-8")
        )
        self.assertEqual(saved["campaign"]["function_records"], 1)
        self.assertEqual(saved["multiplayer"]["function_records"], 1)
        self.assertEqual(saved["campaign"]["builtin_xref_rows"], 1)
        self.assertEqual(saved["multiplayer"]["builtin_xref_rows"], 1)
        self.assertEqual(saved["campaign"]["builtin_xref_call_rows"], 1)
        self.assertEqual(saved["multiplayer"]["builtin_xref_call_rows"], 1)
        self.assertEqual(saved["campaign"]["builtin_xref_direct_call_rows"], 1)
        self.assertEqual(saved["campaign"]["builtin_xref_computed_call_rows"], 0)
        self.assertEqual(saved["campaign"]["builtin_xref_data_rows"], 0)
        self.assertEqual(saved["multiplayer_only_builtin_lead_count"], 1)
        self.assertEqual(saved["builtin_xref_comparison_file"], "splitscreen_builtin_xrefs.json")
        self.assertRegex(saved["builtin_xref_comparison_sha256"], r"^[0-9a-f]{64}$")
        self.assertRegex(saved["campaign"]["builtin_xref_sha256"], r"^[0-9a-f]{64}$")
        self.assertTrue((self.output_dir / "campaign_ppu_inventory.csv").is_file())
        self.assertTrue((self.output_dir / "multiplayer_ppu_inventory.csv").is_file())
        self.assertTrue((self.output_dir / "campaign_builtin_xrefs.csv").is_file())
        self.assertTrue((self.output_dir / "multiplayer_builtin_xrefs.csv").is_file())
        self.assertTrue((self.output_dir / "splitscreen_builtin_xrefs.json").is_file())

    def test_second_mode_failure_does_not_promote_partial_inventory(self):
        calls = 0

        def fake_run(command, check):
            nonlocal calls
            calls += 1
            if calls == 1:
                self._fake_run(command, check)
                return subprocess.CompletedProcess(command, 0)
            raise subprocess.CalledProcessError(1, command)

        with patch("tools.run_ghidra_ppu_inventory.subprocess.run", side_effect=fake_run):
            with self.assertRaises(subprocess.CalledProcessError):
                generate_inventories(
                    self.headless,
                    self.campaign_elf,
                    self.multiplayer_elf,
                    self.output_dir,
                    script_dir=self.script_dir,
                )

        self.assertFalse((self.output_dir / "campaign_ppu_inventory.csv").exists())
        self.assertFalse((self.output_dir / "multiplayer_ppu_inventory.csv").exists())
        self.assertFalse((self.output_dir / "campaign_builtin_xrefs.csv").exists())
        self.assertFalse((self.output_dir / "multiplayer_builtin_xrefs.csv").exists())
        self.assertFalse((self.output_dir / "ppu_inventory_manifest.json").exists())

    def test_existing_outputs_are_not_overwritten_without_explicit_flag(self):
        self.output_dir.mkdir()
        existing = self.output_dir / "campaign_ppu_inventory.csv"
        existing.write_text("keep this file", encoding="utf-8")
        with patch("tools.run_ghidra_ppu_inventory.subprocess.run") as run:
            with self.assertRaises(FileExistsError):
                generate_inventories(
                    self.headless,
                    self.campaign_elf,
                    self.multiplayer_elf,
                    self.output_dir,
                    script_dir=self.script_dir,
                )
        run.assert_not_called()
        self.assertEqual(existing.read_text(encoding="utf-8"), "keep this file")

    def test_direct_api_rejects_invalid_opd_tuple_before_running_ghidra(self):
        with patch("tools.run_ghidra_ppu_inventory.subprocess.run") as run:
            with self.assertRaisesRegex(ValueError, "divisible by 8"):
                generate_inventories(
                    self.headless,
                    self.campaign_elf,
                    self.multiplayer_elf,
                    self.output_dir,
                    script_dir=self.script_dir,
                    campaign_opd=(0x1000, 0x11),
                )
        run.assert_not_called()

    def test_opd_arguments_must_be_paired_and_size_must_be_aligned(self):
        with self.assertRaisesRegex(ValueError, "provide both"):
            _validate_opd_pair("0x1000", None, "Campaign")
        with self.assertRaisesRegex(ValueError, "divisible by 8"):
            _validate_opd_pair("0x1000", "0x11", "Campaign")
        self.assertEqual(
            _validate_opd_pair("0x1000", "64", "Campaign"),
            (0x1000, 64),
        )

    def test_malformed_inventory_aborts_before_any_final_output_is_promoted(self):
        def fake_run(command, check):
            output_csv = Path(command[command.index(EXPORT_SCRIPT) + 1])
            output_csv.write_text(
                "program,name,entry,body_address_count,instruction_count,fingerprint,call_targets\n"
                "Campaign,FUN_bad,0x1000,8,2,not-a-hash,,\n",
                encoding="utf-8",
            )
            return subprocess.CompletedProcess(command, 0)

        with patch("tools.run_ghidra_ppu_inventory.subprocess.run", side_effect=fake_run):
            with self.assertRaisesRegex(ValueError, "fingerprint"):
                generate_inventories(
                    self.headless,
                    self.campaign_elf,
                    self.multiplayer_elf,
                    self.output_dir,
                    script_dir=self.script_dir,
                )
        self.assertFalse((self.output_dir / "campaign_ppu_inventory.csv").exists())
        self.assertFalse((self.output_dir / "campaign_builtin_xrefs.csv").exists())
        self.assertFalse((self.output_dir / "ppu_inventory_manifest.json").exists())

    def test_malformed_xref_csv_aborts_before_any_final_output_is_promoted(self):
        def fake_run(command, check):
            self._fake_run(command, check)
            xref_index = command.index(XREF_SCRIPT)
            Path(command[xref_index + 1]).write_text(
                "program,builtin\nCampaign,isSplitScreen\n", encoding="utf-8"
            )
            return subprocess.CompletedProcess(command, 0)

        with patch("tools.run_ghidra_ppu_inventory.subprocess.run", side_effect=fake_run):
            with self.assertRaisesRegex(ValueError, "missing columns"):
                generate_inventories(
                    self.headless, self.campaign_elf, self.multiplayer_elf,
                    self.output_dir, script_dir=self.script_dir,
                )
        for name in (
            "campaign_ppu_inventory.csv", "multiplayer_ppu_inventory.csv",
            "campaign_builtin_xrefs.csv", "multiplayer_builtin_xrefs.csv",
            "splitscreen_builtin_xrefs.json", "ppu_inventory_manifest.json",
        ):
            self.assertFalse((self.output_dir / name).exists())

    def test_opd_ranges_must_fit_unsigned_32_bit_ppu_address_space(self):
        with self.assertRaisesRegex(ValueError, "unsigned 32-bit PPU address space"):
            _validate_opd_pair("0x100000000", "8", "Campaign")
        with self.assertRaisesRegex(ValueError, "range exceeds"):
            _validate_opd_pair("0xFFFFFFF8", "0x10", "Campaign")
        with self.assertRaisesRegex(ValueError, "unsigned 32-bit PPU address space"):
            _validate_opd_tuple((0x100000000, 8), "Campaign")
        with self.assertRaisesRegex(ValueError, "range exceeds"):
            _validate_opd_tuple((0xFFFFFFF8, 0x10), "Campaign")

    def test_opd_seed_script_checks_ppu_address_bounds_before_mapping(self):
        source = (
            Path(__file__).resolve().parents[1] / "tools" / "ghidra"
            / "SeedGhostsOpdFunctions.java"
        ).read_text()
        self.assertIn("MAX_PPU_ADDRESS", source)
        self.assertIn("PPU_ADDRESS_SPACE - opdVa", source)
        self.assertIn("opdSize > PPU_ADDRESS_SPACE - opdVa", source)
        self.assertIn("(opdVa & 7L) != 0L", source)
        self.assertIn("(codeUnsigned & 3L) != 0L", source)
        self.assertIn("getMemory().contains(opdStart)", source)

    def test_opd_table_base_must_be_eight_byte_aligned(self):
        with self.assertRaisesRegex(ValueError, "8-byte aligned"):
            _validate_opd_pair("0x1004", "0x10", "Campaign")
        with self.assertRaisesRegex(ValueError, "8-byte aligned"):
            _validate_opd_tuple((0x1004, 0x10), "Campaign")

    def test_opd_seed_summary_is_required_before_inventory_promotion(self):
        def fake_run(command, check):
            self._fake_run(command, check)
            if SEED_SCRIPT in command:
                summary_path = Path(command[command.index(SEED_SCRIPT) + 3])
                summary_path.unlink()
            return subprocess.CompletedProcess(command, 0)

        with patch("tools.run_ghidra_ppu_inventory.subprocess.run", side_effect=fake_run):
            with self.assertRaisesRegex(FileNotFoundError, "did not write its summary"):
                generate_inventories(
                    self.headless,
                    self.campaign_elf,
                    self.multiplayer_elf,
                    self.output_dir,
                    script_dir=self.script_dir,
                    campaign_opd=(0x1000, 0x40),
                )
        self.assertFalse((self.output_dir / "campaign_ppu_inventory.csv").exists())
        self.assertFalse((self.output_dir / "ppu_inventory_manifest.json").exists())

    def test_opd_seed_summary_must_match_requested_range_and_reconcile_counts(self):
        def fake_run(command, check):
            self._fake_run(command, check)
            if SEED_SCRIPT in command:
                summary_path = Path(command[command.index(SEED_SCRIPT) + 3])
                report = json.loads(summary_path.read_text(encoding="utf-8"))
                report["opd_va"] = "0x2000"
                summary_path.write_text(json.dumps(report), encoding="utf-8")
            return subprocess.CompletedProcess(command, 0)

        with patch("tools.run_ghidra_ppu_inventory.subprocess.run", side_effect=fake_run):
            with self.assertRaisesRegex(ValueError, "address does not match"):
                generate_inventories(
                    self.headless,
                    self.campaign_elf,
                    self.multiplayer_elf,
                    self.output_dir,
                    script_dir=self.script_dir,
                    campaign_opd=(0x1000, 0x40),
                )
        self.assertFalse((self.output_dir / "campaign_ppu_inventory.csv").exists())

    def test_opd_seed_script_requires_summary_and_fails_closed_on_empty_scan(self):
        source = (
            Path(__file__).resolve().parents[1] / "tools" / "ghidra"
            / "SeedGhostsOpdFunctions.java"
        ).read_text()
        for token in (
            "OPD_VA> <OPD_SIZE> <SUMMARY_JSON>",
            "OPD_SEEDING_COMPLETED",
            "OPD_SEEDING_EMPTY",
            "Files.write(Paths.get(args[2])",
            r'\"schema_version\": 2',
            "descriptorSlotsScanned++",
            "emptyDescriptors++",
            "unreadableDescriptors++",
            "invalidCodePointers++",
            "validCodePointers == 0",
            "throw new IllegalStateException",
            "throw new IllegalArgumentException",
            "mappedTocValues",
            "unmappedTocValues",
            "zeroTocDescriptors",
        ):
            with self.subTest(token=token):
                self.assertIn(token, source)

    def test_opd_seed_summary_with_no_created_or_existing_functions_is_rejected(self):
        def fake_run(command, check):
            self._fake_run(command, check)
            if SEED_SCRIPT in command:
                summary_path = Path(command[command.index(SEED_SCRIPT) + 3])
                report = json.loads(summary_path.read_text(encoding="utf-8"))
                report["functions_created"] = 0
                report["functions_already_present"] = 0
                report["functions_failed_to_create"] = report["valid_code_pointers"]
                summary_path.write_text(json.dumps(report), encoding="utf-8")
            return subprocess.CompletedProcess(command, 0)

        with patch("tools.run_ghidra_ppu_inventory.subprocess.run", side_effect=fake_run):
            with self.assertRaisesRegex(ValueError, "created or found no functions"):
                generate_inventories(
                    self.headless,
                    self.campaign_elf,
                    self.multiplayer_elf,
                    self.output_dir,
                    script_dir=self.script_dir,
                    campaign_opd=(0x1000, 0x40),
                )
        self.assertFalse((self.output_dir / "campaign_ppu_inventory.csv").exists())

    def test_opd_seed_summary_rejects_unreconciled_toc_mapping_counts(self):
        def fake_run(command, check):
            self._fake_run(command, check)
            if SEED_SCRIPT in command:
                summary_path = Path(command[command.index(SEED_SCRIPT) + 3])
                report = json.loads(summary_path.read_text(encoding="utf-8"))
                report["distinct_toc_values"] = 3
                report["mapped_toc_values"] = 2
                report["unmapped_toc_values"] = 0
                summary_path.write_text(json.dumps(report), encoding="utf-8")
            return subprocess.CompletedProcess(command, 0)

        with patch("tools.run_ghidra_ppu_inventory.subprocess.run", side_effect=fake_run):
            with self.assertRaisesRegex(ValueError, "TOC mapping counts do not reconcile"):
                generate_inventories(
                    self.headless,
                    self.campaign_elf,
                    self.multiplayer_elf,
                    self.output_dir,
                    script_dir=self.script_dir,
                    campaign_opd=(0x1000, 0x40),
                )
        self.assertFalse((self.output_dir / "campaign_ppu_inventory.csv").exists())

    def test_direct_api_rejects_boolean_opd_values(self):
        with patch("tools.run_ghidra_ppu_inventory.subprocess.run") as run:
            with self.assertRaisesRegex(ValueError, "must be integers"):
                generate_inventories(
                    self.headless,
                    self.campaign_elf,
                    self.multiplayer_elf,
                    self.output_dir,
                    script_dir=self.script_dir,
                    campaign_opd=(False, 8),
                )
        run.assert_not_called()

    def test_opd_summary_rejects_slot_count_that_disagrees_with_range(self):
        def fake_run(command, check):
            self._fake_run(command, check)
            if SEED_SCRIPT in command:
                summary_path = Path(command[command.index(SEED_SCRIPT) + 3])
                report = json.loads(summary_path.read_text(encoding="utf-8"))
                report["descriptor_slots_scanned"] = 7
                summary_path.write_text(json.dumps(report), encoding="utf-8")
            return subprocess.CompletedProcess(command, 0)

        with patch("tools.run_ghidra_ppu_inventory.subprocess.run", side_effect=fake_run):
            with self.assertRaisesRegex(ValueError, "slot count does not match"):
                generate_inventories(
                    self.headless, self.campaign_elf, self.multiplayer_elf,
                    self.output_dir, script_dir=self.script_dir,
                    campaign_opd=(0x1000, 0x40),
                )
        self.assertFalse((self.output_dir / "campaign_ppu_inventory.csv").exists())

    def test_opd_summary_rejects_unclassified_descriptor_slots(self):
        def fake_run(command, check):
            self._fake_run(command, check)
            if SEED_SCRIPT in command:
                summary_path = Path(command[command.index(SEED_SCRIPT) + 3])
                report = json.loads(summary_path.read_text(encoding="utf-8"))
                report["empty_descriptors"] = 3
                summary_path.write_text(json.dumps(report), encoding="utf-8")
            return subprocess.CompletedProcess(command, 0)

        with patch("tools.run_ghidra_ppu_inventory.subprocess.run", side_effect=fake_run):
            with self.assertRaisesRegex(ValueError, "descriptor classifications do not reconcile"):
                generate_inventories(
                    self.headless, self.campaign_elf, self.multiplayer_elf,
                    self.output_dir, script_dir=self.script_dir,
                    campaign_opd=(0x1000, 0x40),
                )
        self.assertFalse((self.output_dir / "campaign_ppu_inventory.csv").exists())

    def test_opd_summary_rejects_unclassified_code_pointers(self):
        def fake_run(command, check):
            self._fake_run(command, check)
            if SEED_SCRIPT in command:
                summary_path = Path(command[command.index(SEED_SCRIPT) + 3])
                report = json.loads(summary_path.read_text(encoding="utf-8"))
                report["invalid_code_pointers"] = 0
                summary_path.write_text(json.dumps(report), encoding="utf-8")
            return subprocess.CompletedProcess(command, 0)

        with patch("tools.run_ghidra_ppu_inventory.subprocess.run", side_effect=fake_run):
            with self.assertRaisesRegex(ValueError, "code-pointer counts do not reconcile"):
                generate_inventories(
                    self.headless, self.campaign_elf, self.multiplayer_elf,
                    self.output_dir, script_dir=self.script_dir,
                    campaign_opd=(0x1000, 0x40),
                )
        self.assertFalse((self.output_dir / "campaign_ppu_inventory.csv").exists())


    def test_inspector_report_selects_opd_range_and_records_provenance(self):
        report_path = self.write_inspector_report(
            self.root / "campaign-inspector.json", self.campaign_elf
        )
        with patch("tools.run_ghidra_ppu_inventory.subprocess.run", side_effect=self._fake_run) as run:
            manifest = generate_inventories(
                self.headless,
                self.campaign_elf,
                self.multiplayer_elf,
                self.output_dir,
                script_dir=self.script_dir,
                campaign_opd_inspector_report=report_path,
            )
        command = run.call_args_list[0].args[0]
        seed_index = command.index(SEED_SCRIPT)
        self.assertEqual(command[seed_index + 1], "0x2000")
        self.assertEqual(command[seed_index + 2], "0x10")
        export_index = command.index(EXPORT_SCRIPT)
        self.assertEqual(command[export_index + 2], "0x2000")
        self.assertEqual(command[export_index + 3], "0x10")
        evidence = manifest["campaign"]["opd_seeding"]["range_evidence"]
        self.assertEqual(evidence["source"], "ELF_INSPECTOR_REPORT")
        self.assertEqual(evidence["report_sha256"], hashlib.sha256(report_path.read_bytes()).hexdigest())
        self.assertEqual(evidence["input_elf_sha256"], hashlib.sha256(self.campaign_elf.read_bytes()).hexdigest())
        self.assertFalse(manifest["multiplayer"]["opd_seeding"]["enabled"])

    def test_inspector_report_for_different_elf_is_rejected_before_ghidra(self):
        report_path = self.write_inspector_report(
            self.root / "campaign-inspector.json", self.campaign_elf
        )
        report = json.loads(report_path.read_text(encoding="utf-8"))
        report["input"]["sha256"] = "0" * 64
        report_path.write_text(json.dumps(report), encoding="utf-8")
        with patch("tools.run_ghidra_ppu_inventory.subprocess.run") as run:
            with self.assertRaisesRegex(ValueError, "does not match the supplied ELF"):
                generate_inventories(
                    self.headless, self.campaign_elf, self.multiplayer_elf,
                    self.output_dir, script_dir=self.script_dir,
                    campaign_opd_inspector_report=report_path,
                )
        run.assert_not_called()

    def test_inspector_report_with_invalid_section_flags_is_rejected(self):
        report_path = self.write_inspector_report(
            self.root / "campaign-inspector.json", self.campaign_elf, valid=False
        )
        with patch("tools.run_ghidra_ppu_inventory.subprocess.run") as run:
            with self.assertRaisesRegex(ValueError, "section_alignment_valid"):
                generate_inventories(
                    self.headless, self.campaign_elf, self.multiplayer_elf,
                    self.output_dir, script_dir=self.script_dir,
                    campaign_opd_inspector_report=report_path,
                )
        run.assert_not_called()

    def test_manual_opd_range_and_inspector_report_are_mutually_exclusive(self):
        report_path = self.write_inspector_report(
            self.root / "campaign-inspector.json", self.campaign_elf
        )
        with patch("tools.run_ghidra_ppu_inventory.subprocess.run") as run:
            with self.assertRaisesRegex(ValueError, "either a manual OPD range"):
                generate_inventories(
                    self.headless, self.campaign_elf, self.multiplayer_elf,
                    self.output_dir, script_dir=self.script_dir,
                    campaign_opd=(0x1000, 0x40),
                    campaign_opd_inspector_report=report_path,
                )
        run.assert_not_called()

    def test_input_elf_change_during_ghidra_analysis_prevents_promotion(self):
        def fake_run(command, check):
            self._fake_run(command, check)
            if "Campaign" in command[2]:
                self.campaign_elf.write_bytes(b"mutated-during-analysis")
            return subprocess.CompletedProcess(command, 0)

        with patch("tools.run_ghidra_ppu_inventory.subprocess.run", side_effect=fake_run):
            with self.assertRaisesRegex(ValueError, "input ELF changed during Ghidra analysis"):
                generate_inventories(
                    self.headless, self.campaign_elf, self.multiplayer_elf,
                    self.output_dir, script_dir=self.script_dir,
                )
        self.assertFalse((self.output_dir / "campaign_ppu_inventory.csv").exists())
        self.assertFalse((self.output_dir / "ppu_inventory_manifest.json").exists())


    def test_inspector_report_with_forged_opd_candidate_is_rejected(self):
        report_path = self.write_inspector_report(
            self.root / "campaign-inspector.json", self.campaign_elf
        )
        report = json.loads(report_path.read_text(encoding="utf-8"))
        report["elf"]["opd"]["operator_seed_range_candidate"]["size"] = "0x80"
        report_path.write_text(json.dumps(report), encoding="utf-8")
        with patch("tools.run_ghidra_ppu_inventory.subprocess.run") as run:
            with self.assertRaisesRegex(ValueError, "candidate size does not match"):
                generate_inventories(
                    self.headless, self.campaign_elf, self.multiplayer_elf,
                    self.output_dir, script_dir=self.script_dir,
                    campaign_opd_inspector_report=report_path,
                )
        run.assert_not_called()



    def test_opd_resolution_mismatch_does_not_promote_partial_inventory(self):
        def bad_resolution_run(command, check):
            self._fake_run(command, check)
            if EXPORT_SCRIPT in command:
                export_index = command.index(EXPORT_SCRIPT)
                if command[export_index + 2] != "-postScript":
                    summary_path = Path(command[export_index + 4])
                    summary = json.loads(summary_path.read_text(encoding="utf-8"))
                    summary["exact_entry_matches"] = 2
                    summary_path.write_text(json.dumps(summary), encoding="utf-8")

        with patch(
            "tools.run_ghidra_ppu_inventory.subprocess.run",
            side_effect=bad_resolution_run,
        ):
            with self.assertRaisesRegex(ValueError, "matches do not reconcile"):
                generate_inventories(
                    self.headless,
                    self.campaign_elf,
                    self.multiplayer_elf,
                    self.output_dir,
                    script_dir=self.script_dir,
                    campaign_opd=(0x1000, 0x40),
                )
        self.assertFalse((self.output_dir / "campaign_ppu_inventory.csv").exists())
        self.assertFalse((self.output_dir / "ppu_inventory_manifest.json").exists())

if __name__ == "__main__":
    unittest.main()
