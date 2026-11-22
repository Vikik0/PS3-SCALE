import unittest

from tools.clockwork_pip_owner_audit import audit


INTERIOR = r'''
handle_drill_spot( var_0, var_1, var_2, var_3 )
{
    drill_player = drill_activator;
    maps\clockwork_pip::pip_display_timer( var_15, undefined, undefined, undefined, undefined, 50, 50, 400, 225, "clockwork_pillar_room", drill_player );
}
'''

PIP = r'''
pip_enable( player )
{
    if ( !isdefined( player ) || !isplayer( player ) )
        player = level.player;
    if ( isdefined( level.pintoscreenedge ) )
    {
        if ( isdefined( level.pintoscreenedge_player ) && level.pintoscreenedge_player == player )
        {
            level.pintoscreenedge.enable = 0;
            return;
        }
        level.pintoscreenedge notify( "stop_interference" );
        level.pintoscreenedge.enable = 0;
    }
    level.pintoscreenedge = player newpip();
    level.pintoscreenedge.enable = 0;
    level.pintoscreenedge_player = player;
}

pip_display_timer( var_0, var_1, var_2, var_3, var_4, var_5, var_6, var_7, var_8, var_9, player )
{
    if ( !isdefined( player ) || !isplayer( player ) )
        player = level.player;
    pip_enable( player );
    pip_remove_l_corners( var_0, var_1, var_2, var_3, var_4, player );
}

pip_remove_l_corners( var_0, var_1, var_2, var_3, var_4, player )
{
    if ( !isdefined( player ) || !isplayer( player ) )
        player = level.player;
    if ( !isdefined( var_0 ) )
        level.pintoscreenedge.origin = player.origin + ( 0, 0, 60 );
}
'''


class ClockworkPipOwnerAuditTests(unittest.TestCase):
    def test_player_owned_pip_source_passes(self):
        report = audit(INTERIOR, PIP)
        self.assertEqual(report["status"], "PASS", report["checks"])
        self.assertEqual(len(report["checks"]), 13)

    def test_drill_call_must_pass_the_actual_owner(self):
        report = audit(INTERIOR.replace(", drill_player );", " );"), PIP)
        self.assertFalse(report["checks"]["drill_call_passes_selected_owner"])
        self.assertEqual(report["status"], "BLOCKED")

    def test_pip_enable_must_not_create_on_primary_unconditionally(self):
        bad = PIP.replace("player newpip()", "level.player newpip()")
        report = audit(INTERIOR, bad)
        self.assertFalse(report["checks"]["pip_enable_creates_pip_for_selected_player"])

    def test_corner_origin_must_use_selected_player(self):
        bad = PIP.replace("player.origin + ( 0, 0, 60 )", "level.player.origin + ( 0, 0, 60 )")
        report = audit(INTERIOR, bad)
        self.assertFalse(report["checks"]["corner_origin_uses_selected_player"])
        self.assertFalse(report["checks"]["corner_origin_not_hardcoded_to_primary"])

    def test_previous_pip_must_be_stopped_before_rebinding(self):
        bad = PIP.replace('level.pintoscreenedge notify( "stop_interference" );', "")
        report = audit(INTERIOR, bad)
        self.assertFalse(report["checks"]["pip_enable_stops_previous_owner_pip"])


if __name__ == "__main__":
    unittest.main()
