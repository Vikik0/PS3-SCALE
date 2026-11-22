import unittest
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
BRIDGE = PROJECT / "client" / "ps3" / "gsc" / "ghc_campaign_local_session.gsc"


def function_body(source: str, name: str) -> str:
    marker = name + "("
    start = source.index(marker)
    opening = source.index("{", start)
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

    def test_bridge_reuses_engine_created_players_and_refreshes_campaign_array(self):
        self.assertIn('getentarray( "player", "classname" )', self.source)
        self.assertIn("level.playerpiggyback = var_1", self.source)
        self.assertIn("issplitscreen()", self.source)
        self.assertIn("var_2 issplitscreenplayer()", self.source)
        self.assertIn("var_2 issplitscreenplayerprimary()", self.source)
        self.assertIn("var_0 < 200", self.source)
        self.assertIn("wait 0.05", self.source)

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


if __name__ == "__main__":
    unittest.main()
