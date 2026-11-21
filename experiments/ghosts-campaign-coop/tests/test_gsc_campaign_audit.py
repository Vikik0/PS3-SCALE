import tempfile
import unittest
from pathlib import Path

from tools.gsc_campaign_audit import audit_paths, scan_file


class GscCampaignAuditTests(unittest.TestCase):
    def test_finds_split_screen_and_player_lifecycle_terms(self):
        source = (
            'if ( isSplitScreen() ) {\n'
            '    level.players[clientid] = self;\n'
            '    self set_split_screen();\n'
        )
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "playerlogic.gsc"
            path.write_text(source, encoding="utf-8")
            findings = scan_file(path)
        self.assertEqual([item.line for item in findings], [1, 2, 3])
        self.assertEqual(findings[0].category, "split_screen")
        self.assertEqual(findings[1].category, "player_lifecycle")
        self.assertEqual(findings[2].category, "split_screen")

    def test_finds_revive_and_campaign_flow(self):
        source = (
            'self thread laststand();\n'
            'self waittill("revive_complete");\n'
            'level.checkpoint = "mission_02";\n'
        )
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "campaign.csc"
            path.write_text(source, encoding="utf-8")
            findings = scan_file(path)
        categories = [item.category for item in findings]
        self.assertIn("down_revive", categories)
        self.assertIn("campaign_flow", categories)

    def test_finds_native_connect_identity_and_player_ui_hooks(self):
        source = (
            'level waittill( "connected", player );\n'
            'self.clientid = self.pers["clientid"];\n'
            'self SetClientOmnvar( "ui_laststand_end_milliseconds", 0 );\n'
            'if ( isPlayingSolo() ) return wait_for_self_revive();\n'
        )
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "extinction.gsc"
            path.write_text(source, encoding="utf-8")
            findings = scan_file(path)
        categories = {(item.line, item.category) for item in findings}
        self.assertIn((1, "player_lifecycle"), categories)
        self.assertIn((2, "player_lifecycle"), categories)
        self.assertIn((3, "client_presentation"), categories)
        self.assertIn((4, "solo_coop_branch"), categories)
        self.assertIn((4, "down_revive"), categories)

    def test_audit_directory_ignores_binary_and_unrelated_files(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "a.gsc").write_text("isSplitscreenPlayer();\n", encoding="utf-8")
            (root / "b.bin").write_bytes(b"isSplitScreen()\x00")
            (root / "c.txt").write_text("checkpoint();\n", encoding="utf-8")
            report = audit_paths([root])
        self.assertEqual(report["source_file_count"], 2)
        self.assertEqual(report["finding_count"], 2)
        self.assertEqual(report["category_counts"]["split_screen"], 1)
        self.assertEqual(report["category_counts"]["campaign_flow"], 1)


if __name__ == "__main__":
    unittest.main()
