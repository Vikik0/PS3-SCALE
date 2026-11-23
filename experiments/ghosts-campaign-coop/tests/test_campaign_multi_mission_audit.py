import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from campaign_multi_mission_audit import audit_mission, audit_paths, function_ranges, strip_comments_and_strings


class CampaignMultiMissionAuditTests(unittest.TestCase):
    def test_comments_and_strings_do_not_create_player_hits(self):
        lines = [
            "main()",
            "{",
            '    // level.player giveweapon("fake");',
            '    self iprintln("level.player is only text");',
            '    level.player giveweapon("real_weapon");',
            "}",
        ]
        clean = "\n".join(strip_comments_and_strings(line) for line in lines)
        self.assertEqual(clean.count("level.player"), 1)

    def test_function_ranges_capture_nested_blocks(self):
        lines = [
            "main()", "{", "    if ( 1 )", "    {",
            '        level.player giveweapon( "weapon" );',
            "    }", "}", "other()", "{", "    return;", "}",
        ]
        self.assertEqual(function_ranges(lines), [("main", 1, 7), ("other", 8, 11)])

    def test_audit_separates_map_scripts_from_support_scripts(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "black_ice"
            (root / "maps").mkdir(parents=True)
            (root / "animscripts").mkdir()
            (root / "maps" / "mission.gsc").write_text(
                'main()\n{\n level.player giveweapon("weapon");\n}\n', encoding="utf-8"
            )
            (root / "animscripts" / "support.gsc").write_text(
                'helper()\n{\n level.player waittill("death");\n}\n', encoding="utf-8"
            )
            report = audit_mission("black_ice", root)
            self.assertEqual(report["all_scripts"]["file_count"], 2)
            self.assertEqual(report["all_scripts"]["totals"]["level_player"], 2)
            self.assertEqual(report["map_scripts"]["file_count"], 1)
            self.assertEqual(report["map_scripts"]["totals"]["level_player"], 1)

    def test_multi_mission_summary_totals(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for mission in ("black_ice", "carrier"):
                maps = root / mission / "maps"
                maps.mkdir(parents=True)
                (maps / f"{mission}.gsc").write_text(
                    'main()\n{\n level.player waittill("death");\n}\n', encoding="utf-8"
                )
            report = audit_paths({
                "black_ice": root / "black_ice",
                "carrier": root / "carrier",
            })
            self.assertEqual(report["mission_count"], 2)
            self.assertEqual(report["all_scripts"]["file_count"], 2)
            self.assertEqual(report["map_scripts"]["level_player_refs"], 2)


if __name__ == "__main__":
    unittest.main()
