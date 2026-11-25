import json
import tempfile
import unittest
import zipfile
from pathlib import Path

from tools.mission_event_activator_audit import (
    _call_arguments,
    _mask_comments_and_strings,
    audit_archive,
    validate_report,
    validate_dependency_graph,
)


def fixture_sources():
    return {
        "missions/black_ice/decompiled_gsc/maps/black_ice_flarestack.gsc": r"""
magicreloadwhenreachenemy()
{
    thread event_player_turns_key();
}
event_player_turns_key()
{
    maps\\black_ice_util::waittill_trigger( level.console );
    level.player disableweapons();
}
""",
        "missions/black_ice/decompiled_gsc/maps/black_ice_util.gsc": r"""
waittill_trigger( trigger_ent )
{
    level.player usebuttonpressed();
}
""",
        "missions/carrier/decompiled_gsc/maps/carrier_code_zodiac.gsc": r"""
run_corpse_cleanup( rope )
{
    wait_for_capture( rope );
    thread cut_the_rope( rope );
}
wait_for_capture( rope )
{
    level.player waittill( "cut" );
}
cut_the_rope( rope )
{
    level.player unlink();
}
""",
        "missions/cornered/decompiled_gsc/maps/cornered_building_entry.gsc": r"""
gun_down_trigger()
{
    common_scripts\\utility::flag_wait( "start" );
    gun_down_trigger_internal();
}
gun_down_trigger_internal()
{
    level.player takeallweapons();
}
upload_progress()
{
    upload_virus_enter();
}
upload_virus_exit()
{
    upload_virus_enter();
}
upload_virus_enter()
{
    level.player unlink();
}
""",
        "missions/cornered/decompiled_gsc/maps/player_scripted_anim_util.gsc": r"""
waittill_trigger( trigger_ent )
{
    level.player player_looking_at_on_dog();
}
""",
        "missions/black_ice/decompiled_gsc/maps/black_ice_exfil.gsc": r"""
player_fail_finale_instakill()
{
    level.player kill();
    thread maps\\_utility::missionfailed();
}
""",
        "missions/black_ice/decompiled_gsc/maps/black_ice_ascend.gsc": r"""
ascend_logic()
{
    level.player attackbuttonpressed();
    level notify( "ascend" );
}
""",
        "missions/carrier/decompiled_gsc/maps/carrier_sparrow_run.gsc": r"""
knockback_hesh()
{
    level.player freezecontrols( 1 );
    maps\\_utility::slowmo_end();
}
""",
        "missions/carrier/decompiled_gsc/maps/carrier_deck_victory.gsc": r"""
victory_choppers_land()
{
    level.player unlink();
    thread maps\\_utility::autosave_now();
}
""",
        "missions/cornered/decompiled_gsc/maps/cornered_destruct.gsc": r"""
fall_player()
{
    level.player viewkick( 1 );
    setphysicsgravitydir( ( 0, -1, 0 ) );
}
""",
    }


def write_archive(path: Path, sources=None):
    sources = sources or fixture_sources()
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, content in sources.items():
            archive.writestr("Ghosts_Campaign_Missions_Phase31/" + name, content.encode("utf-8"))


