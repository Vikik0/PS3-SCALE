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
