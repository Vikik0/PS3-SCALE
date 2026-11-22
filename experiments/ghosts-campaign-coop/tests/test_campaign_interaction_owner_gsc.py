import unittest
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT / "client" / "ps3" / "gsc" / "ghc_campaign_interaction_owner.gsc"


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


class CampaignInteractionOwnerGscTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text(encoding="utf-8")

    def test_helper_waits_for_validated_local_session_with_a_bound(self):
        self.assertIn("level.ghc_campaign_session_ready", self.source)
        self.assertIn("var_0 < 200", self.source)
        self.assertIn("wait 0.05", self.source)
        self.assertIn('level notify( "ghc_campaign_interactions_ready" )', self.source)

    def test_player_identity_uses_validated_primary_and_secondary_entities(self):
        body = function_body(self.source, "ghc_campaign_interaction_slot_for_player")
        self.assertIn("level.ghc_campaign_player_0", body)
        self.assertIn("level.ghc_campaign_player_1", body)
        self.assertIn("return 0", body)
        self.assertIn("return 1", body)
        self.assertIn("return 2", body)

    def test_begin_enforces_shared_spot_and_per_player_ownership(self):
        body = function_body(self.source, "ghc_campaign_interaction_begin")
        self.assertIn("ghc_campaign_interaction_slot_for_player", body)
        self.assertIn("level.ghc_campaign_interaction_completed_by_spot", body)
        self.assertIn("level.ghc_campaign_interaction_active_spot_by_player", body)
        self.assertIn("level.ghc_campaign_interaction_owner_by_spot", body)
        self.assertIn("level.ghc_campaign_interaction_status = 6", body)
        self.assertIn("level.ghc_campaign_interaction_status = 7", body)
        self.assertIn("level.ghc_campaign_interaction_owner_by_spot[var_1] = var_0", body)

    def test_finish_requires_the_same_player_to_own_the_same_spot(self):
        body = function_body(self.source, "ghc_campaign_interaction_finish")
        self.assertIn("level.ghc_campaign_interaction_owner_by_spot[var_1] != var_0", body)
        self.assertIn("level.ghc_campaign_interaction_active_spot_by_player[var_3] != var_1", body)
        self.assertIn("level.ghc_campaign_interaction_completed_by_spot[var_1] = 1", body)
        self.assertIn("level.ghc_campaign_interaction_owner_by_spot[var_1] = undefined", body)
        self.assertIn("level.ghc_campaign_interaction_active_spot_by_player[var_3] = undefined", body)

    def test_complete_and_cancel_share_the_same_owner_guard(self):
        complete = function_body(self.source, "ghc_campaign_interaction_complete")
        cancel = function_body(self.source, "ghc_campaign_interaction_cancel")
        self.assertIn("ghc_campaign_interaction_finish( var_0, var_1, 1 )", complete)
        self.assertIn("ghc_campaign_interaction_finish( var_0, var_1, 0 )", cancel)

    def test_abort_releases_owner_without_completing_the_spot(self):
        body = function_body(self.source, "ghc_campaign_interaction_abort_player")
        self.assertIn("ghc_campaign_interaction_cancel", body)
        self.assertNotIn("ghc_campaign_interaction_complete", body)

    def test_helper_does_not_spawn_players_or_poll_game_input(self):
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
