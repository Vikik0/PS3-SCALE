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
    PROCESSOR,
    SEED_SCRIPT,
    _validate_opd_pair,
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

    def _fake_run(self, command, check):
        export_index = command.index(EXPORT_SCRIPT)
        output_csv = Path(command[export_index + 1])
        if "Campaign" in command[2]:
            self._write_inventory(output_csv, "Campaign", "a" * 64)
        else:
            self._write_inventory(output_csv, "Multiplayer", "b" * 64)
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
        self.assertNotIn(SEED_SCRIPT, multiplayer_command)
        self.assertIn(EXPORT_SCRIPT, campaign_command)
        self.assertIn(EXPORT_SCRIPT, multiplayer_command)
        self.assertIn("-deleteProject", campaign_command)
        self.assertIn("-deleteProject", multiplayer_command)

        campaign_hash = hashlib.sha256(self.campaign_elf.read_bytes()).hexdigest()
        multiplayer_hash = hashlib.sha256(self.multiplayer_elf.read_bytes()).hexdigest()
        self.assertEqual(manifest["campaign"]["elf_sha256"], campaign_hash)
        self.assertEqual(manifest["multiplayer"]["elf_sha256"], multiplayer_hash)
        self.assertTrue(manifest["campaign"]["opd_seeding"]["enabled"])
        self.assertFalse(manifest["multiplayer"]["opd_seeding"]["enabled"])

        saved = json.loads(
            (self.output_dir / "ppu_inventory_manifest.json").read_text(encoding="utf-8")
        )
        self.assertEqual(saved["campaign"]["function_records"], 1)
        self.assertEqual(saved["multiplayer"]["function_records"], 1)
        self.assertTrue((self.output_dir / "campaign_ppu_inventory.csv").is_file())
        self.assertTrue((self.output_dir / "multiplayer_ppu_inventory.csv").is_file())

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
        self.assertFalse((self.output_dir / "ppu_inventory_manifest.json").exists())


if __name__ == "__main__":
    unittest.main()
