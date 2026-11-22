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
    if ( !isalive( drill_player ) )
    {
        common_scripts\\utility::flag_clear( var_1 );
        wait 0.1;
        maps\\_utility::enable_trigger_with_targetname( var_0 );
    }
    drill_player.ghc_clockwork_frag_ammo = 1;
    drill_player.ghc_clockwork_flash_ammo = 1;
    drill_player unlink();
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
    level.ghc_clockwork_drill_available = 0;
    level.playengineeffects takeweapon( "drill_press" );
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
    if ( !isalive( drill_player ) )
    {
        common_scripts\\utility::flag_clear( var_1 );
        wait 0.1;
        maps\\_utility::enable_trigger_with_targetname( var_0 );
    }
    drill_player.ghc_clockwork_frag_ammo = 1;
    drill_player.ghc_clockwork_flash_ammo = 1;
    drill_player unlink();
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
    level.ghc_clockwork_drill_available = 0;
    level.playengineeffects takeweapon( "drill_press" );
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
            '    if ( player adsbuttonpressed( "ads" ) )',
            '    flag_wait( var_1 );\n    if ( player adsbuttonpressed( "ads" ) )',
        )
        self.assertIn("flag_wait( var_1 );", source)
ger waittill( "trigger", drill_activator );
    if ( isdefined( drill_activator ) && isplayer( drill_activator ) && isalive( drill_activator ) )
        drill_player = drill_activator;
    else
        drill_player = level.player;
    if ( drill_player != level.player && ( !issplitscreen() || drill_player != level.playengineeffects || !drill_player issplitscreenplayer() || drill_player issplitscreenplayerprimary() ) )
        drill_player = level.player;
    level.ghc_clockwork_drill_activator = drill_activator;
    common_scripts\\utility::flag_set( var_1, drill_player );
    thread handle_drilling( var_2, var_1, drill_player );
    if ( !isalive( drill_player ) )
    {
        common_scripts\\utility::flag_clear( var_1 );
        wait 0.1;
        maps\\_utility::enable_trigger_with_targetname( var_0 );
    }
    drill_player.ghc_clockwork_frag_ammo = 1;
    drill_player.ghc_clockwork_flash_ammo = 1;
    drill_player unlink();
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
    level.ghc_clockwork_drill_available = 0;
    level.playengineeffects takeweapon( "drill_press" );
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
    if ( !isalive( drill_player ) )
    {
        common_scripts\\utility::flag_clear( var_1 );
        wait 0.1;
        maps\\_utility::enable_trigger_with_targetname( var_0 );
    }
    drill_player.ghc_clockwork_frag_ammo = 1;
    drill_player.ghc_clockwork_flash_ammo = 1;
    drill_player unlink();
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
    level.ghc_clockwork_drill_available = 0;
    level.playengineeffects takeweapon( "drill_press" );
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

