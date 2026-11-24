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



    def test_default_targets_are_scoped_to_exact_file_and_function(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            maps = root / "maps"
            maps.mkdir()
            (maps / "black_ice_exfil.gsc").write_text(
                "player_fail_finale_instakill()\n{\n"
                "    level.player kill();\n"
                "}\n",
                encoding="utf-8",
            )
            (root / "duplicate_name.gsc").write_text(
                "player_fail_finale_instakill()\n{\n"
                "    level.player giveweapon(\"wrong_target\");\n"
                "}\n",
                encoding="utf-8",
            )
            report = audit_mission("black_ice", root)

        self.assertEqual(report["target_function_count"], 21)
        self.assertEqual(report["target_functions_found"], 1)
        self.assertEqual(report["findings"][0]["file"], "maps/black_ice_exfil.gsc")
        self.assertEqual(report["findings"][0]["function"], "player_fail_finale_instakill")
        self.assertIn(
            "maps/black_ice_ascend.gsc::ascend_logic",
            report["missing_target_functions"],
        )

    def test_default_hotspot_inventory_contains_68_unique_file_function_pairs(self):
        from gsc_player_state_ownership_audit import DEFAULT_TARGETS

        flattened = [
            (mission, relative, function)
            for mission, specs in DEFAULT_TARGETS.items()
            for relative, function in specs
        ]
        self.assertEqual(len(flattened), 68)
        self.assertEqual(len(set(flattened)), 68)
        self.assertEqual(
            {mission: len(specs) for mission, specs in DEFAULT_TARGETS.items()},
            {"black_ice": 21, "carrier": 23, "cornered": 24},
        )


    def test_audit_reports_all_player_container_function_sites(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "container_paths.gsc"
            source.write_text(
                "container_scan()\n{\n"
                "    foreach (player in level.playerpiggyback)\n"
                "    {\n"
                "        player iprintln(\"piggyback\");\n"
                "    }\n"
                "    foreach (player in level.players)\n"
                "    {\n"
                "        player iprintln(\"players\");\n"
                "    }\n"
                "    level.playengineeffects doSomething();\n"
                "}\n"
                "ordinary_player_function()\n{\n"
                "    level.player giveweapon(\"rifle\");\n"
                "}\n",
                encoding="utf-8",
            )
            report = audit_mission("synthetic", root, ["container_scan"])

        self.assertEqual(report["target_functions_found"], 1)
        self.assertEqual(report["player_container_function_count"], 1)
        self.assertEqual(
            report["player_container_summary"]["player_array_references"],
            {"level.playerpiggyback": 1, "level.players": 1},
        )
        self.assertEqual(
            report["player_container_summary"]["secondary_client_reference_count"],
            1,
        )
        self.assertEqual(
            report["player_container_functions"][0]["function"],
            "container_scan",
        )

if __name__ == "__main__":
    unittest.main()
