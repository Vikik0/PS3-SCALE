import tempfile
import unittest
from pathlib import Path

from tools.audit_gsc_splitscreen import GscAuditError, scan_group

class GscSplitScreenAuditTests(unittest.TestCase):
    def test_reports_split_screen_and_player_array_references(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "common.gsc").write_text(
                'init_level_flags()\n{\n'
                '    level.playerpiggyback = getentarray("player", "classname");\n'
                '    if (issplitscreen())\n'
                '        level.playengineeffects = level.playerpiggyback[1];\n'
                '}\n',
                encoding="utf-8",
            )
            result = scan_group("common", root)
            self.assertEqual(result["file_count"], 1)
            self.assertEqual(result["line_match_count"]["issplitscreen"], 1)
            self.assertEqual(result["line_match_count"]["playerpiggyback"], 2)
            self.assertEqual(result["line_match_count"]["player_entity_array"], 1)
            hit = next(item for item in result["hits"] if item["keyword"] == "issplitscreen")
            self.assertEqual(hit["function"], "init_level_flags")

    def test_primary_player_reference_is_distinct_from_player_array(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "mission.gsc").write_text(
                'main()\n{\n'
                '    level.player freezecontrols(1);\n'
                '    level.playerpiggyback[1] freezecontrols(0);\n'
                '}\n',
                encoding="utf-8",
            )
            result = scan_group("mission", root)
            self.assertEqual(result["line_match_count"]["primary_player_ref"], 1)
            self.assertEqual(result["line_match_count"]["playerpiggyback"], 1)

    def test_reports_primary_secondary_and_client_helpers(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "mp.gsc").write_text(
                'onconnect()\n{\n'
                '    if (self issplitscreenplayer() && !self issplitscreenplayerprimary())\n'
                '        return;\n'
                '    self getplayerfromclientnum();\n'
                '}\n',
                encoding="utf-8",
            )
            result = scan_group("mp", root)
            self.assertEqual(result["line_match_count"]["issplitscreenplayer"], 1)
            self.assertEqual(result["line_match_count"]["issplitscreenplayerprimary"], 1)
            self.assertEqual(result["line_match_count"]["getplayerfromclientnum"], 1)

    def test_reports_campaign_and_multiplayer_player_lifecycle_markers(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "campaign.gsc").write_text(
                'init_level_flags()\n{\n'
                '    level.playerpiggyback = getentarray("player", "classname");\n'
                '    level.playengineeffects = level.playerpiggyback[1];\n'
                '}\n',
                encoding="utf-8",
            )
            (root / "mp.gsc").write_text(
                'callback_playerconnect()\n{\n'
                '    level.playerpiggyback[level.playerpiggyback.size] = self;\n'
                '    self.clientid = getlowestavailableclientid();\n'
                '}\n',
                encoding="utf-8",
            )
            result = scan_group("mixed", root)
            counts = result["line_match_count"]
            self.assertEqual(counts["player_array_initializer"], 1)
            self.assertEqual(counts["second_player_alias"], 1)
            self.assertEqual(counts["player_array_append"], 1)
            self.assertEqual(counts["getlowestavailableclientid"], 1)
            self.assertEqual(counts["callback_playerconnect"], 1)

    def test_caps_hit_records_without_losing_counts(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "many.gsc").write_text("a()\n{\nissplitscreen();\nissplitscreen();\n}\n", encoding="utf-8")
            result = scan_group("many", root, max_hits=1)
            self.assertEqual(result["total_matches"], 2)
            self.assertEqual(result["hits_recorded"], 1)
            self.assertTrue(result["hits_truncated"])

    def test_rejects_empty_or_missing_group(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with self.assertRaises(GscAuditError):
                scan_group("empty", root)
            with self.assertRaises(GscAuditError):
                scan_group("missing", root / "missing")

if __name__ == "__main__":
    unittest.main()
