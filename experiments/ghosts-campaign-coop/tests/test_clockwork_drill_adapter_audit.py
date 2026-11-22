import tempfile
import unittest
from pathlib import Path

from tools.clockwork_drill_adapter_audit import audit, main


class ClockworkDrillAdapterAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source = self.root / "maps__clockwork_interior.gsc"

    def tearDown(self):
        self.temp.cleanup()

    def write_source(self, content):
        if "handle_misc_drill_details(" not in content:
            content += (
                "\nhandle_misc_drill_details()\n"
                "{\n"
                "}\n"
            )
        self.source.write_text(content, encoding="utf-8")
        return self.source

    def test_original_single_player_drill_is_blocked_and_reports_dependencies(self):
        report = audit(self.write_source(
            'handle_drill_spot()\n'
            '{\n'
            '    if ( level.player usebuttonpressed( "use" ) )\n'
            '    {\n'
            '        level.player setactionslot( 1, "weapon" );\n'
            '        thread handle_drilling();\n'
            '    }\n'
            '}\n'
            'handle_drilling()\n'
            '{\n'
            '    if ( level.player adsbuttonpressed( "ads" ) )\n'
            '        level.player takeweapon( "drill" );\n'
            '}\n'
        ))

        self.assertEqual(report["status"], "BLOCKED_INCOMPLETE_PLAYER_SCOPING")
        self.assertEqual(report["call_graph"]["handle_drill_spot"], [
            "handle_drill_spot", "handle_drilling"
        ])
        self.assertEqual(
            report["unscoped_level_player_functions"],
            ["handle_drill_spot", "handle_drilling"],
        )
        entry = report["target_functions"]["handle_drill_spot"][0]
        self.assertEqual(entry["level_player_refs"], 2)
        self.assertIn("input", entry["player_method_categories"])
        self.assertIn("hud_interaction", entry["player_method_categories"])

    def test_player_scoped_adapter_passes_static_source_gate(self):
        report = audit(self.write_source(
            'handle_drill_spot( player, spot )\n'
            '{\n'
            '    if ( !ghc_campaign_drill_begin( player, spot ) )\n'
            '        return;\n'
            '    thread handle_drilling( player, spot );\n'
            '}\n'
            'handle_drilling( player, spot )\n'
            '{\n'
            '    if ( player adsbuttonpressed( "ads" ) )\n'
            '        ghc_campaign_drill_complete( player, spot );\n'
            '    else\n'
            '        ghc_campaign_drill_cancel( player, spot );\n'
            '}\n'
            'cleanup_drill( player )\n'
            '{\n'
            '    ghc_campaign_drill_abort_player( player );\n'
            '}\n'
        ))

        self.assertEqual(report["status"], "SOURCE_ADAPTER_READY_FOR_MANUAL_REVIEW")
        self.assertTrue(all(report["checks"].values()))
        self.assertEqual(
            report["target_functions"]["handle_drill_spot"][0]["parameters"],
            ["player", "spot"],
        )
        self.assertEqual(report["unscoped_level_player_functions"], [])

    def test_transitive_helper_with_global_player_reference_blocks_adapter(self):
        report = audit(self.write_source(
            'handle_drill_spot( player, spot )\n'
            '{\n'
            '    if ( !ghc_campaign_drill_begin( player, spot ) ) return;\n'
            '    thread handle_drilling( player, spot );\n'
            '}\n'
            'handle_drilling( player, spot )\n'
            '{\n'
            '    thread update_drill( player, spot );\n'
            '}\n'
            'update_drill( player, spot )\n'
            '{\n'
            '    level.player setactionslot( 1, "drill" );\n'
            '    ghc_campaign_drill_complete( player, spot );\n'
            '    ghc_campaign_drill_cancel( player, spot );\n'
            '}\n'
        ))

        self.assertEqual(report["status"], "BLOCKED_INCOMPLETE_PLAYER_SCOPING")
        self.assertEqual(report["unscoped_level_player_functions"], ["update_drill"])
        self.assertIn("update_drill", report["call_graph"]["handle_drill_spot"])

    def test_comments_and_string_literals_do_not_fake_adapter_markers(self):
        report = audit(self.write_source(
            'handle_drill_spot( player, spot )\n'
            '{\n'
            '    // ghc_campaign_drill_begin( player, spot );\n'
            '    iprintln( "ghc_campaign_drill_begin( player, spot )" );\n'
            '    thread handle_drilling( player, spot );\n'
            '}\n'
            'handle_drilling( player, spot )\n'
            '{\n'
            '    ghc_campaign_drill_complete( player, spot );\n'
            '    ghc_campaign_drill_cancel( player, spot );\n'
            '}\n'
        ))

        self.assertEqual(report["status"], "BLOCKED_INCOMPLETE_PLAYER_SCOPING")
        self.assertFalse(report["checks"]["begin_called_from_handle_drill_spot"])
        self.assertEqual(report["adapter_marker_locations"]["begin"], [])

    def test_missing_target_function_is_reported_as_incomplete_source(self):
        report = audit(self.write_source(
            'handle_drill_spot()\n'
            '{\n'
            '    level.player usebuttonpressed( "use" );\n'
            '}\n'
        ))

        self.assertEqual(report["status"], "SOURCE_INCOMPLETE")
        self.assertEqual(report["checks"]["all_core_drill_functions_found"], False)
        self.assertEqual(report["target_functions"]["handle_drilling"], [])

    def test_missing_abort_cleanup_keeps_adapter_blocked(self):
        report = audit(self.write_source(
            'handle_drill_spot( player, spot )\n'
            '{\n'
            '    if ( !ghc_campaign_drill_begin( player, spot ) ) return;\n'
            '    thread handle_drilling( player, spot );\n'
            '}\n'
            'handle_drilling( player, spot )\n'
            '{\n'
            '    ghc_campaign_drill_complete( player, spot );\n'
            '    ghc_campaign_drill_cancel( player, spot );\n'
            '}\n'
        ))

        self.assertEqual(report["status"], "BLOCKED_INCOMPLETE_PLAYER_SCOPING")
        self.assertFalse(report["checks"]["abort_player_helper_referenced_in_source"])

    def test_json_output_cannot_overwrite_input_gsc(self):
        source = self.write_source(
            'handle_drill_spot()\n{\n level.player usebuttonpressed( "use" );\n}\n'
            'handle_drilling()\n{\n}\n'
        )
        original = source.read_text(encoding="utf-8")
        result = main([str(source), "--json-out", str(source)])
        self.assertEqual(result, 1)
        self.assertEqual(source.read_text(encoding="utf-8"), original)

    def test_logan_only_switchmsg_is_reported_but_not_treated_as_p2_scope_leak(self):
        report = audit(self.write_source(
            'handle_drill_spot( player, spot )\n'
            '{\n'
            '    ghc_campaign_drill_begin( player, spot );\n'
            '}\n'
            'handle_drilling( player, spot )\n'
            '{\n'
            '    ghc_campaign_drill_complete( player, spot );\n'
            '    ghc_campaign_drill_cancel( player, spot );\n'
            '}\n'
            'cleanup_drill( player )\n'
            '{\n'
            '    ghc_campaign_drill_abort_player( player );\n'
            '}\n'
            'switchmsg()\n'
            '{\n'
            '    level.player giveweapon( "drill_press" );\n'
            '}\n'
        ))

        self.assertEqual(report["status"], "SOURCE_ADAPTER_READY_FOR_MANUAL_REVIEW")
        self.assertIn("switchmsg", report["primary_only_function_details"])
        self.assertGreater(report["primary_only_level_player_refs"], 0)
        self.assertNotIn("switchmsg", report["unscoped_level_player_functions"])

    def test_related_drill_support_functions_are_audited_as_independent_roots(self):
        report = audit(self.write_source(
            'handle_drill_spot( player, spot )\n'
            '{\n'
            '    ghc_campaign_drill_begin( player, spot );\n'
            '    thread handle_drilling( player, spot );\n'
            '}\n'
            'handle_drilling( player, spot )\n'
            '{\n'
            '    ghc_campaign_drill_complete( player, spot );\n'
            '    ghc_campaign_drill_cancel( player, spot );\n'
            '}\n'
            'drillthrough_plate_sound()\n'
            '{\n'
            '    level.player maps\\\\::play_sound( "drill" );\n'
            '}\n'
        ))

        self.assertIn("drillthrough_plate_sound", report["related_function_details"])
        self.assertIn("drillthrough_plate_sound", report["call_graph"])
        self.assertIn("drillthrough_plate_sound", report["unscoped_level_player_functions"])
        self.assertEqual(report["status"], "BLOCKED_INCOMPLETE_PLAYER_SCOPING")

    def test_strict_mode_rejects_a_source_that_does_not_match_the_clockwork_reference(self):
        report = audit(self.write_source(
            'handle_drill_spot( player, spot )\n'
            '{\n'
            '    ghc_campaign_drill_begin( player, spot );\n'
            '}\n'
            'handle_drilling( player, spot )\n'
            '{\n'
            '    ghc_campaign_drill_complete( player, spot );\n'
            '    ghc_campaign_drill_cancel( player, spot );\n'
            '}\n'
            'cleanup_drill( player )\n'
            '{\n'
            '    ghc_campaign_drill_abort_player( player );\n'
            '}\n'
        ), reference_source=self.source, require_reference_baseline=True)

        self.assertEqual(report["status"], "SOURCE_BASELINE_MISMATCH")
        self.assertFalse(report["reference_baseline"]["matches"])
        self.assertIn("handle_drill_spot", report["reference_baseline"]["mismatches"])

    def test_strict_mode_requires_an_original_reference_source(self):
        report = audit(self.write_source(
            'handle_drill_spot()\n{\n}\n'
            'handle_drilling()\n{\n}\n'
        ), require_reference_baseline=True)
        self.assertEqual(report["status"], "SOURCE_REFERENCE_MISSING")
        self.assertFalse(report["reference_baseline"]["provided"])

    def test_missing_misc_drill_details_is_reported_as_incomplete_source(self):
        self.source.write_text(
            'handle_drill_spot()\n{\n}\n'
            'handle_drilling()\n{\n}\n',
            encoding="utf-8",
        )
        report = audit(self.source)
        self.assertEqual(report["status"], "SOURCE_INCOMPLETE")
        self.assertIn("handle_misc_drill_details", report["required_core_functions"])
        self.assertEqual(report["target_functions"]["handle_misc_drill_details"], [])

    def test_non_gsc_input_is_rejected(self):
        other = self.root / "source.txt"
        other.write_text("not GSC", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "decompiled .gsc"):
            audit(other)


if __name__ == "__main__":
    unittest.main()
