import unittest
from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory

from tools.clockwork_nvg_player_patch import main, patch_source


ORIGINAL_FIXTURE = r'''
setup()
{
    thread player_lever_input();
}

player_lever_input()
{
    if ( level.x_turretcheck || level.ps3 )
    {
        var_0 = common_scripts\utility::spawn_tag_fx();
        playfxontag( level._effect["player_nvg_light"], var_0, "tag_origin" );
        thread keep_player_below_surface();
    }
}

nvg_animted_scene()
{
    common_scripts\utility::exploder( 250 );
}

keep_player_below_surface()
{
    level.player notifyonplayercommand( "nvg_toggle", "+actionslot 1" );
    common_scripts\utility::flag_set( "nvg_light_on" );
}
'''


class ClockworkNvgPlayerPatchTests(unittest.TestCase):
    def test_patch_keeps_existing_no_argument_call_sites(self):
        patched = patch_source(ORIGINAL_FIXTURE)
        self.assertIn("thread player_lever_input();", patched)
        self.assertIn("player_lever_input()", patched)

    def test_patch_adds_player_two_route(self):
        patched = patch_source(ORIGINAL_FIXTURE)
        self.assertIn("player_nvg_light_fx( level.player )", patched)
        self.assertIn("player_nvg_light_fx( level.playengineeffects )", patched)
        self.assertIn("watch_secondary_nvg_player()", patched)
        self.assertIn("isplayer( level.playengineeffects )", patched)

    def test_patch_uses_per_player_nvgs_state(self):
        patched = patch_source(ORIGINAL_FIXTURE)
        self.assertIn("player.nightvision_effectsoff", patched)
        self.assertIn("player.ghc_nvg_light_on", patched)
        self.assertNotIn('flag( "nvg_light_on" )', patched)

    def test_patch_inserts_only_one_player_fx_helper(self):
        patched = patch_source(ORIGINAL_FIXTURE)
        self.assertEqual(patched.count("player_nvg_light_fx( player )"), 1)

    def test_patch_rejects_missing_target_function(self):
        bad = ORIGINAL_FIXTURE.replace("keep_player_below_surface()", "old_keep_player_below_surface()")
        with self.assertRaises(ValueError):
            patch_source(bad)

    def test_cli_rejects_unpinned_source_without_writing_output(self):
        with TemporaryDirectory() as temp:
            source = Path(temp) / "unknown.gsc"
            output = Path(temp) / "patched.gsc"
            source.write_text("not the pinned retail source", encoding="utf-8")
            with redirect_stderr(StringIO()):
                result = main([str(source), str(output)])
            self.assertEqual(result, 1)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
