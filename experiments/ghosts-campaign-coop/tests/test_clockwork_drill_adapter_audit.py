import tempfile
import unittest
from pathlib import Path

from tools.clockwork_drill_adapter_audit import audit, main


def passing_source():
    return r'''
handle_drill_spot( var_0, var_1, var_2, var_3 )
{
    drill_trigger = getent( var_0, "targetname" );
    drill_trigger waittill( "trigger", drill_activator );
    if ( isdefined( drill_activator ) && isplayer( drill_activator ) && isalive( drill_activator ) )
        drill_player = drill_activator;
    else
        drill_player = level.player;
    if ( drill_player != level.player && ( !issplitscreen() || drill_player != level.playengineeffects || !drill_player issplitscreenplayer() || drill_player issplitscreenplayerprimary() ) )
        drill_player = level.player;
    level.ghc_clockwork_drill_activator = drill_activator;
    common_scripts\\utility::flag_set( var_1, drill_player );
    thread handle_drilling( var_2, var_1, drill_player );
    thread clockwork_drill_plant_sound( drill_player );
    if ( !isalive( drill_player ) )
    {
        common_scripts\\utility::flag_clear( var_1 );
        wait 0.1;
        maps\\_utility::enable_trigger_with_targetname( var_0 );
        return drill_player;
    }
    drill_player.ghc_clockwork_frag_ammo = 1;
    drill_player.ghc_clockwork_flash_ammo = 1;
    drill_player unlink();
    return drill_player;
}
clockwork_drill_plant_sound( player )
{
    wait 0.25;
    if ( isdefined( player ) && isalive( player ) )
        player playsound( "clkw_scn_vault_drill_plant" );
}
handle_drilling( var_0, var_1, player )
{
    if ( player adsbuttonpressed( "ads" ) )
        player attackbuttonpressed();
}
handle_misc_drill_details( player )
{
    thread watch_for_air_drop_death( player );
    player.ghc_aiming_at_ally = 0;
    player playsound( "clkw_scn_vault_drill_pullout" );
    player playsound( "clkw_scn_vault_drill_pullout" );
}
watch_for_air_drop_death( player )
{
    player.ghc_aiming_at_ally = 1;
}
restore_footstep_notetrack_scripts( player )
{
    player.ghc_clockwork_frag_ammo;
    player.ghc_clockwork_flash_ammo;
}
drillthrough_plate_sound( var_0, var_1, player )
{
    player attackbuttonpressed();
}
stop_current_car_watcher( player, var_0, var_1 )
{
    player playsound( "drill" );
}
clockwork_setup_player_two( player )
{
    if ( !player issplitscreenplayer() || player issplitscreenplayerprimary() )
        return;
    player takeallweapons();
    player giveweapon( "gm6+scopegm6_sp+silencer03_sp" );
    player giveweapon( "cz805bren+reflex_sp+silencer_sp" );
    player giveweapon( "fraggrenade" );
    player giveweapon( "flash_grenade" );
}
handle_drill_ammo_counter()
{
    var_0 = getdvar( "ammoCounterHide" );
    level.player getcurrentweapon();
    level.playengineeffects getcurrentweapon();
    setsaveddvar( "ammoCounterHide", "1" );
    setsaveddvar( "ammoCounterHide", var_0 );
}
clockwork_watch_player_pair()
{
    level.playengineeffects;
    clockwork_setup_player_two( level.playengineeffects );
    thread handle_misc_drill_details( level.playengineeffects );
}
switchmsg()
{
    level.player giveweapon( "drill_press" );
    level.ghc_clockwork_drill_available = 1;
    level.playengineeffects giveweapon( "drill_press" );
}
new_trench_setup()
{
    drill_player_1 = handle_drill_spot( "drill_spot1", "drill1_start", "drill1_complete", "TAG_FX_XMark_RI" );
    drill_player_2 = handle_drill_spot( "drill_spot2", "drill2_start", "drill2_complete", "TAG_FX_XMark_LE" );
    drill_player_1 playerlinktodelta( level.player_approach_weld, "tag_player", 1, 45, 45, 10, 10 );
    drill_player_2 playerlinktodelta( level.player_approach_weld, "tag_player", 0.5, 90, 20, 10, 10 );
    if ( common_scripts\\utility::flag( "vault_blast_area" ) )
    {
        var_3 = getent( "smoke_spot", "targetname" );
        thread clockwork_apply_vault_blast( level.player, var_3 );
        if ( isdefined( level.playengineeffects ) && isplayer( level.playengineeffects ) && isalive( level.playengineeffects ) )
            thread clockwork_apply_vault_blast( level.playengineeffects, var_3 );
    }
    level.player playrumbleonentity( "artillery_rumble" );
    if ( isdefined( level.playengineeffects ) && isplayer( level.playengineeffects ) && isalive( level.playengineeffects ) )
        level.playengineeffects playrumbleonentity( "artillery_rumble" );
    level.ghc_clockwork_drill_available = 0;
    level.playengineeffects takeweapon( "drill_press" );
}
clockwork_apply_vault_blast( player, blast_origin )
{
    if ( !isdefined( player ) || !isplayer( player ) || !isalive( player ) || !isdefined( blast_origin ) )
        return;
    var_0 = player.health - 5;
    if ( var_0 > 0 )
        player dodamage( var_0, blast_origin.origin );
    player shellshock( "default", 5 );
    player setstance( "prone" );
    player pushplayervector( ( 100, 0, 0 ), 1 );
    wait 0.2;
    if ( isdefined( player ) && isplayer( player ) && isalive( player ) )
        player pushplayervector( ( 0, 0, 0 ), 1 );
}
'''


class ClockworkDrillAdapterAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source = self.root / "maps__clockwork_interior.gsc"

    def tearDown(self):
        self.temp.cleanup()

    def write_source(self, content):
        self.source.write_text(content, encoding="utf-8")
        return self.source

    def test_complete_player_routing_source_passes_static_gate(self):
        report = audit(self.write_source(passing_source()))
        self.assertTrue(all(report["checks"].values()), report["checks"])
        self.assertEqual(report["status"], "SOURCE_ADAPTER_READY_FOR_MANUAL_REVIEW")
        self.assertEqual(report["unscoped_level_player_functions"], [])
        self.assertIn("clockwork_watch_player_pair", report["coordination_functions"])

    def test_original_single_player_method_reference_blocks_player_scoping(self):
        source = passing_source().replace(
            '    level.ghc_clockwork_drill_activator = drill_activator;\n',
            '    level.player getcurrentweapon();\n'
            '    level.ghc_clockwork_drill_activator = drill_activator;\n',
        )
        report = audit(self.write_source(source))
        self.assertEqual(report["status"], "BLOCKED_INCOMPLETE_PLAYER_SCOPING")
        self.assertIn("handle_drill_spot", report["unscoped_level_player_functions"])

    def test_comments_and_string_literals_do_not_fake_trigger_activator_capture(self):
        source = passing_source().replace(
            '    drill_trigger waittill( "trigger", drill_activator );\n',
            '    // drill_trigger waittill( "trigger", drill_activator );\n'
            '    iprintln( "drill_trigger waittill( trigger, drill_activator )" );\n',
        )
        report = audit(self.write_source(source))
        self.assertEqual(report["status"], "BLOCKED_INCOMPLETE_PLAYER_SCOPING")
        self.assertFalse(report["checks"]["trigger_activator_capture_present"])

    def test_missing_core_function_is_reported_as_incomplete_source(self):
        source = passing_source()
        start = source.index("handle_drilling(")
        end = source.index("\nhandle_misc_drill_details(", start)
        source = source[:start] + source[end + 1:]
        report = audit(self.write_source(source))
        self.assertEqual(report["status"], "SOURCE_INCOMPLETE")
        self.assertFalse(report["checks"]["all_required_functions_found"])
        self.assertEqual(report["target_functions"]["handle_drilling"], [])

    def test_missing_death_disconnect_recovery_blocks_adapter(self):
        source = passing_source().replace(
            '        common_scripts\\\\utility::flag_clear( var_1 );\n'
            '        wait 0.1;\n'
            '        maps\\\\_utility::enable_trigger_with_targetname( var_0 );\n',
            '',
        )
        report = audit(self.write_source(source))
        self.assertEqual(report["status"], "BLOCKED_INCOMPLETE_PLAYER_SCOPING")
        self.assertFalse(report["checks"]["death_disconnect_reopens_drill_trigger"])

    def test_drill_worker_does_not_wait_forever_on_a_cleared_start_flag(self):
        source = passing_source().replace(
            '    if ( player adsbuttonpressed( "ads" ) )',
            '    flag_wait( var_1 );\n    if ( player adsbuttonpressed( "ads" ) )',
        )
        self.assertIn("flag_wait( var_1 );", source)
        report = audit(self.write_source(source))
        self.assertEqual(report["status"], "BLOCKED_INCOMPLETE_PLAYER_SCOPING")
        self.assertFalse(report["checks"]["drill_worker_does_not_block_on_start_flag"])

    def test_primary_alias_fallback_and_identity_checks_are_allowed(self):
        report = audit(self.write_source(passing_source()))
        row = report["target_functions"]["handle_drill_spot"][0]
        self.assertEqual(row["allowed_primary_alias_refs"], row["level_player_refs"])
        self.assertEqual(row["unsafe_level_player_refs"], 0)

    def test_logan_only_switchmsg_is_reported_but_not_treated_as_p2_scope_leak(self):
        report = audit(self.write_source(passing_source()))
        self.assertIn("switchmsg", report["primary_only_function_details"])
        self.assertGreater(
            report["primary_only_function_details"]["switchmsg"][0]["level_player_refs"],
            0,
        )
        self.assertNotIn("switchmsg", report["unscoped_level_player_functions"])

    def test_related_support_functions_are_player_scoped(self):
        source = passing_source().replace(
            '    player attackbuttonpressed();\n}\nstop_current_car_watcher',
            '    level.player attackbuttonpressed();\n}\nstop_current_car_watcher',
        )
        report = audit(self.write_source(source))
        self.assertIn("drillthrough_plate_sound", report["related_function_details"])
        self.assertIn("drillthrough_plate_sound", report["unscoped_level_player_functions"])

    def test_json_output_cannot_overwrite_input_gsc(self):
        source = self.write_source(passing_source())
        original = source.read_text(encoding="utf-8")
        result = main([str(source), "--json-out", str(source)])
        self.assertEqual(result, 1)
        self.assertEqual(source.read_text(encoding="utf-8"), original)

    def test_strict_mode_rejects_a_reference_source_with_the_wrong_hash(self):
        source = self.write_source(passing_source())
        report = audit(
            source,
            reference_source=source,
            require_reference_baseline=True,
        )
        self.assertEqual(report["status"], "SOURCE_BASELINE_MISMATCH")
        self.assertFalse(report["reference_baseline"]["source_hash_matches"])

    def test_strict_mode_requires_an_original_reference_source(self):
        report = audit(
            self.write_source(passing_source()),
            require_reference_baseline=True,
        )
        self.assertEqual(report["status"], "SOURCE_REFERENCE_MISSING")
        self.assertFalse(report["reference_baseline"]["provided"])

    def test_missing_misc_drill_details_is_reported_as_incomplete_source(self):
        source = passing_source()
        start = source.index("handle_misc_drill_details(")
        end = source.index("\nwatch_for_air_drop_death(", start)
        source = source[:start] + source[end + 1:]
        report = audit(self.write_source(source))
        self.assertEqual(report["status"], "SOURCE_INCOMPLETE")
        self.assertIn("handle_misc_drill_details", report["required_core_functions"])

    def test_non_gsc_input_is_rejected(self):
        other = self.root / "source.txt"
        other.write_text("not GSC", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "decompiled .gsc"):
            audit(other)


if __name__ == "__main__":
    unittest.main()
