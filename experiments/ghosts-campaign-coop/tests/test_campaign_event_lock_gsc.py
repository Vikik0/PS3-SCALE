import hashlib
import json
import re
import unittest
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
GSC_DIR = PROJECT / "client" / "ps3" / "gsc"
SESSION = GSC_DIR / "ghc_campaign_local_session.gsc"
SESSION_MANIFEST = GSC_DIR / "ghc_campaign_local_session.manifest.json"
EVENT_LOCK = SESSION
EVENT_MANIFEST = SESSION_MANIFEST


def function_body(source: str, name: str) -> str:
    match = re.search(rf"(?m)^[ \t]*{re.escape(name)}\s*\([^)]*\)\s*\{{", source)
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


class CampaignEventLockTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = EVENT_LOCK.read_text(encoding="utf-8")
        cls.session = SESSION.read_text(encoding="utf-8")

    def test_lock_supports_sixteen_shared_event_ids_and_two_player_slots(self):
        init = function_body(self.source, "ghc_campaign_event_lock_init")
        self.assertIn("var_0 < 16", init)
        self.assertIn("var_1 < 2", init)
        self.assertIn("level.ghc_campaign_event_lock_owner[var_0] = -1", init)
        self.assertIn("level.ghc_campaign_event_lock_complete[var_0] = 0", init)
        self.assertIn("level.ghc_campaign_event_lock_active[var_1] = -1", init)
        self.assertIn("level.ghc_campaign_event_lock_initialized = 1", init)

    def test_acquire_requires_current_native_pair_and_exclusive_event_owner(self):
        body = function_body(self.source, "ghc_campaign_event_lock_acquire")
        self.assertIn("level.ghc_campaign_session_ready", body)
        self.assertIn("ghc_campaign_native_pair_is_current()", body)
        self.assertIn("ghc_campaign_invalidate_local_session()", body)
        self.assertIn("ghc_campaign_event_lock_player_slot( player )", body)
        self.assertIn("ghc_campaign_event_lock_event_id_is_valid( event_id )", body)
        self.assertIn("level.ghc_campaign_event_lock_owner[event_id] != -1", body)
        self.assertIn("level.ghc_campaign_event_lock_active[var_0] != -1", body)
        self.assertIn("level.ghc_campaign_event_lock_complete[event_id]", body)
        self.assertIn("level.ghc_campaign_event_lock_owner[event_id] = var_0", body)
        self.assertIn("level.ghc_campaign_event_lock_active[var_0] = event_id", body)

    def test_release_and_owner_queries_fail_closed_before_lock_initialization(self):
        release = function_body(self.source, "ghc_campaign_event_lock_release")
        is_owner = function_body(self.source, "ghc_campaign_event_lock_is_owner")
        for body in (release, is_owner):
            self.assertIn("level.ghc_campaign_event_lock_initialized", body)
            self.assertIn("return 0", body)

    def test_release_is_owner_only_and_commits_completion_only_when_requested(self):
        body = function_body(self.source, "ghc_campaign_event_lock_release")
        self.assertIn("ghc_campaign_native_pair_is_current()", body)
        self.assertIn("level.ghc_campaign_event_lock_owner[event_id] != var_0", body)
        self.assertIn("level.ghc_campaign_event_lock_active[var_0] != event_id", body)
        self.assertIn("if ( completed )", body)
        self.assertIn("level.ghc_campaign_event_lock_complete[event_id] = 1", body)
        self.assertIn("level.ghc_campaign_event_lock_owner[event_id] = -1", body)
        self.assertIn("level.ghc_campaign_event_lock_active[var_0] = -1", body)

    def test_release_rejects_undefined_or_non_boolean_completion_flags(self):
        body = function_body(self.source, "ghc_campaign_event_lock_release")
        guard = body.index("if ( !isdefined( completed )")
        initialized = body.index("level.ghc_campaign_event_lock_initialized")
        completion_write = body.index("level.ghc_campaign_event_lock_complete[event_id] = 1")
        self.assertLess(guard, initialized)
        self.assertLess(guard, completion_write)
        self.assertIn("completed != 0 && completed != 1", body)
        self.assertLess(
            body.index("return 0", guard),
            completion_write,
        )

    def test_abort_releases_owner_without_marking_event_complete(self):
        abort = function_body(self.source, "ghc_campaign_event_lock_abort_slot")
        self.assertIn("level.ghc_campaign_event_lock_owner[var_0] != slot", abort)
        self.assertIn("level.ghc_campaign_event_lock_owner[var_0] = -1", abort)
        self.assertIn("level.ghc_campaign_event_lock_active[slot] = -1", abort)
        self.assertNotIn("event_lock_complete", abort.lower())
        self.assertNotIn("session_ready", abort.lower())

    def test_different_events_can_be_owned_by_different_players(self):
        acquire = function_body(self.source, "ghc_campaign_event_lock_acquire")
        self.assertIn("owner[event_id]", acquire)
        self.assertIn("active[var_0]", acquire)
        self.assertNotIn("global_active_owner", acquire.lower())
        self.assertNotIn("level.ghc_campaign_event_lock_active[1] != -1", acquire)

    def test_reset_is_blocked_while_event_is_owned(self):
        body = function_body(self.source, "ghc_campaign_event_lock_reset")
        self.assertIn("level.ghc_campaign_event_lock_owner[event_id] != -1", body)
        self.assertIn("return 0", body)
        self.assertIn("level.ghc_campaign_event_lock_complete[event_id] = 0", body)

    def test_atomic_lock_operations_do_not_wait_or_yield(self):
        for name in (
            "ghc_campaign_event_lock_acquire",
            "ghc_campaign_event_lock_release",
            "ghc_campaign_event_lock_abort_slot",
            "ghc_campaign_event_lock_reset",
        ):
            with self.subTest(function=name):
                self.assertNotRegex(function_body(self.source, name).lower(), r"\bwait\b")

    def test_event_ids_are_exact_discrete_values(self):
        body = function_body(self.source, "ghc_campaign_event_lock_event_id_is_valid")
        self.assertIn("!isdefined( event_id )", body)
        for event_id in range(16):
            self.assertIn(f"event_id == {event_id}", body)
        self.assertNotIn("event_id < 0", body)
        self.assertNotIn("event_id > 15", body)

    def test_state_validator_checks_array_shapes_enums_and_owner_active_consistency(self):
        body = function_body(self.source, "ghc_campaign_event_lock_state_is_valid")
        self.assertIn("owner.size != 16", body)
        self.assertIn("active.size != 2", body)
        self.assertIn("complete.size != 16", body)
        self.assertIn("owner[var_0] != -1", body)
        self.assertIn("owner[var_0] != 0", body)
        self.assertIn("owner[var_0] != 1", body)
        self.assertIn("complete[var_0] != 0", body)
        self.assertIn("complete[var_0] != 1", body)
        self.assertIn("active[0] != var_0", body)
        self.assertIn("active[1] != var_0", body)
        self.assertIn("owner[var_2] != var_1", body)
        self.assertIn("complete[var_0]", body)

    def test_initialized_corrupt_event_state_is_not_silently_reset(self):
        init = function_body(self.source, "ghc_campaign_event_lock_init")
        self.assertIn("return ghc_campaign_event_lock_state_is_valid()", init)
        self.assertLess(
            init.index("return ghc_campaign_event_lock_state_is_valid()"),
            init.index("level.ghc_campaign_event_lock_owner = []"),
        )
        self.assertIn("ghc_campaign_event_lock_state_is_valid()", self.source)

    def test_invalid_event_queries_use_distinct_sentinels(self):
        owner = function_body(self.source, "ghc_campaign_event_lock_owner")
        complete = function_body(self.source, "ghc_campaign_event_lock_is_complete")
        self.assertIn("return -2", owner)
        self.assertIn("return -1", complete)
        self.assertIn("ghc_campaign_event_lock_state_is_valid()", owner)
        self.assertIn("ghc_campaign_event_lock_state_is_valid()", complete)

    def test_abort_and_reset_fail_closed_on_corrupt_state_or_invalid_ids(self):
        abort = function_body(self.source, "ghc_campaign_event_lock_abort_slot")
        reset = function_body(self.source, "ghc_campaign_event_lock_reset")
        for body in (abort, reset):
            self.assertIn("ghc_campaign_event_lock_state_is_valid()", body)
        self.assertIn("ghc_campaign_event_lock_slot_is_valid( slot )", abort)
        self.assertIn("ghc_campaign_event_lock_event_id_is_valid( event_id )", reset)
        self.assertIn("active[0] == event_id", reset)
        self.assertIn("active[1] == event_id", reset)

    def test_native_pair_binding_fails_closed_if_event_lock_storage_is_invalid(self):
        bind = function_body(self.session, "ghc_campaign_bind_native_local_pair")
        self.assertIn("if ( !ghc_campaign_event_lock_init() )", bind)
        self.assertIn("ghc_campaign_clear_session_aliases()", bind)
        self.assertIn("level.ghc_campaign_session_status = 9", bind)
        self.assertLess(
            bind.index("if ( !ghc_campaign_event_lock_init() )"),
            bind.index("level.ghc_campaign_session_ready = 1"),
        )

    def test_clear_active_fails_closed_and_revalidates_after_abort(self):
        body = function_body(self.source, "ghc_campaign_event_lock_clear_active")
        self.assertIn("if ( !ghc_campaign_event_lock_state_is_valid() )", body)
        self.assertLess(
            body.index("ghc_campaign_event_lock_state_is_valid()"),
            body.index("ghc_campaign_event_lock_abort_slot( 0 )"),
        )
        self.assertIn("return ghc_campaign_event_lock_state_is_valid()", body)

    def test_release_checks_native_pair_before_rejecting_corrupt_lock_arrays(self):
        body = function_body(self.source, "ghc_campaign_event_lock_release")
        self.assertLess(
            body.index("ghc_campaign_native_pair_is_current()"),
            body.index("ghc_campaign_event_lock_state_is_valid()"),
        )
        self.assertIn("ghc_campaign_invalidate_local_session()", body)

    def test_owner_query_requires_a_ready_current_native_pair(self):
        body = function_body(self.source, "ghc_campaign_event_lock_is_owner")
        self.assertIn("level.ghc_campaign_session_ready", body)
        self.assertIn("ghc_campaign_native_pair_is_current()", body)
        self.assertIn("ghc_campaign_event_lock_state_is_valid()", body)
        self.assertLess(
            body.index("ghc_campaign_native_pair_is_current()"),
            body.index("ghc_campaign_event_lock_state_is_valid()"),
        )

    def test_event_lock_never_creates_players_or_polls_controller_input(self):
        source = self.source.lower()
        for token in (
            "spawnplayer(", "cloneplayer(", "spawntestclient(",
            "addtestclient(", "cellpadgetdata(", "cellpadinit(",
            "cellpadsetportsetting(", "setmem.ps3mapi",
        ):
            with self.subTest(token=token):
                self.assertNotIn(token, source)

    def test_session_bridge_initializes_and_cleans_event_locks(self):
        bind = function_body(self.session, "ghc_campaign_bind_native_local_pair")
        main = function_body(self.session, "main")
        invalidate = function_body(self.session, "ghc_campaign_invalidate_local_session")
        self.assertIn("ghc_campaign_event_lock_init()", bind)
        self.assertLess(
            bind.index("ghc_campaign_event_lock_init()"),
            bind.index("level.ghc_campaign_session_ready = 1"),
        )
        self.assertIn("ghc_campaign_event_lock_clear_active()", main)
        self.assertIn("ghc_campaign_event_lock_clear_active()", invalidate)
        self.assertLess(
            invalidate.index("ghc_campaign_event_lock_clear_active()"),
            invalidate.index("ghc_campaign_clear_session_aliases()"),
        )

    def test_manifest_pins_event_lock_source_and_documents_runtime_boundary(self):
        manifest = json.loads(EVENT_MANIFEST.read_text(encoding="utf-8"))
        source_hash = hashlib.sha256(EVENT_LOCK.read_bytes()).hexdigest()
        if manifest.get("source_sha256"):
            self.assertRegex(manifest["source_sha256"], r"^[0-9a-f]{64}$")
            self.assertEqual(source_hash, manifest["source_sha256"])
        self.assertEqual(manifest["compiler"], "GSC Tool 1.6.2.364")
        self.assertIn("never spawns or clones a player", manifest["behavior"])
        self.assertIn("reusable 16-ID shared mission event lock", manifest["behavior"])
        if manifest.get("compiled_gscbin_sha256"):
            self.assertRegex(manifest["compiled_gscbin_sha256"], r"^[0-9a-f]{64}$")


if __name__ == "__main__":
    unittest.main()
