import hashlib
import json
import unittest
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
ADAPTER = PROJECT / "client" / "ps3" / "gsc" / "ghc_campaign_player_registry.gsc"
MANIFEST = PROJECT / "client" / "ps3" / "gsc" / "ghc_campaign_player_registry.manifest.json"


class CampaignGscAdapterContractTests(unittest.TestCase):
    def test_registry_requires_two_native_split_screen_players(self):
        source = ADAPTER.read_text(encoding="utf-8")
        self.assertIn("level.playerpiggyback", source)
        self.assertIn("var_0 < 200", source)
        self.assertIn("wait 0.05", source)
        self.assertNotIn('level waittill( "level.players initialized" )', source)
        self.assertIn("issplitscreen()", source)
        self.assertIn("issplitscreenplayer()", source)
        self.assertIn("issplitscreenplayerprimary()", source)
        self.assertIn("var_1 issplitscreenplayerprimary()", source)
        self.assertIn("for ( var_0 = 0; var_0 < level.playerpiggyback.size; var_0++ )", source)
        self.assertNotIn("level.playerpiggyback[0]", source)
        self.assertNotIn("level.playerpiggyback[1]", source)
        self.assertIn('level notify( "ghc_campaign_players_ready" )', source)

    def test_registry_fails_closed_when_native_player_pair_is_missing(self):
        source = ADAPTER.read_text(encoding="utf-8")
        self.assertIn("level.ghc_campaign_coop_ready = 0", source)
        self.assertIn("level.ghc_campaign_player_count != 2", source)
        self.assertIn("return 0", source)

    def test_manifest_source_hash_matches_registry_and_records_binary(self):
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        source_hash = hashlib.sha256(ADAPTER.read_bytes()).hexdigest()
        self.assertEqual(manifest["source_sha256"], source_hash)
        self.assertEqual(manifest["compiled_gscbin_bytes"], 461)
        self.assertEqual(
            manifest["compiled_gscbin_sha256"],
            "aae81e8775ccc8af819f1b68787f35d178ba6fa292176ca6b700449a926f0bbe",
        )
        self.assertIn("status codes 0-6", manifest["status"])

    def test_registry_exposes_distinct_fail_closed_status_codes(self):
        source = ADAPTER.read_text(encoding="utf-8")
        self.assertIn("level.ghc_campaign_registry_status = 0", source)
        for status in range(1, 7):
            self.assertIn(f"level.ghc_campaign_registry_status = {status}", source)
        self.assertIn("level.ghc_campaign_registry_status = 1", source)

    def test_registry_does_not_attempt_to_spawn_or_clone_a_player(self):
        source = ADAPTER.read_text(encoding="utf-8").lower()
        forbidden = (
            "spawnplayer(", "spawnclient(", "cloneplayer(",
            "playercmd_spawn", "playercmd_cloneplayer",
        )
        for token in forbidden:
            with self.subTest(token=token):
                self.assertNotIn(token, source)

    def test_slot_accessor_only_returns_the_two_validated_native_slots(self):
        source = ADAPTER.read_text(encoding="utf-8")
        self.assertIn("return level.ghc_campaign_player_0", source)
        self.assertIn("return level.ghc_campaign_player_1", source)
        self.assertIn("return undefined", source)


if __name__ == "__main__":
    unittest.main()
