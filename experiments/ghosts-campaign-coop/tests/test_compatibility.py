import unittest

from server.compatibility import GAME_PROFILES, inspect_install, list_profiles


class CompatibilityTests(unittest.TestCase):
    def test_known_regional_title_ids_are_registered(self):
        expected = {
            "BLUS31270", "NPUB31301", "NPUB50021", "BLES01945", "BLES01948",
            "NPEB01832", "NPEB01835", "BLJM61125", "BLJM61126", "NPJB00528",
        }
        self.assertEqual(set(GAME_PROFILES), expected)

    def test_registry_is_explicitly_unverified(self):
        data = list_profiles()
        self.assertEqual(data["target_title_update"], "1.16")
        self.assertEqual(data["target_firmware"]["status"], "target_only_not_hardware_verified")

    def test_first_target_is_not_misreported_as_supported(self):
        result = inspect_install("BLUS31270", "1.16", "CEX", "4.93")
        self.assertEqual(result["status"], "target_profile_not_hardware_verified")
        self.assertIsNone(result["compatible"])

    def test_unknown_title_id_is_rejected(self):
        result = inspect_install("BLES99999", "1.16", "CEX", "4.93")
        self.assertEqual(result["status"], "unknown_title_id")
        self.assertFalse(result["compatible"])

    def test_other_title_update_is_rejected(self):
        result = inspect_install("BLUS31270", "1.15", "CEX", "4.93")
        self.assertEqual(result["status"], "unsupported_title_update")

    def test_dex_requires_a_separate_build(self):
        result = inspect_install("BLUS31270", "1.16", "DEX", "4.93")
        self.assertEqual(result["status"], "unsupported_firmware_type")

    def test_other_cex_firmware_is_not_assumed_compatible(self):
        result = inspect_install("BLUS31270", "1.16", "CEX", "4.92")
        self.assertEqual(result["status"], "firmware_not_yet_verified")


if __name__ == "__main__":
    unittest.main()
