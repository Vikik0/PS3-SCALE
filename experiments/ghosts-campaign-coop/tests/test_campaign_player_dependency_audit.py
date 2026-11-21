import tempfile
import unittest
from pathlib import Path

from tools.campaign_player_dependency_audit import audit_paths, function_ranges


class CampaignPlayerDependencyAuditTests(unittest.TestCase):
    def test_function_ranges_skip_nested_control_blocks(self):
        lines = [
            "main()", "{", "    if ( 1 )", "    {",
            '        level.player giveweapon( "weapon_{x}" );',
            "    }", "}", "next_function()", "{", "    return 1;", "}",
        ]
        self.assertEqual(function_ranges(lines), [("main", 1, 7), ("next_function", 8, 11)])

    def test_counts_player_methods_and_split_markers(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "maps__clockwork_code.gsc").write_text(
                'main()\n{\n level.player giveweapon( "rifle" );\n'
                ' if ( issplitscreen() )\n level.player buttonpressed( "use" );\n}\n',
                encoding="utf-8",
            )
            report = audit_paths([root])
        self.assertEqual(report["file_count"], 1)
        self.assertEqual(report["level_player_refs"], 2)
        self.assertEqual(report["split_screen_refs"], 1)
        self.assertEqual(report["coop_refs"], 0)
        self.assertEqual(report["player_method_line_counts"]["giveweapon"], 1)
        self.assertEqual(report["player_method_line_counts"]["buttonpressed"], 1)

    def test_ignores_comments_and_string_literals(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "maps__clockwork_intro.gsc").write_text(
                'main()\n{\n // level.player fake();\n'
                ' iprintln( "level.player issplitscreen()" );\n'
                ' level.player setorigin( (0,0,0) );\n}\n',
                encoding="utf-8",
            )
            report = audit_paths([root])
        self.assertEqual(report["level_player_refs"], 1)
        self.assertEqual(report["split_screen_refs"], 0)
        self.assertEqual(len(report["line_hits"]), 1)

    def test_clockwork_filter_excludes_non_mission_script_names(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "maps__clockwork.gsc").write_text('main()\n{\n level.player foo();\n}\n')
            (root / "aitype__iw5_clockwork_scientist.gsc").write_text('main()\n{\n level.player baz();\n}\n')
            (root / "maps__other.gsc").write_text('main()\n{\n level.player bar();\n}\n')
            report = audit_paths([root])
        self.assertEqual(report["file_count"], 1)
        self.assertEqual(report["level_player_refs"], 1)