class MissionEventActivatorAuditTests(unittest.TestCase):
    def test_fixture_audit_blocks_all_four_player_locks_and_preserves_five_coordinators(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "missions.zip"
            write_archive(archive)
            report = audit_archive(archive, enforce_reference_hashes=False)

        self.assertEqual(report["summary"]["shared_interactions"], 4)
        self.assertEqual(report["summary"]["safe_to_wire"], 0)
        self.assertEqual(report["summary"]["blocked_missing_activator_lineage"], 4)
        self.assertEqual(report["summary"]["mission_coordinators_single_shot"], 5)
        self.assertEqual(validate_report(report, require_reference_hashes=False), [])

    def test_call_sites_and_rope_entity_argument_are_reported(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "missions.zip"
            write_archive(archive)
            report = audit_archive(archive, enforce_reference_hashes=False)

        rope = next(item for item in report["shared_interactions"] if item["routine"] == "cut_the_rope")
        self.assertEqual(rope["call_sites"][0]["caller"], "run_corpse_cleanup")
        self.assertEqual(rope["call_sites"][0]["arguments"], ["rope"])
        upload = next(item for item in report["shared_interactions"] if item["routine"] == "upload_virus_enter")
        self.assertEqual(
            {item["caller"] for item in upload["call_sites"]},
            {"upload_progress", "upload_virus_exit"},
        )

    def test_source_hash_pinning_rejects_unrecognized_archives(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "missions.zip"
            write_archive(archive)
            with self.assertRaisesRegex(ValueError, "reference source hash mismatch"):
                audit_archive(archive, enforce_reference_hashes=True)

    def test_missing_required_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "missions.zip"
            sources = fixture_sources()
            del sources["missions/carrier/decompiled_gsc/maps/carrier_code_zodiac.gsc"]
            write_archive(archive, sources)
            with self.assertRaisesRegex(ValueError, "expected exactly one archive member"):
                audit_archive(archive, enforce_reference_hashes=False)

    def test_comments_and_string_literals_do_not_become_code_evidence(self):
        source = """
target()
{
    // level.player waittill( "trigger", fake_activator );
    var_0 = "level.player waittill( trigger, fake_activator )";
    level notify( "still_not_a_player_reference" );
}
"""
        masked = _mask_comments_and_strings(source)
        self.assertNotIn("level.player", masked)
        self.assertNotIn("fake_activator", masked)

    def test_call_argument_parser_handles_nested_expressions(self):
        source = "handler( player, vector_add( origin, ( 1, 2, 3 ) ), 7 )"
        opening = source.index("(")
        self.assertEqual(
            _call_arguments(source, opening),
            ["player", "vector_add( origin, ( 1, 2, 3 ) )", "7"],
        )

    def test_report_rejects_claim_that_original_handlers_are_already_wired(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "missions.zip"
            write_archive(archive)
            report = audit_archive(archive, enforce_reference_hashes=False)
        report["shared_interactions"][0]["safe_to_wire_event_lock"] = True
        errors = validate_report(report, require_reference_hashes=False)
        self.assertTrue(any("must not claim an unsafe interaction is ready to wire" in error for error in errors))

    def test_phase90_dependency_graph_passes_source_and_edge_validation(self):
        report_path = Path(__file__).resolve().parents[1] / "reports" / "PHASE90_MISSION_EVENT_REFACTOR_GRAPH.json"
        graph = json.loads(report_path.read_text(encoding="utf-8"))
        self.assertEqual(validate_dependency_graph(graph), [])

    def test_dependency_graph_rejects_edge_outside_its_chain(self):
        report_path = Path(__file__).resolve().parents[1] / "reports" / "PHASE90_MISSION_EVENT_REFACTOR_GRAPH.json"
        graph = json.loads(report_path.read_text(encoding="utf-8"))
        graph["chains"][0]["edges"][0]["to"] = "missing::node"
        errors = validate_dependency_graph(graph)
        self.assertTrue(any("dependency edge escapes its chain" in error for error in errors))

    def test_dependency_graph_keeps_carrier_self_and_var0_separate(self):
        report_path = Path(__file__).resolve().parents[1] / "reports" / "PHASE90_MISSION_EVENT_REFACTOR_GRAPH.json"
        graph = json.loads(report_path.read_text(encoding="utf-8"))
        graph["carrier_entity_contract"]["cut_the_rope_var_0"] = "rope entity"
        errors = validate_dependency_graph(graph)
        self.assertTrue(any("var_0 must remain the map struct" in error for error in errors))

    def test_dependency_graph_keeps_mission_coordinators_single_shot(self):
        report_path = Path(__file__).resolve().parents[1] / "reports" / "PHASE90_MISSION_EVENT_REFACTOR_GRAPH.json"
        graph = json.loads(report_path.read_text(encoding="utf-8"))
        graph["mission_coordinator_rules"][0]["event_lock_id"] = 0
        errors = validate_dependency_graph(graph)
        self.assertTrue(any("mission coordinator rules must remain single-shot" in error for error in errors))

    def test_report_rejects_player_lock_on_mission_coordinator(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / "missions.zip"
            write_archive(archive)
            report = audit_archive(archive, enforce_reference_hashes=False)
        report["mission_coordinators"][0]["event_lock_id"] = 9
        errors = validate_report(report, require_reference_hashes=False)
        self.assertTrue(any("must remain single-shot" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
