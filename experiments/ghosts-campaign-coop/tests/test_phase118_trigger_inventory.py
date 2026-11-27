import copy
import json
import unittest
from pathlib import Path

from tools.verify_phase118_trigger_inventory import validate_files, validate_inventory

ROOT = Path(__file__).resolve().parents[1]
INVENTORY_PATH = ROOT / "reports" / "PHASE118_TRIGGER_CALLSITE_INVENTORY.json"
VALIDATION_PATH = ROOT / "reports" / "PHASE118_VALIDATION.json"
REPORT_PATH = ROOT / "reports" / "PHASE118_REPORT_AR.md"


def baseline():
    return (
        json.loads(INVENTORY_PATH.read_text(encoding="utf-8")),
        json.loads(VALIDATION_PATH.read_text(encoding="utf-8")),
        REPORT_PATH.read_text(encoding="utf-8"),
    )


class Phase118InventoryValidatorTests(unittest.TestCase):
    def test_repository_inventory_passes(self):
        self.assertEqual(validate_files(INVENTORY_PATH, VALIDATION_PATH, REPORT_PATH), [])

    def test_rejects_duplicate_callsite(self):
        inventory, validation, report = baseline()
        inventory["callsites"][1] = copy.deepcopy(inventory["callsites"][0])
        errors = validate_inventory(inventory, validation, report)
        self.assertTrue(any("duplicates an existing call site" in error for error in errors))

    def test_rejects_invalid_source_line(self):
        inventory, validation, report = baseline()
        inventory["callsites"][0]["line"] = 0
        errors = validate_inventory(inventory, validation, report)
        self.assertTrue(any("line must be a positive integer" in error for error in errors))

    def test_rejects_malformed_source_hash(self):
        inventory, validation, report = baseline()
        inventory["callsites"][0]["source_sha256"] = "not-a-sha"
        errors = validate_inventory(inventory, validation, report)
        self.assertTrue(any("source SHA-256 is invalid" in error for error in errors))

    def test_rejects_wrong_mission_distribution(self):
        inventory, validation, report = baseline()
        inventory["summary"]["cornered"] = 9
        errors = validate_inventory(inventory, validation, report)
        self.assertTrue(any("summary count mismatch for Cornered" in error for error in errors))

    def test_rejects_claim_that_a_shared_interaction_is_ready(self):
        inventory, validation, report = baseline()
        inventory["critical_shared_interactions"][0]["status"] = "READY"
        errors = validate_inventory(inventory, validation, report)
        self.assertTrue(any("must remain blocked" in error for error in errors))

    def test_rejects_premature_native_player_two_claim(self):
        inventory, validation, report = baseline()
        inventory["runtime_boundary"]["native_campaign_player_two_proven"] = True
        errors = validate_inventory(inventory, validation, report)
        self.assertTrue(any("must not claim native Campaign Player 2 is proven" in error for error in errors))

    def test_rejects_validation_archive_hash_mismatch(self):
        inventory, validation, report = baseline()
        validation["source_archive_sha256"] = "0" * 64
        errors = validate_inventory(inventory, validation, report)
        self.assertTrue(any("does not match the inventory" in error for error in errors))

    def test_rejects_report_that_omits_runtime_boundary(self):
        inventory, validation, _ = baseline()
        errors = validate_inventory(inventory, validation, "تقرير غير مكتمل")
        self.assertTrue(any("must disclose Player 2 NOT_PROVEN and 0/4" in error for error in errors))


    def test_rejects_archive_hash_changed_even_if_both_json_files_match(self):
        inventory, validation, report = baseline()
        inventory["source_archive"]["sha256"] = "0" * 64
        validation["source_archive_sha256"] = "0" * 64
        errors = validate_inventory(inventory, validation, report)
        self.assertTrue(any("source archive SHA-256 does not match the pinned reference" in error for error in errors))

    def test_rejects_positive_but_wrong_source_line(self):
        inventory, validation, report = baseline()
        inventory["callsites"][0]["line"] += 1
        errors = validate_inventory(inventory, validation, report)
        self.assertTrue(any("call-site identities do not match the pinned source inventory" in error for error in errors))

    def test_rejects_source_file_hash_not_matching_pinned_source(self):
        inventory, validation, report = baseline()
        inventory["callsites"][0]["source_sha256"] = "0" * 64
        errors = validate_inventory(inventory, validation, report)
        self.assertTrue(any("source file SHA-256 does not match the pinned source" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
