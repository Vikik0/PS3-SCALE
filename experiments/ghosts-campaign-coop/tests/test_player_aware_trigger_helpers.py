import hashlib
import tempfile
import unittest
from pathlib import Path

from tools.player_aware_trigger_helpers import (
    EXPECTED_BLACK_ICE_UTIL_SHA256,
    EXPECTED_PLAYER_SCRIPTED_ANIM_UTIL_SHA256,
    PatchError,
    build_bundle,
    extract_function,
    patch_black_ice_source,
    patch_player_scripted_anim_source,
    verify_pinned_source,
    verify_shared_source_pair,
    write_bundle,
)


BLACK_ICE_FIXTURE = """waittill_trigger( var_0, var_1, var_2, var_3, var_4, var_5, var_6 )
{
    if ( level.player ismeleeing() )
        level.player enableweaponpickup();
    wait 0.05;
}
"""

PLAYER_SCRIPTED_ANIM_FIXTURE = """waittill_trigger( var_0, var_1, var_2, var_3, var_4, var_5 )
{
    var_7 = isdefined( var_4 ) && var_4;
    var_0 thread _trigger_handle_triggering( var_7 );
    if ( level.player getstance() == "prone" )
        var_0 common_scripts\\utility::trigger_nobloodpool();
    wait 0.1;
}

_trigger_handle_triggering( var_0 )
{
    self waittill( "trigger" );
    if ( !level.player ismeleeing() )
        self notify( "valid_trigger" );
}
"""


class PlayerAwareTriggerHelperTests(unittest.TestCase):
    def test_black_ice_legacy_wrapper_and_explicit_player_helper(self):
        patched = patch_black_ice_source(BLACK_ICE_FIXTURE)
        wrapper = extract_function(patched, "waittill_trigger")
        helper = extract_function(patched, "waittill_trigger_for_player")
        self.assertIn("waittill_trigger_for_player( level.player,", wrapper)
        self.assertIn("player ismeleeing()", helper)
        self.assertIn("player enableweaponpickup()", helper)
        self.assertNotIn("level.player", helper)

    def test_shared_helper_propagates_player_to_trigger_callback(self):
        patched = patch_player_scripted_anim_source(PLAYER_SCRIPTED_ANIM_FIXTURE)
        wrapper = extract_function(patched, "waittill_trigger")
        helper = extract_function(patched, "waittill_trigger_for_player")
        callback = extract_function(patched, "_trigger_handle_triggering")
        self.assertIn("waittill_trigger_for_player( level.player,", wrapper)
        self.assertIn("_trigger_handle_triggering( player, var_7 )", helper)
        self.assertIn("player getstance()", helper)
        self.assertIn('self waittill( "trigger", var_7 );', callback)
        self.assertIn("var_7 != player", callback)
        self.assertIn("continue;", callback)
        self.assertIn("player ismeleeing()", callback)
        self.assertNotIn("level.player", helper)
        self.assertNotIn("level.player", callback)

    def test_parser_ignores_indented_calls_and_finds_top_level_definition(self):
        source = """caller()
{
    waittill_trigger( 1, 2, 3, 4, 5, 6, 7 );
}

waittill_trigger( var_0, var_1, var_2, var_3, var_4, var_5, var_6 )
{
    level.player usebuttonpressed();
}
"""
        self.assertIn("level.player usebuttonpressed()", extract_function(source, "waittill_trigger"))

    def test_patchers_reject_missing_or_already_patched_functions(self):
        with self.assertRaises(PatchError):
            patch_black_ice_source("some_other_function()\n{\n}\n")
        with self.assertRaises(PatchError):
            patch_black_ice_source(patch_black_ice_source(BLACK_ICE_FIXTURE))
        with self.assertRaises(PatchError):
            patch_player_scripted_anim_source("waittill_trigger()\n{\n}\n")

    def test_hash_pin_rejects_stale_or_wrong_source(self):
        data = b"synthetic"
        with self.assertRaises(PatchError):
            verify_pinned_source(data, EXPECTED_BLACK_ICE_UTIL_SHA256, "fixture")
        with self.assertRaises(PatchError):
            verify_pinned_source(data, EXPECTED_PLAYER_SCRIPTED_ANIM_UTIL_SHA256, "fixture")
        self.assertEqual(len(hashlib.sha256(data).hexdigest()), 64)

    def test_shared_helper_pair_requires_identical_source_bytes(self):
        source = b"same pinned helper"
        verify_shared_source_pair(source, source)
        with self.assertRaises(PatchError):
            verify_shared_source_pair(source, source + b"changed")

    def test_write_bundle_creates_manifest_and_distinct_mission_outputs(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            outputs = {
                "black_ice/black_ice_util.player_aware.gsc": b"black ice",
                "carrier/player_scripted_anim_util.player_aware.gsc": b"shared",
                "cornered/player_scripted_anim_util.player_aware.gsc": b"shared",
            }
            manifest = {"outputs": {name: {"bytes": len(data)} for name, data in outputs.items()}}
            path = write_bundle(outputs, manifest, root / "out")
            self.assertTrue(path.is_file())
            self.assertTrue((root / "out/black_ice/black_ice_util.player_aware.gsc").is_file())
            self.assertTrue((root / "out/carrier/player_scripted_anim_util.player_aware.gsc").is_file())
            self.assertTrue((root / "out/cornered/player_scripted_anim_util.player_aware.gsc").is_file())


if __name__ == "__main__":
    unittest.main()
