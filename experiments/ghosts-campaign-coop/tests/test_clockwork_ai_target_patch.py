import hashlib
import unittest
from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory

from tools.clockwork_ai_target_patch import main, patch_source, function_span


SYNTHETIC_SOURCE = r'''
setup_clockwork()
{
    level.ghc_fixture_marker = 1;
}

discovery_guys_wakeup()
{
    common_scripts\utility::flag_wait( "discovery_guys" );
    foreach ( var_1 in level.discovery_guys )
    {
        var_1 maps\_utility::anim_stopanimscripted();
        var_1 maps\_utility::set_ignore_until_fired_at( 0 );
        var_1 getenemyinfo( level.player );
    }
}

fixture_interior_setup()
{
    level.ghc_stage6_marker = 2;
}
'''


class ClockworkAiTargetPatchTests(unittest.TestCase):
    def setUp(self):
        _, _, _, body = function_span(SYNTHETIC_SOURCE, "discovery_guys_wakeup")
        self.body_hash = hashlib.sha256(body.encode("utf-8")).hexdigest()

    def test_patch_selects_player_and_preserves_other_functions(self):
        patched = patch_source(SYNTHETIC_SOURCE, self.body_hash)
        self.assertIn("level.ghc_stage6_marker = 1;", patched)
        self.assertIn("level.ghc_stage6_marker = 2;", patched)
        self.assertIn("clockwork_pick_enemy_target( var_1 )", patched)
        self.assertIn("issplitscreen() && isdefined( level.playengineeffects )", patched)
        self.assertIn("secondary_player issplitscreenplayer()", patched)
        self.assertIn("!secondary_player issplitscreenplayerprimary()", patched)
        self.assertIn("distance( ai.origin, secondary_player.origin )", patched)
        self.assertEqual(patched.count("getenemyinfo( var_2 )"), 1)
        self.assertNotIn("var_1 getenemyinfo( level.player )", patched)

    def test_patch_rejects_changed_discovery_sequence(self):
        changed = SYNTHETIC_SOURCE.replace("set_ignore_until_fired_at( 0 )", "set_ignore_until_fired_at( 1 )")
        with self.assertRaises(ValueError):
            patch_source(changed, self.body_hash)

    def test_patch_rejects_duplicate_helper(self):
        with_helper = "clockwork_pick_enemy_target( ai )\n{ return level.player; }\n" + SYNTHETIC_SOURCE
        with self.assertRaises(ValueError):
            patch_source(with_helper, self.body_hash)

    def test_patch_rejects_missing_primary_only_call(self):
        changed = SYNTHETIC_SOURCE.replace("var_1 getenemyinfo( level.player );", "")
        _, _, _, body = function_span(changed, "discovery_guys_wakeup")
        changed_hash = hashlib.sha256(body.encode("utf-8")).hexdigest()
        with self.assertRaises(ValueError):
            patch_source(changed, changed_hash)

    def test_cli_rejects_unpinned_source_without_writing_output(self):
        with TemporaryDirectory() as temp:
            source = Path(temp) / "unknown.gsc"
            output = Path(temp) / "patched.gsc"
            source.write_text("not the pinned Clockwork source", encoding="utf-8")
            with redirect_stderr(StringIO()):
                result = main([str(source), str(output)])
            self.assertEqual(result, 1)
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
