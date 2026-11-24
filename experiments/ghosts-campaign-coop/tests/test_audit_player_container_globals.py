import sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import audit_player_container_globals as audit


class PlayerContainerGlobalAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "maps").mkdir()
        self.source = self.root / "maps" / "globals.gsc"

    def tearDown(self):
        self.temp.cleanup()

    def test_classifies_global_array_slot_entity_and_mutator_writes(self):
        self.source.write_text(
            "init_players()\n{\n"
            " level.playerpiggyback = [];\n"
            " level.playerpiggyback[0] = player_one;\n"
            " level.playerpiggyback[0].sniper_degy = \"cg_playerFovScale0\";\n"
            " level.playerpiggyback.push(player_two);\n"
            " level.playengineeffects = player_two;\n}\n", encoding="utf-8")
        report = audit.audit_mission("carrier", self.root)
        player, secondary = report["playerpiggyback"], report["playengineeffects"]
        self.assertEqual(player["GLOBAL_ASSIGNMENT"], 1)
        self.assertEqual(player["ARRAY_SLOT_ASSIGNMENT"], 1)
        self.assertEqual(player["PLAYER_ENTITY_PROPERTY_WRITE"], 1)
        self.assertEqual(player["ARRAY_MUTATION"], 1)
        self.assertEqual(player["initializer_status"], "SCRIPT_INITIALIZER_FOUND")
        self.assertEqual(secondary["GLOBAL_ASSIGNMENT"], 1)

    def test_reads_do_not_count_as_initialization(self):
        self.source.write_text(
            "scan_players()\n{\n"
            " foreach ( player in level.playerpiggyback ) { }\n"
            " if ( isdefined( level.playengineeffects ) ) { }\n}\n", encoding="utf-8")
        report = audit.audit_mission("carrier", self.root)
        self.assertEqual(report["playerpiggyback"]["ITERATION_READ"], 1)
        self.assertEqual(report["playerpiggyback"]["initializer_status"], "NO_SCRIPT_INITIALIZER_IN_SCANNED_GSC")
        self.assertEqual(report["playengineeffects"]["initializer_status"], "NO_SCRIPT_INITIALIZER_IN_SCANNED_GSC")

    def test_ignores_comments_and_string_literals(self):
        self.source.write_text(
            "scan_players()\n{\n"
            " // level.playerpiggyback = [];\n"
            " message = \"level.playengineeffects = fake\";\n"
            " foreach ( player in level.playerpiggyback ) { }\n}\n", encoding="utf-8")
        report = audit.audit_mission("carrier", self.root)
        self.assertEqual(report["playerpiggyback"]["reference_count"], 1)
        self.assertEqual(report["playerpiggyback"]["initializer_status"], "NO_SCRIPT_INITIALIZER_IN_SCANNED_GSC")
        self.assertEqual(report["playengineeffects"]["reference_count"], 0)

    def test_scans_nested_raw_and_decompiled_gsc_sources(self):
        (self.root / "decompiled_gsc").mkdir()
        (self.root / "raw_gsc_sources").mkdir()
        (self.root / "decompiled_gsc" / "one.gsc").write_text("a()\n{\n level.playerpiggyback = [];\n}\n", encoding="utf-8")
        (self.root / "raw_gsc_sources" / "two.gsc").write_text("b()\n{\n level.playengineeffects = self;\n}\n", encoding="utf-8")
        report = audit.audit_paths({"carrier": self.root})
        mission = report["missions"][0]
        self.assertEqual(mission["playerpiggyback"]["GLOBAL_ASSIGNMENT"], 1)
        self.assertEqual(mission["playengineeffects"]["GLOBAL_ASSIGNMENT"], 1)
        self.assertEqual(mission["source_manifest"]["file_count"], 2)

    def test_reports_reference_only_source_set_without_initializer(self):
        self.source.write_text("a()\n{\n level.playerpiggyback.size;\n}\n", encoding="utf-8")
        report = audit.audit_paths({"carrier": self.root})
        self.assertEqual(report["status"], "PLAYER_CONTAINER_GLOBAL_REFERENCE_AUDIT_COMPLETED")
        self.assertEqual(report["missions"][0]["playerpiggyback"]["initializer_or_array_mutation_count"], 0)


if __name__ == "__main__":
    unittest.main()
