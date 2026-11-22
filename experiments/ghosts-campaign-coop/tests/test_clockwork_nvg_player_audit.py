import unittest

from tools.clockwork_nvg_player_audit import audit


PASSING_SOURCE = r'''
player_lever_input()
{
    if ( level.x_turretcheck || level.ps3 )
    {
        if ( isdefined( level.player ) && isalive( level.player ) )
            thread player_nvg_light_fx( level.player );
        if ( isdefined( level.playengineeffects ) && isplayer( level.playengineeffects ) && isalive( level.playengineeffects ) && level.playengineeffects != level.player )
            thread player_nvg_light_fx( level.playengineeffects );
        if ( !isdefined( level.ghc_nvg_secondary_light_watcher_active ) || !level.ghc_nvg_secondary_light_watcher_active )
        {
            level.ghc_nvg_secondary_light_watcher_active = 1;
            thread watch_secondary_nvg_player();
        }
    }
}

watch_secondary_nvg_player()
{
    while ( !common_scripts\utility::flag( "lights_on" ) )
    {
        if ( isdefined( level.playengineeffects ) )
        {
            var_0 = level.playengineeffects;
            if ( isdefined( var_0 ) && isplayer( var_0 ) && isalive( var_0 ) && var_0 != level.player )
            {
                if ( !isdefined( var_0.ghc_nvg_light_active ) || !var_0.ghc_nvg_light_active )
                    thread player_nvg_light_fx( var_0 );
            }
        }
        wait 0.1;
    }
    level.ghc_nvg_secondary_light_watcher_active = undefined;
}

player_nvg_light_fx( player )
{
    if ( !isdefined( player ) || !isalive( player ) )
        return;
    if ( isdefined( player.ghc_nvg_light_active ) && player.ghc_nvg_light_active )
        return;
    player.ghc_nvg_light_active = 1;
    player.ghc_nvg_light_on = !isdefined( player.nightvision_effectsoff );
    var_0 = common_scripts\utility::spawn_tag_fx();
    playfxontag( level._effect["player_nvg_light"], var_0, "tag_origin" );
    thread keep_player_below_surface( player );
    while ( !common_scripts\utility::flag( "lights_on" ) && isdefined( player ) && isalive( player ) )
    {
        if ( isdefined( player.ghc_nvg_light_on ) && player.ghc_nvg_light_on )
        {
            var_0.angles = player getplayerangles();
            var_0.origin = player.origin + ( 0, 0, 64 );
        }
        else
            var_0.origin = ( 0, 0, 0 );
        wait 0.01;
    }
    if ( isdefined( var_0 ) )
        var_0 delete();
    if ( isdefined( player ) )
    {
        player.ghc_nvg_light_active = undefined;
        player.ghc_nvg_light_on = undefined;
    }
}

keep_player_below_surface( player )
{
    if ( !isdefined( player ) || !isalive( player ) )
        return;
    player notifyonplayercommand( "nvg_toggle", "+actionslot 1" );
    while ( !common_scripts\utility::flag( "lights_on" ) && isdefined( player ) && isalive( player ) )
    {
        player.ghc_nvg_light_on = !isdefined( player.nightvision_effectsoff );
        wait 0.05;
    }
}
'''


class ClockworkNvgPlayerAuditTests(unittest.TestCase):
    def test_player_owned_nvg_source_passes(self):
        report = audit(PASSING_SOURCE)
        self.assertEqual(report["status"], "PASS", report["checks"])
        self.assertEqual(len(report["checks"]), 20)

    def test_secondary_player_must_receive_light_fx(self):
        bad = PASSING_SOURCE.replace(
            "            thread player_nvg_light_fx( level.playengineeffects );", ""
        )
        report = audit(bad)
        self.assertFalse(report["checks"]["secondary_alias_receives_light_fx"])
        self.assertEqual(report["status"], "BLOCKED")

    def test_secondary_player_must_be_validated_and_distinct(self):
        bad = PASSING_SOURCE.replace(
            " && level.playengineeffects != level.player", ""
        )
        report = audit(bad)
        self.assertFalse(report["checks"]["secondary_player_is_validated_and_distinct"])

    def test_late_secondary_watcher_must_be_started(self):
        bad = PASSING_SOURCE.replace("thread watch_secondary_nvg_player();", "")
        report = audit(bad)
        self.assertFalse(report["checks"]["late_secondary_player_watcher_is_started_once"])

    def test_secondary_watcher_must_revalidate_player_alias(self):
        bad = PASSING_SOURCE.replace("isplayer( var_0 )", "isdefined( var_0 )")
        report = audit(bad)
        self.assertFalse(report["checks"]["secondary_watcher_revalidates_current_alias"])

    def test_fx_must_follow_selected_players_pose(self):
        bad = PASSING_SOURCE.replace(
            "var_0.origin = player.origin + ( 0, 0, 64 );",
            "var_0.origin = level.player.origin + ( 0, 0, 64 );",
        )
        report = audit(bad)
        self.assertFalse(report["checks"]["fx_tracks_selected_player_pose"])

    def test_nvgs_state_must_not_use_shared_level_flag(self):
        bad = PASSING_SOURCE.replace(
            "player.ghc_nvg_light_on = !isdefined( player.nightvision_effectsoff );",
            'player.ghc_nvg_light_on = common_scripts\\utility::flag( "nvg_light_on" );',
            1,
        )
        report = audit(bad)
        self.assertFalse(report["checks"]["fx_initial_state_is_player_specific"])

    def test_duplicate_fx_thread_guard_is_required(self):
        bad = PASSING_SOURCE.replace(
            "    if ( isdefined( player.ghc_nvg_light_active ) && player.ghc_nvg_light_active )\n        return;\n", ""
        )
        report = audit(bad)
        self.assertFalse(report["checks"]["fx_helper_prevents_duplicate_threads"])

    def test_fx_entity_must_be_deleted_on_exit(self):
        bad = PASSING_SOURCE.replace("    if ( isdefined( var_0 ) )\n        var_0 delete();\n", "")
        report = audit(bad)
        self.assertFalse(report["checks"]["fx_stops_on_shared_lights_on_or_player_exit"])

    def test_player_state_must_be_cleaned(self):
        bad = PASSING_SOURCE.replace("        player.ghc_nvg_light_active = undefined;\n", "")
        report = audit(bad)
        self.assertFalse(report["checks"]["fx_cleans_player_owned_state"])

    def test_comments_cannot_fake_secondary_route(self):
        bad = PASSING_SOURCE.replace(
            "            thread player_nvg_light_fx( level.playengineeffects );",
            "            // thread player_nvg_light_fx( level.playengineeffects );",
        )
        report = audit(bad)
        self.assertFalse(report["checks"]["secondary_alias_receives_light_fx"])

    def test_missing_player_state_helper_blocks(self):
        bad = PASSING_SOURCE[:PASSING_SOURCE.index("keep_player_below_surface( player )")]
        report = audit(bad)
        self.assertFalse(report["checks"]["state_helper_accepts_explicit_player"])
        self.assertEqual(report["status"], "BLOCKED")


if __name__ == "__main__":
    unittest.main()
