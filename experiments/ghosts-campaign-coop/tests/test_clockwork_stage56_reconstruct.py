import hashlib
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from tools.clockwork_stage56_reconstruct import patch_source, main
from tools.clockwork_stage56_audit import audit

FIXTURE = Path(__file__).parent / "fixtures" / "clockwork_stage56.gsc"
SOURCE = FIXTURE.read_text(encoding="utf-8")
SOURCE_HASH = hashlib.sha256(SOURCE.encode("utf-8")).hexdigest()


class ClockworkStage56ReconstructionTests(unittest.TestCase):
    def test_stage2_patch_input_and_stage56_toolchain_fixture_are_separate(self):
        toolchain_fixture = FIXTURE.with_name("clockwork_stage56_toolchain.gsc")
        toolchain_source = toolchain_fixture.read_text(encoding="utf-8")

        self.assertNotIn("clockwork_get_secondary_player", SOURCE)
        self.assertIn("Synthetic Stage 5/6 GSC Tool fixture", toolchain_source)
        self.assertIn("clockwork_get_secondary_player", toolchain_source)
        self.assertIn("clockwork_apply_vault_blast", toolchain_source)
        self.assertIn("level.ghc_vault_player_two_grenades", toolchain_source)
        self.assertIn("secondary_player allowsprint", toolchain_source)
        self.assertIn('cinematicingameloopresident( "clockwork_vault_scan" )', toolchain_source)
        self.assertIn('cinematicingame( "clockwork_vault_drill" )', toolchain_source)

    def test_reconstructs_stage5_and_stage6_documented_behavior(self):
        stage5, stage6 = patch_source(SOURCE, SOURCE_HASH)
        self.assertIn("clockwork_apply_vault_blast( level.player, var_3.origin )", stage5)
        self.assertNotIn("secondary_player allowsprint( 1 )", stage5)
        self.assertIn("secondary_player allowsprint( 1 )", stage6)
        self.assertIn("secondary_player == level.ghc_vault_player_two", stage6)
        self.assertEqual(audit(stage6)["status"], "PASS")

    def test_stage5_and_stage6_preserve_one_call_site_per_vault_cinematic(self):
        stage5, stage6 = patch_source(SOURCE, SOURCE_HASH)
        scan_call = 'cinematicingameloopresident( "clockwork_vault_scan" )'
        drill_call = 'cinematicingame( "clockwork_vault_drill" )'

        for reconstructed in (stage5, stage6):
            self.assertEqual(reconstructed.count(scan_call), 1)
            self.assertEqual(reconstructed.count(drill_call), 1)

    def test_secondary_identity_is_native_split_screen_only(self):
        _, stage6 = patch_source(SOURCE, SOURCE_HASH)
        self.assertIn("issplitscreen()", stage6)
        self.assertIn("secondary_player issplitscreenplayer()", stage6)
        self.assertIn("secondary_player issplitscreenplayerprimary()", stage6)

    def test_rejects_wrong_input_hash(self):
        with self.assertRaises(ValueError):
            patch_source(SOURCE, "0" * 64)

    def test_rejects_duplicate_stage_helpers(self):
        duplicate = "clockwork_get_secondary_player( ai )\\n{ return undefined; }\\n" + SOURCE
        with self.assertRaises(ValueError):
            patch_source(duplicate, hashlib.sha256(duplicate.encode()).hexdigest())

    def test_rejects_missing_pinned_blast_block(self):
        changed = SOURCE.replace('level.player playrumbleonentity( "artillery_rumble" );', "")
        with self.assertRaises(ValueError):
            patch_source(changed, hashlib.sha256(changed.encode()).hexdigest())

    def test_cli_does_not_overwrite_input(self):
        with TemporaryDirectory() as temp:
            source = Path(temp) / "input.gsc"
            source.write_text(SOURCE, encoding="utf-8")
            result = main([str(source), str(source)])
            self.assertEqual(result, 1)

    def test_audit_blocks_missing_secondary_dog_notification(self):
        _, stage6 = patch_source(SOURCE, SOURCE_HASH)
        changed = stage6.replace('secondary_player notify( "dog_attack_override" );', "")
        self.assertEqual(audit(changed)["status"], "BLOCKED")


if __name__ == "__main__":
    unittest.main()
