import sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import audit_split_screen_client_semantics as audit


class SplitScreenClientSemanticsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source = self.root / "carrier.gsc"

    def tearDown(self):
        self.temp.cleanup()

    def test_traces_visibility_client_selectors_and_fov_slots(self):
        self.source.write_text(
            "playerview_playmissanim(player)\n{\n"
            " self showonclient(player);\n"
            " player hideonclient(level.player);\n"
            " player hideonclient(level.playengineeffects);\n"
            " level.playerpiggyback[0].sniper_degy = \"cg_playerFovScale0\";\n"
            " level.playerpiggyback[1].sniper_degy = \"cg_playerFovScale1\";\n"
            " setsaveddvar(self.sniper_degy, 1);\n"
            "}\n",
            encoding="utf-8",
        )
        report = audit.scan_mission("carrier", self.root)
        self.assertEqual(report["summary"]["visibility_call_count"], 3)
        self.assertEqual(report["summary"]["hideonclient_count"], 2)
        self.assertEqual(report["summary"]["showonclient_count"], 1)
        self.assertEqual(report["summary"]["secondary_client_selector_count"], 1)
        self.assertEqual(report["summary"]["primary_player_selector_count"], 1)
        self.assertEqual(
            [(item["array_index"], item["dvar"]) for item in report["player_fov_bindings"]],
            [(0, "cg_playerFovScale0"), (1, "cg_playerFovScale1")],
        )
        self.assertEqual(report["summary"]["sniper_dvar_setter_count"], 1)

    def test_traces_favoriteenemy_comparison_with_secondary_client(self):
        self.source.write_text(
            "get_different_favoriteenemy()\n{\n"
            " var_0 = self.favoriteenemy != level.playengineeffects;\n"
            " return level.playerpiggyback[var_0];\n}\n",
            encoding="utf-8",
        )
        report = audit.scan_mission("carrier", self.root)
        self.assertEqual(report["summary"]["favoriteenemy_secondary_compare_count"], 1)

    def test_ignores_comments_and_non_code_strings(self):
        self.source.write_text(
            "a()\n{\n"
            " // self hideonclient(level.playengineeffects);\n"
            " text = \"self hideonclient(level.player)\";\n"
            " self hideonclient(level.player);\n}\n",
            encoding="utf-8",
        )
        report = audit.scan_mission("carrier", self.root)
        self.assertEqual(report["summary"]["visibility_call_count"], 1)

    def test_scans_nested_mission_scripts(self):
        (self.root / "animscripts" / "dog").mkdir(parents=True)
        (self.root / "animscripts" / "dog" / "dog_combat.gsc").write_text(
            "a()\n{\n self hideonclient(level.playengineeffects);\n}\n",
            encoding="utf-8",
        )
        report = audit.audit_paths({"carrier": self.root})
        self.assertEqual(report["missions"][0]["summary"]["secondary_client_selector_count"], 1)
        self.assertEqual(report["missions"][0]["source_manifest"]["file_count"], 1)


if __name__ == "__main__":
    unittest.main()
