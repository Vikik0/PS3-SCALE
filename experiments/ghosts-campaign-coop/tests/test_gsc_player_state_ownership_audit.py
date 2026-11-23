import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from gsc_player_state_ownership_audit import audit_mission


class GscPlayerStateOwnershipAuditTests(unittest.TestCase):
    def test_separates_player_methods_from_level_scope_state(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "mission.gsc").write_text(
                'scripted_action()\n{\n'
                '    level.player giveweapon("weapon");\n'
                '    level.player freezecontrols(1);\n'
                '    level.player playerlinktoblend(rig, "tag_player", 0.5);\n'
                '    level.mission_flag = 1;\n'
                '    common_scripts\\utility::flag_set("mission_done");\n'
                '    setsaveddvar("ammoCounterHide", 1);\n'
                '}\n', encoding="utf-8"
            )
            report = audit_mission("test", root, ["scripted_action"])
            finding = report["findings"][0]
            self.assertEqual(finding["player_method_categories"]["weapons"], 1)
            self.assertEqual(finding["player_method_categories"]["movement_control"], 2)
            self.assertEqual(finding["level_scope_field_references"]["mission_flag"], 1)
            self.assertEqual(finding["mission_flag_api_call_count"], 1)
            self.assertEqual(finding["dvar_api_call_count"], 1)
            self.assertEqual(finding["ownership_pattern"], "MIXED_PER_PLAYER_ACTIONS_AND_LEVEL_SCOPE_STATE")

    def test_player_array_reference_is_marked_as_array_aware(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "ai.gsc").write_text(
                'threat_loop()\n{\n'
                '    foreach ( player in level.playerpiggyback )\n'
                '        player waittill("death");\n'
                '}\n', encoding="utf-8"
            )
            report = audit_mission("test", root, ["threat_loop"])
            finding = report["findings"][0]
            self.assertEqual(finding["player_array_references"]["level.playerpiggyback"], 1)
            self.assertEqual(finding["ownership_pattern"], "PLAYER_ARRAY_AWARE_PATH")

    def test_secondary_client_reference_is_not_misclassified_as_player_array(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "dog.gsc").write_text(
                'dog_view()\n{\n'
                '    var_0 hideonclient(level.playengineeffects);\n'
                '    var_1 hideonclient(level.player);\n'
                '}\n', encoding="utf-8"
            )
            report = audit_mission("test", root, ["dog_view"])
            finding = report["findings"][0]
            self.assertEqual(finding["player_array_references"]["level.playerpiggyback"], 0)
            self.assertEqual(finding["secondary_client_reference_count"], 1)
            self.assertEqual(finding["ownership_pattern"], "SECONDARY_CLIENT_REFERENCE_PATH")

    def test_comments_and_strings_do_not_create_player_references(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "mission.gsc").write_text(
                'helper()\n{\n'
                '    // level.player giveweapon("fake");\n'
                '    self iprintln("level.player freezecontrols(1)");\n'
                '}\n', encoding="utf-8"
            )
            report = audit_mission("test", root, ["helper"])
            finding = report["findings"][0]
            self.assertEqual(finding["level_player_reference_count"], 0)
            self.assertEqual(finding["player_method_calls"], {})

    def test_missing_target_is_reported_without_guessing(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "mission.gsc").write_text('main()\n{\n return;\n}\n', encoding="utf-8")
            report = audit_mission("test", root, ["not_present"])
            self.assertEqual(report["target_functions_found"], 0)
            self.assertEqual(report["missing_target_functions"], ["not_present"])


if __name__ == "__main__":
    unittest.main()
