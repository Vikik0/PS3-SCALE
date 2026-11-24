import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools import run_ghosts_tu116_native_analysis as pipeline


def retail_preflight(native_ready=False, status="MATCHED_RETAIL_INPUTS"):
    return {
        "schema_version": 1,
        "status": status,
        "errors": [],
        "readiness": {
            "retail_input_identity_verified": status == "MATCHED_RETAIL_INPUTS",
            "native_ppu_comparison_ready": native_ready,
        },
    }


def inspector_report(name, digest, entry, opd_range):
    return {
        "input": {"file_name": name, "size_bytes": 4096, "sha256": digest},
        "elf": {
            "entry_hex": entry,
            "machine_name": "EM_PPC64",
            "endianness": "big",
            "phnum": 8,
            "opd": {
                "section_name": ".opd",
                "section_address": "0x1000",
                "section_size": "0x80",
                "descriptor_slots_total": 16,
                "descriptor_slots_scanned": 16,
                "valid_code_pointers": 12,
                "invalid_code_pointers": 4,
                "operator_seed_range_candidate": opd_range,
            },
        },
    }


class RunGhostsNativeAnalysisTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.disc = self.root / "disc"
        self.update = self.root / "update"
        self.disc.mkdir()
        self.update.mkdir()
        self.output = self.root / "analysis"
        self.campaign = self.root / "campaign.elf"
        self.multiplayer = self.root / "multiplayer.elf"
        self.campaign.write_bytes(b"campaign decrypted PPU fixture")
        self.multiplayer.write_bytes(b"multiplayer decrypted PPU fixture")
        self.headless = self.root / "analyzeHeadless"
        self.headless.write_text("fixture", encoding="utf-8")
        self.campaign_range = {
            "virtual_address": "0x1000", "size": "0x80",
            "section_name": ".opd", "section_index": 7,
        }
        self.multiplayer_range = {
            "virtual_address": "0x2000", "size": "0x80",
            "section_name": ".opd", "section_index": 7,
        }

    def tearDown(self):
        self.temp.cleanup()

    def test_encrypted_retail_inputs_only_do_not_invoke_ghidra(self):
        with patch.object(pipeline, "verify_inputs", return_value=retail_preflight(False)), patch.object(
            pipeline, "generate_inventories"
        ) as generate:
            code, report = pipeline.run_pipeline(self.disc, self.update, self.output)
        self.assertEqual(code, 0)
        self.assertEqual(report["status"], "WAITING_FOR_BOTH_DECRYPTED_PPU_ELFS")
        self.assertFalse(report["ghidra_invoked"])
        generate.assert_not_called()
        self.assertTrue((self.output / "native_input_preflight.json").is_file())
        self.assertTrue((self.output / "native_analysis_status.json").is_file())

    def test_verified_elfs_without_ghidra_report_ready_but_not_run(self):
        with patch.object(pipeline, "verify_inputs", return_value=retail_preflight(True)):
            code, report = pipeline.run_pipeline(
                self.disc, self.update, self.output, self.campaign, self.multiplayer
            )
        self.assertEqual(code, 0)
        self.assertEqual(report["status"], "DECRYPTED_ELFS_VERIFIED_GHIDRA_NOT_REQUESTED")
        self.assertFalse(report["ghidra_invoked"])

    def test_invalid_retail_preflight_blocks_before_elf_inspection(self):
        with patch.object(
            pipeline, "verify_inputs",
            return_value=retail_preflight(False, "MISMATCH_OR_INCOMPLETE"),
        ), patch.object(pipeline, "inspect_file") as inspect, patch.object(
            pipeline, "generate_inventories"
        ) as generate:
            code, report = pipeline.run_pipeline(self.disc, self.update, self.output)
        self.assertEqual(code, 1)
        self.assertEqual(report["status"], "BLOCKED_RETAIL_INPUT_MISMATCH")
        inspect.assert_not_called()
        generate.assert_not_called()

    def test_missing_opd_range_blocks_without_guessing_or_running_ghidra(self):
        with patch.object(pipeline, "verify_inputs", return_value=retail_preflight(True)), patch.object(
            pipeline, "inspect_file",
            side_effect=[
                inspector_report("campaign.elf", "a" * 64, "0x7E9508", None),
                inspector_report("multiplayer.elf", "b" * 64, "0xA75E38", self.multiplayer_range),
            ],
        ), patch.object(pipeline, "generate_inventories") as generate:
            code, report = pipeline.run_pipeline(
                self.disc, self.update, self.output,
                self.campaign, self.multiplayer, self.headless,
            )
        self.assertEqual(code, 2)
        self.assertEqual(report["status"], "BLOCKED_OPD_RANGE_NOT_VERIFIED")
        generate.assert_not_called()

    def test_valid_elf_pair_uses_hash_bound_inspector_reports_for_ghidra(self):
        fake_manifest = {
            "campaign": {"function_records": 100},
            "multiplayer": {"function_records": 120},
            "multiplayer_only_builtin_lead_count": 3,
        }

        def fake_generate(*args, **kwargs):
            out = Path(args[3])
            out.mkdir(parents=True, exist_ok=True)
            (out / pipeline.OUTPUT_NAMES[5]).write_text("{}", encoding="utf-8")
            campaign_report = json.loads(kwargs["campaign_opd_inspector_report"].read_text(encoding="utf-8"))
            multiplayer_report = json.loads(kwargs["multiplayer_opd_inspector_report"].read_text(encoding="utf-8"))
            self.assertEqual(campaign_report["input"]["sha256"], "a" * 64)
            self.assertEqual(multiplayer_report["input"]["sha256"], "b" * 64)
            self.assertEqual(
                campaign_report["elf"]["opd"]["operator_seed_range_candidate"]["virtual_address"],
                "0x1000",
            )
            return fake_manifest

        with patch.object(pipeline, "verify_inputs", return_value=retail_preflight(True)), patch.object(
            pipeline, "inspect_file",
            side_effect=[
                inspector_report("campaign.elf", "a" * 64, "0x7E9508", self.campaign_range),
                inspector_report("multiplayer.elf", "b" * 64, "0xA75E38", self.multiplayer_range),
            ],
        ), patch.object(pipeline, "generate_inventories", side_effect=fake_generate) as generate:
            code, report = pipeline.run_pipeline(
                self.disc, self.update, self.output,
                self.campaign, self.multiplayer, self.headless,
            )
        self.assertEqual(code, 0)
        self.assertEqual(report["status"], "NATIVE_PPU_INVENTORIES_GENERATED")
        self.assertTrue(report["ghidra_invoked"])
        self.assertEqual(report["campaign_function_records"], 100)
        self.assertEqual(report["multiplayer_only_builtin_lead_count"], 3)
        self.assertTrue(generate.called)
        self.assertTrue((self.output / "native_analysis_status.json").is_file())

    def test_missing_headless_executable_blocks_without_running_ghidra(self):
        missing = self.root / "missingAnalyzeHeadless"
        with patch.object(pipeline, "verify_inputs", return_value=retail_preflight(True)), patch.object(
            pipeline, "inspect_file"
        ) as inspect, patch.object(pipeline, "generate_inventories") as generate:
            code, report = pipeline.run_pipeline(
                self.disc, self.update, self.output,
                self.campaign, self.multiplayer, missing,
            )
        self.assertEqual(code, 2)
        self.assertEqual(report["status"], "BLOCKED_GHIDRA_EXECUTABLE_NOT_FOUND")
        inspect.assert_not_called()
        generate.assert_not_called()

    def test_refuses_to_overwrite_preflight_reports_without_flag(self):
        self.output.mkdir()
        (self.output / "native_input_preflight.json").write_text("{}", encoding="utf-8")
        with patch.object(pipeline, "verify_inputs", return_value=retail_preflight(False)):
            with self.assertRaises(FileExistsError):
                pipeline.run_pipeline(self.disc, self.update, self.output)



    def test_existing_inventory_output_blocks_before_preflight_or_verification(self):
        self.output.mkdir(parents=True)
        existing_inventory = self.output / pipeline.OUTPUT_NAMES[0]
        existing_inventory.write_text("keep-existing-inventory", encoding="utf-8")

        with patch.object(pipeline, "verify_inputs") as verify:
            with self.assertRaisesRegex(
                FileExistsError, "refusing to overwrite existing native analysis outputs"
            ):
                pipeline.run_pipeline(self.disc, self.update, self.output)

        verify.assert_not_called()
        self.assertEqual(
            existing_inventory.read_text(encoding="utf-8"),
            "keep-existing-inventory",
        )
        self.assertFalse((self.output / "native_input_preflight.json").exists())
        self.assertFalse((self.output / "native_analysis_status.json").exists())

if __name__ == "__main__":
    unittest.main()
