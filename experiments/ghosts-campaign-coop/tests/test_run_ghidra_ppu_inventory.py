import csv
import hashlib
import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import call, patch

from tools.run_ghidra_ppu_inventory import (
    EXPORT_SCRIPT,
    XREF_SCRIPT,
    PROCESSOR,
    SEED_SCRIPT,
    _validate_opd_pair,
    _validate_opd_tuple,
    generate_inventories,
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

    def _write_inventory(self, path, name, fingerprint):
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

    def _fake_run(self, command, check):
        export_index = command.index(EXPORT_SCRIPT)
        xref_index = command.index(XREF_SCRIPT)
        output_csv = Path(command[export_index + 1])
        output_xref_csv = Path(command[xref_index + 1])
        if "Campaign" in command[2]:
            self._write_inventory(output_csv, "Campaign", "a" * 64)
            self._write_xrefs(output_xref_csv, "Campaign")
        else:
            self._write_inventory(output_csv, "Multiplayer", "b" * 64)
            self._write_xrefs(output_xref_csv, "Multiplayer")
        if SEED_SCRIPT in command:
            seed_index = command.index(SEED_SCRIPT)
            summary_path = Path(command[seed_index + 3])
            summary_path.write_text(json.dumps({
                "schema_version": 1,
                "status": "OPD_SEEDING_COMPLETED",
                "opd_va": command[seed_index + 1],
                "opd_size": command[seed_index + 2],
                "descriptors_visited": 4,
                "valid_code_pointers": 3,
                "functions_created": 2,
                "functions_already_present": 1,
                "functions_failed_to_create": 0,
                "invalid_entries": 1,
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
            "validCodePointers == 0",
            "throw new IllegalStateException",
            "throw new IllegalArgumentException",
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

if __name__ == "__main__":
    unittest.main()
