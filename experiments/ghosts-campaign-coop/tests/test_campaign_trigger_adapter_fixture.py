import re
import unittest
from pathlib import Path

from tools.mission_event_activator_audit import _mask_comments_and_strings


PROJECT = Path(__file__).resolve().parents[1]
FIXTURE = PROJECT / "tests" / "fixtures" / "ghc_campaign_trigger_adapter.gsc"


def function_body(source: str, name: str) -> str:
    masked = _mask_comments_and_strings(source)
    match = re.search(rf"\b{re.escape(name)}\s*\([^)]*\)\s*\{{", masked)
    if match is None:
        raise AssertionError(f"function definition not found: {name}")
    opening = masked.index("{", match.start())
    depth = 0
    for index in range(opening, len(masked)):
        if masked[index] == "{":
            depth += 1
        elif masked[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unterminated function: {name}")


class CampaignTriggerAdapterFixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = FIXTURE.read_text(encoding="utf-8")

    def test_fixture_is_explicitly_not_a_retail_patch_or_payload(self):
        lower = self.source.lower()
        self.assertIn("compile-only", lower)
        self.assertIn("not a retail mission patch", lower)
        self.assertIn("not a ps3", lower)

    def test_adapter_checks_session_and_event_range_before_waiting(self):
        body = function_body(self.source, "ghc_campaign_wait_for_owned_trigger")
        self.assertIn("level.ghc_campaign_session_ready", body)
        self.assertIn("event_id < 0 || event_id > 15", body)
        self.assertLess(body.index("event_id < 0"), body.index('waittill( "trigger", activator )'))

    def test_adapter_revalidates_native_pair_after_trigger_wait(self):
        body = function_body(self.source, "ghc_campaign_wait_for_owned_trigger")
        wait = body.index('waittill( "trigger", activator )')
        second_pair_check = body.index("ghc_campaign_native_pair_is_current()", wait)
        player_slot = body.index("ghc_campaign_event_lock_player_slot( activator )")
        acquire = body.index("ghc_campaign_event_lock_acquire( activator, event_id )")
        self.assertLess(wait, second_pair_check)
        self.assertLess(second_pair_check, player_slot)
        self.assertLess(player_slot, acquire)

    def test_adapter_returns_the_actual_activator_and_never_substitutes_global_player(self):
        body = function_body(self.source, "ghc_campaign_wait_for_owned_trigger")
        self.assertIn("return activator", body)
        self.assertNotIn("level.player", self.source)
        self.assertNotIn("waittill_trigger", self.source)

    def test_adapter_acquires_the_lock_for_the_validated_activator(self):
        body = function_body(self.source, "ghc_campaign_wait_for_owned_trigger")
        self.assertIn("ghc_campaign_event_lock_player_slot( activator )", body)
        self.assertIn("ghc_campaign_event_lock_acquire( activator, event_id )", body)
        self.assertIn("ghc_campaign_invalidate_local_session()", body)

    def test_release_and_abort_helpers_take_an_explicit_player_argument(self):
        release = function_body(self.source, "ghc_campaign_release_owned_trigger")
        abort = function_body(self.source, "ghc_campaign_abort_owned_trigger")
        self.assertIn("ghc_campaign_event_lock_release( player, event_id, completed )", release)
        self.assertIn("ghc_campaign_event_lock_abort_player( player )", abort)
        self.assertNotIn("level.player", release + abort)


if __name__ == "__main__":
    unittest.main()
