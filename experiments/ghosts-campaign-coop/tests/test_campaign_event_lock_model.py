import json
import re
import unittest
from pathlib import Path

from tools.mission_event_activator_audit import _mask_comments_and_strings


PROJECT = Path(__file__).resolve().parents[1]
GSC = PROJECT / "client" / "ps3" / "gsc" / "ghc_campaign_local_session.gsc"
MODEL_HEADER = PROJECT / "tests" / "campaign_event_lock_model.h"
MODEL_SOURCE = PROJECT / "tests" / "campaign_event_lock_model.c"
EVENT_MAP = PROJECT / "reports" / "PHASE88_MISSION_EVENT_LOCK_MAP.json"


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


class CampaignEventLockModelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.gsc = GSC.read_text(encoding="utf-8")
        cls.header = MODEL_HEADER.read_text(encoding="utf-8")
        cls.model = MODEL_SOURCE.read_text(encoding="utf-8")
        cls.event_map = json.loads(EVENT_MAP.read_text(encoding="utf-8"))

    def test_gsc_event_lock_api_is_complete(self):
        for name in (
            "ghc_campaign_event_lock_init",
            "ghc_campaign_event_lock_player_slot",
            "ghc_campaign_event_lock_acquire",
            "ghc_campaign_event_lock_release",
            "ghc_campaign_event_lock_abort_slot",
            "ghc_campaign_event_lock_abort_player",
            "ghc_campaign_event_lock_clear_active",
            "ghc_campaign_event_lock_owner",
            "ghc_campaign_event_lock_active_for_player",
            "ghc_campaign_event_lock_is_complete",
            "ghc_campaign_event_lock_is_owner",
            "ghc_campaign_event_lock_reset",
        ):
            with self.subTest(function=name):
                function_body(self.gsc, name)

    def test_gsc_table_is_sized_for_sixteen_events_and_two_local_players(self):
        init = function_body(self.gsc, "ghc_campaign_event_lock_init")
        self.assertIn("var_0 < 16", init)
        self.assertIn("var_1 < 2", init)
        self.assertIn("event_lock_owner", init)
        self.assertIn("event_lock_active", init)
        self.assertIn("event_lock_complete", init)

    def test_gsc_acquire_is_fail_closed_and_has_no_yield_in_critical_section(self):
        acquire = function_body(self.gsc, "ghc_campaign_event_lock_acquire")
        self.assertIn("level.ghc_campaign_session_ready", acquire)
        self.assertIn("ghc_campaign_native_pair_is_current()", acquire)
        self.assertIn("ghc_campaign_invalidate_local_session()", acquire)
        self.assertIn("event_id < 0 || event_id > 15", acquire)
        self.assertIn("event_lock_owner[event_id] != -1", acquire)
        self.assertIn("event_lock_active[var_0] != -1", acquire)
        self.assertIn("event_lock_complete[event_id]", acquire)
        self.assertLess(
            acquire.index("level.ghc_campaign_event_lock_owner[event_id] = var_0"),
            acquire.index("level.ghc_campaign_event_lock_active[var_0] = event_id"),
        )
        self.assertNotIn("wait", acquire.lower())

    def test_gsc_release_requires_owner_and_clears_both_sides(self):
        release = function_body(self.gsc, "ghc_campaign_event_lock_release")
        self.assertIn("level.ghc_campaign_event_lock_owner[event_id] != var_0", release)
        self.assertIn("level.ghc_campaign_event_lock_active[var_0] != event_id", release)
        self.assertIn("if ( completed )", release)
        self.assertIn("level.ghc_campaign_event_lock_complete[event_id] = 1", release)
        self.assertIn("level.ghc_campaign_event_lock_owner[event_id] = -1", release)
        self.assertIn("level.ghc_campaign_event_lock_active[var_0] = -1", release)

    def test_gsc_abort_and_session_clear_release_without_completing(self):
        abort = function_body(self.gsc, "ghc_campaign_event_lock_abort_slot")
        clear = function_body(self.gsc, "ghc_campaign_event_lock_clear_active")
        self.assertIn("level.ghc_campaign_event_lock_owner[var_0] = -1", abort)
        self.assertIn("level.ghc_campaign_event_lock_active[slot] = -1", abort)
        self.assertNotIn("event_lock_complete", abort)
        self.assertIn("ghc_campaign_event_lock_abort_slot( 0 )", clear)
        self.assertIn("ghc_campaign_event_lock_abort_slot( 1 )", clear)

    def test_gsc_reset_cannot_clear_an_event_while_it_has_an_owner(self):
        reset = function_body(self.gsc, "ghc_campaign_event_lock_reset")
        self.assertIn("event_lock_owner[event_id] != -1", reset)
        self.assertIn("event_lock_complete[event_id] = 0", reset)

    def test_four_mission_event_ids_remain_stable_and_unique(self):
        expected = {
            ("Black Ice", "event_player_turns_key"): 0,
            ("Carrier", "cut_the_rope"): 1,
            ("Cornered", "gun_down_trigger_internal"): 2,
            ("Cornered", "upload_virus_enter"): 3,
        }
        observed = {
            (event.get("mission"), event.get("routine")): event.get("event_lock_id")
            for event in self.event_map.get("events", [])
            if event.get("event_lock_id") is not None
        }
        self.assertEqual(observed, expected)
        self.assertEqual(len(set(observed.values())), 4)

    def test_host_model_matches_dimensions_and_is_not_a_prx_module(self):
        self.assertIn("GHC_CAMPAIGN_EVENT_LOCK_COUNT 16u", self.header)
        self.assertIn("GHC_CAMPAIGN_EVENT_LOCK_PLAYER_COUNT 2u", self.header)
        self.assertIn("HOST-ONLY contract model", self.header)
        self.assertIn("not linked into the PS3 PRX", self.header)
        self.assertIn("GHC_CAMPAIGN_EVENT_LOCK_SESSION_STALE", self.model)
        self.assertIn("completed_by_event", self.model)

    def test_host_model_requires_boolean_flags_and_preserves_completed_history(self):
        self.assertIn("return value <= 1u", self.model)
        clear = function_body(self.model, "ghc_campaign_event_lock_model_clear_active")
        self.assertIn("ghc_campaign_event_lock_model_abort_player", clear)
        self.assertNotIn("completed_by_event", clear)
        reset = function_body(self.model, "ghc_campaign_event_lock_model_reset_event")
        self.assertIn("owner_by_event[event_id] != GHC_CAMPAIGN_EVENT_LOCK_NO_OWNER", reset)


if __name__ == "__main__":
    unittest.main()
