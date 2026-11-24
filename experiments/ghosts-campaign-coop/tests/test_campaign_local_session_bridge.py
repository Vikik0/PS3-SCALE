import hashlib
import json
import re
import unittest
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
BRIDGE = PROJECT / "client" / "ps3" / "gsc" / "ghc_campaign_local_session.gsc"
MANIFEST = PROJECT / "client" / "ps3" / "gsc" / "ghc_campaign_local_session.manifest.json"


def function_body(source: str, name: str) -> str:
    match = re.search(rf"(?m)^{re.escape(name)}\s*\([^)]*\)\s*\{{", source)
    if match is None:
        raise AssertionError(f"function definition not found: {name}")
    opening = source.index("{", match.start())
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unterminated GSC function: {name}")


class CampaignLocalSessionBridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = BRIDGE.read_text(encoding="utf-8")

    def test_bridge_never_uses_bot_or_test_client_creation_for_local_player_two(self):
        source = self.source.lower()
        for forbidden in (
            "spawntestclient(",
            "addtestclient(",
            "addbot(",
            "sv_addtestclient",
            "sv_spawntestclient",
        ):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, source)
        self.assertIn("reuses the engine-created local split-screen players", source)
        self.assertIn("it does not create clients", source)

    def test_bridge_reuses_engine_created_players_and_refreshes_campaign_array(self):
        self.assertIn('getentarray( "player", "classname" )', self.source)
        self.assertIn("level.playerpiggyback = var_1", self.source)
        self.assertIn("issplitscreen()", self.source)
        self.assertIn("var_2 issplitscreenplayer()", self.source)
        self.assertIn("var_2 issplitscreenplayerprimary()", self.source)
        self.assertIn("var_0 < 200", self.source)
        self.assertIn("wait 0.05", self.source)

    def test_ready_session_revalidates_both_native_players_against_live_entity_array(self):
        body = function_body(self.source, "ghc_campaign_native_pair_is_current")
        self.assertIn('getentarray( "player", "classname" )', body)
        self.assertIn("level.playerpiggyback = var_0", body)
        self.assertIn("level.ghc_campaign_player_0 issplitscreenplayer()", body)
        self.assertIn("level.ghc_campaign_player_0 issplitscreenplayerprimary()", body)
        self.assertIn("level.ghc_campaign_player_1 issplitscreenplayer()", body)
        self.assertIn("level.ghc_campaign_player_1 issplitscreenplayerprimary()", body)
        self.assertIn("var_5 == level.ghc_campaign_player_0", body)
        self.assertIn("var_5 == level.ghc_campaign_player_1", body)
        self.assertIn("if ( var_1 != 2 || !var_2 || !var_3 )", body)
        self.assertNotIn("wait", body.lower())

    def test_player_specific_actions_revalidate_native_pair_before_mutation(self):
        main = function_body(self.source, "main")
        loadout = function_body(self.source, "ghc_campaign_apply_clockwork_player_two_loadout")
        begin = function_body(self.source, "ghc_campaign_drill_begin")
        finish = function_body(self.source, "ghc_campaign_drill_finish")
        self.assertIn("ghc_campaign_native_pair_is_current()", main)
        for body in (loadout, begin, finish):
            self.assertIn("ghc_campaign_native_pair_is_current()", body)
        self.assertIn("ghc_campaign_invalidate_local_session()", loadout)
        self.assertIn("ghc_campaign_invalidate_local_session()", begin)
        self.assertIn("ghc_campaign_invalidate_local_session()", finish)

    def test_stale_session_clears_secondary_alias_and_preserves_only_valid_primary(self):
        main = function_body(self.source, "main")
        clear = function_body(self.source, "ghc_campaign_clear_session_aliases")
        invalidate = function_body(self.source, "ghc_campaign_invalidate_local_session")
        self.assertIn("ghc_campaign_clear_session_aliases()", main)
        self.assertIn("level.ghc_campaign_player_0 issplitscreenplayerprimary()", clear)
        self.assertIn("level.player = level.ghc_campaign_player_0", clear)
        self.assertIn("level.player = undefined", clear)
        self.assertIn("level.playengineeffects = undefined", clear)
        self.assertIn("level.ghc_campaign_player_0 = undefined", clear)
        self.assertIn("level.ghc_campaign_player_1 = undefined", clear)
        self.assertIn("level.playerpiggyback = undefined", clear)
        self.assertIn("ghc_campaign_clear_session_aliases()", invalidate)

    def test_stale_pair_invalidation_clears_ready_and_releases_both_drill_locks(self):
        body = function_body(self.source, "ghc_campaign_invalidate_local_session")
        self.assertIn("level.ghc_campaign_session_ready = 0", body)
        self.assertIn("level.ghc_campaign_session_status = 8", body)
        self.assertIn("ghc_campaign_drill_abort_slot( 0 )", body)
        self.assertIn("ghc_campaign_drill_abort_slot( 1 )", body)
        self.assertIn("return 0", body)

    def test_startup_retries_identity_binding_until_native_flags_are_ready(self):
        body = function_body(self.source, "main")
        self.assertIn("var_0 < 200", body)
        self.assertIn("if ( ghc_campaign_bind_native_local_pair() )", body)
        self.assertIn("return 1", body)
        self.assertLess(
            body.index("if ( ghc_campaign_bind_native_local_pair() )"),
            body.index("wait 0.05"),
        )
        self.assertIn("level.playerpiggyback = undefined", body)
        self.assertIn("if ( ghc_campaign_bind_native_local_pair() )", body)
        self.assertIn("return 0", body)

    def test_campaign_aliases_are_bound_only_after_exact_native_pair_validation(self):
        body = function_body(self.source, "ghc_campaign_bind_native_local_pair")
        self.assertIn("if ( var_0 != 2 )", body)
        self.assertIn("level.ghc_campaign_session_status = 6", body)
        self.assertIn("level.ghc_campaign_player_0 = var_2", body)
        self.assertIn("level.ghc_campaign_player_1 = var_2", body)
        self.assertIn("level.player = level.ghc_campaign_player_0", body)
        self.assertIn("level.playengineeffects = level.ghc_campaign_player_1", body)
        self.assertLess(
            body.index("if ( var_0 != 2 )"),
            body.index("level.player = level.ghc_campaign_player_0"),
        )

    def test_primary_secondary_identity_is_not_inferred_from_array_order(self):
        body = function_body(self.source, "ghc_campaign_bind_native_local_pair")
        self.assertIn("var_2 issplitscreenplayerprimary()", body)
        self.assertNotIn("level.playerpiggyback[0]", body)
        self.assertNotIn("level.playerpiggyback[1]", body)

    def test_bridge_fails_closed_without_native_split_screen_or_two_players(self):
        body = function_body(self.source, "ghc_campaign_bind_native_local_pair")
        self.assertIn("if ( !issplitscreen() )", body)
        for status in range(2, 8):
            self.assertIn(f"level.ghc_campaign_session_status = {status}", body)
        self.assertIn("level.ghc_campaign_session_ready = 0", body)

    def test_player_accessor_only_returns_validated_slots(self):
        body = function_body(self.source, "ghc_campaign_player_for_slot")
        self.assertIn("level.ghc_campaign_session_ready", body)
        self.assertIn("return level.ghc_campaign_player_0", body)
        self.assertIn("return level.ghc_campaign_player_1", body)
        self.assertIn("return undefined", body)

    def test_clockwork_loadout_is_applied_only_to_secondary_native_player(self):
        body = function_body(self.source, "ghc_campaign_apply_clockwork_player_two_loadout")
        self.assertIn("ghc_campaign_player_for_slot( 1 )", body)
        self.assertIn("var_0 issplitscreenplayer()", body)
        self.assertIn("var_0 issplitscreenplayerprimary()", body)
        self.assertIn('var_0 setviewmodel( "viewhands_fed_army_arctic" )', body)
        self.assertIn('var_0 takeallweapons()', body)
        self.assertIn('var_0 giveweapon( "gm6+scopegm6_sp+silencer03_sp" )', body)
        self.assertIn('var_0 giveweapon( "cz805bren+reflex_sp+silencer_sp" )', body)
        self.assertIn('var_0 switchtoweaponimmediate( "cz805bren+reflex_sp+silencer_sp" )', body)
        self.assertIn('var_0 setoffhandprimaryclass( "frag" )', body)
        self.assertIn('var_0 setoffhandsecondaryclass( "flash" )', body)
        self.assertIn("level.ghc_campaign_clockwork_p2_loadout_applied = 1", body)
        self.assertNotIn("level.player ", body)

    def test_loadout_is_idempotent_and_does_not_touch_primary_loadout(self):
        body = function_body(self.source, "ghc_campaign_apply_clockwork_player_two_loadout")
        self.assertIn("level.ghc_campaign_clockwork_p2_loadout_applied", body)
        self.assertIn("return 1", body)
        self.assertNotIn("level.ghc_campaign_player_0", body)


    def test_new_native_pair_rebind_resets_player_two_loadout_guard(self):
        body = function_body(self.source, "ghc_campaign_bind_native_local_pair")
        reset = body.index("level.ghc_campaign_clockwork_p2_loadout_applied = 0")
        ready = body.index("level.ghc_campaign_session_ready = 1")
        self.assertLess(body.index("level.player = level.ghc_campaign_player_0"), reset)
        self.assertLess(reset, ready)

    def test_ready_session_is_reused_only_if_both_native_players_still_match(self):
        body = function_body(self.source, "main")
        return_position = body.index("return 1")
        for check in (
            "issplitscreen()",
            "level.ghc_campaign_player_0 issplitscreenplayer()",
            "level.ghc_campaign_player_0 issplitscreenplayerprimary()",
            "level.ghc_campaign_player_1 issplitscreenplayer()",
            "!level.ghc_campaign_player_1 issplitscreenplayerprimary()",
            "level.player == level.ghc_campaign_player_0",
            "level.playengineeffects == level.ghc_campaign_player_1",
        ):
            self.assertIn(check, body)
            self.assertLess(body.index(check), return_position)
        self.assertLess(return_position, body.index("ghc_campaign_drill_abort_slot( 0 )"))

    def test_rebind_releases_only_active_drill_locks_before_clearing_player_handles(self):
        body = function_body(self.source, "main")
        guard = body.index("level.ghc_campaign_drill_state_initialized")
        abort_zero = body.index("ghc_campaign_drill_abort_slot( 0 )")
        abort_one = body.index("ghc_campaign_drill_abort_slot( 1 )")
        clear_handles = body.index("level.ghc_campaign_player_0 = undefined")
        self.assertLess(guard, abort_zero)
        self.assertLess(abort_zero, abort_one)
        self.assertLess(abort_one, clear_handles)

    def test_native_pair_binding_initializes_shared_drill_state_once(self):
        bind = function_body(self.source, "ghc_campaign_bind_native_local_pair")
        init = function_body(self.source, "ghc_campaign_drill_state_init")
        self.assertIn("ghc_campaign_drill_state_init()", bind)
        self.assertIn("level.ghc_campaign_drill_state_initialized", init)
        self.assertIn("return", init)
        for name in (
            "level.ghc_campaign_drill_owner_0 = -1",
            "level.ghc_campaign_drill_owner_1 = -1",
            "level.ghc_campaign_drill_active_player_0 = -1",
            "level.ghc_campaign_drill_active_player_1 = -1",
            "level.ghc_campaign_drill_complete_0 = 0",
            "level.ghc_campaign_drill_complete_1 = 0",
        ):
            self.assertIn(name, init)

    def test_drill_claim_requires_a_validated_player_and_one_of_two_spots(self):
        body = function_body(self.source, "ghc_campaign_drill_begin")
        self.assertIn("level.ghc_campaign_session_ready", body)
        self.assertIn("ghc_campaign_drill_player_slot( var_0 )", body)
        self.assertIn("var_1 < 0 || var_1 > 1", body)
        self.assertIn("ghc_campaign_drill_active_spot_for_player", body)
        self.assertIn("ghc_campaign_drill_is_complete", body)
        self.assertIn("ghc_campaign_drill_owner_for_spot", body)
        self.assertIn("ghc_campaign_drill_set_owner_for_spot", body)
        self.assertIn("ghc_campaign_drill_set_active_spot_for_player", body)
        self.assertNotIn("wait", body.lower())

    def test_drill_finish_is_owner_only_and_completion_is_shared(self):
        body = function_body(self.source, "ghc_campaign_drill_finish")
        self.assertIn("ghc_campaign_drill_player_slot( var_0 )", body)
        self.assertIn("ghc_campaign_drill_owner_for_spot( var_1 ) != var_3", body)
        self.assertIn("ghc_campaign_drill_active_spot_for_player( var_3 ) != var_1", body)
        self.assertIn("if ( var_2 )", body)
        self.assertIn("ghc_campaign_drill_set_complete_for_spot( var_1, 1 )", body)
        self.assertIn("ghc_campaign_drill_set_owner_for_spot( var_1, -1 )", body)
        self.assertIn("ghc_campaign_drill_set_active_spot_for_player( var_3, -1 )", body)
        self.assertNotIn("wait", body.lower())

    def test_drill_cancel_and_abort_release_without_marking_complete(self):
        cancel = function_body(self.source, "ghc_campaign_drill_cancel")
        abort_player = function_body(self.source, "ghc_campaign_drill_abort_player")
        abort_slot = function_body(self.source, "ghc_campaign_drill_abort_slot")
        self.assertIn("ghc_campaign_drill_finish( var_0, var_1, 0 )", cancel)
        self.assertIn("ghc_campaign_drill_abort_slot( var_1 )", abort_player)
        self.assertIn("ghc_campaign_drill_set_owner_for_spot( var_1, -1 )", abort_slot)
        self.assertIn("ghc_campaign_drill_set_active_spot_for_player( var_0, -1 )", abort_slot)
        self.assertNotIn("set_complete", abort_slot)
        self.assertIn("ghc_campaign_drill_owner_for_spot( var_1 ) != var_0", abort_slot)
        self.assertIn("level.ghc_campaign_drill_state_initialized", abort_slot)
        self.assertNotIn("level.ghc_campaign_session_ready", abort_slot)

    def test_slot_lookup_remains_available_for_cleanup_after_session_flag_drops(self):
        body = function_body(self.source, "ghc_campaign_drill_player_slot")
        self.assertIn("level.ghc_campaign_player_0", body)
        self.assertIn("level.ghc_campaign_player_1", body)
        self.assertNotIn("ghc_campaign_session_ready", body)

    def test_manifest_pins_source_and_compiled_gsc_artifact(self):
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        source_hash = hashlib.sha256(BRIDGE.read_bytes()).hexdigest()
        if manifest.get("source_sha256"):
            self.assertRegex(manifest["source_sha256"], r"^[0-9a-f]{64}$")
            self.assertEqual(source_hash, manifest["source_sha256"])
        if manifest.get("compiled_gscbin_sha256"):
            self.assertRegex(manifest["compiled_gscbin_sha256"], r"^[0-9a-f]{64}$")
        if manifest.get("compiled_gscbin_bytes"):
            self.assertGreater(manifest["compiled_gscbin_bytes"], 0)
        self.assertIn("not injected or runtime-tested", manifest["status"])

    def test_bridge_does_not_fake_or_spawn_a_second_player(self):
        lowered = self.source.lower()
        forbidden = (
            "spawnplayer(", "spawnclient(", "cloneplayer(",
            "playercmd_spawn", "playercmd_cloneplayer",
            "cellpadgetdata(", "setdvar(", 'getdvar( "coop"'
        )
        for token in forbidden:
            with self.subTest(token=token):
                self.assertNotIn(token, lowered)


    def test_successful_bind_starts_one_guarded_session_monitor(self):
        main = function_body(self.source, "main")
        start = function_body(self.source, "ghc_campaign_start_session_monitor")
        self.assertGreaterEqual(main.count("ghc_campaign_start_session_monitor()"), 3)
        self.assertIn("level.ghc_campaign_session_monitor_active", start)
        self.assertIn("thread ghc_campaign_watch_native_session()", start)
        self.assertIn("return 1", start)

    def test_session_monitor_revalidates_both_native_entities_and_campaign_aliases(self):
        body = function_body(self.source, "ghc_campaign_watch_native_session")
        self.assertIn("ghc_campaign_native_pair_is_current()", body)
        self.assertIn("level.player == level.ghc_campaign_player_0", body)
        self.assertIn("level.playengineeffects == level.ghc_campaign_player_1", body)
        self.assertIn("while ( isdefined( level.ghc_campaign_session_ready )", body)

    def test_session_monitor_requires_two_seconds_of_consecutive_failures(self):
        body = function_body(self.source, "ghc_campaign_watch_native_session")
        self.assertIn("var_0 >= 8", body)
        self.assertIn("wait 0.25", body)
        self.assertIn("ghc_campaign_invalidate_local_session()", body)

    def test_session_monitor_resets_failure_count_after_valid_pair_returns(self):
        body = function_body(self.source, "ghc_campaign_watch_native_session")
        self.assertIn("var_0 = 0", body)
        self.assertIn("level.ghc_campaign_session_monitor_active = undefined", body)

    def test_session_monitor_does_not_create_or_rebind_players(self):
        body = function_body(self.source, "ghc_campaign_watch_native_session").lower()
        for token in ("spawnplayer(", "cloneplayer(", "playercmd_spawn", "playercmd_cloneplayer", "ghc_campaign_bind_native_local_pair("):
            self.assertNotIn(token, body)


if __name__ == "__main__":
    unittest.main()
