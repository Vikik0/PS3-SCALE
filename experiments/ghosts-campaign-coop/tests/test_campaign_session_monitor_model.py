import re
import unittest
from pathlib import Path


PROJECT = Path(__file__).resolve().parents[1]
GSC = PROJECT / "client" / "ps3" / "gsc" / "ghc_campaign_local_session.gsc"


def function_body(source: str, name: str) -> str:
    match = re.search(rf"(?m)^[ \t]*{re.escape(name)}\s*\([^)]*\)\s*\{{", source)
    if match is None:
        raise AssertionError(f"function not found: {name}")
    opening = source.index("{", match.start())
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"unterminated function: {name}")


class CampaignSessionMonitorModelContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = GSC.read_text(encoding="utf-8")

    def test_gsc_watcher_uses_eight_failures_and_quarter_second_poll(self):
        body = function_body(self.source, "ghc_campaign_watch_native_session")
        self.assertIn("var_0 >= 8", body)
        self.assertIn("wait 0.25", body)
        self.assertIn("ghc_campaign_invalidate_local_session()", body)

    def test_gsc_rebind_uses_fresh_engine_array_and_strict_native_pair_binder(self):
        body = function_body(self.source, "ghc_campaign_watch_native_session")
        query = body.index('getentarray( "player", "classname" )')
        refresh = body.index("level.playerpiggyback = var_1", query)
        bind = body.index("ghc_campaign_bind_native_local_pair()", query)
        self.assertLess(query, refresh)
        self.assertLess(refresh, bind)
        self.assertNotIn("spawnplayer(", body.lower())
        self.assertNotIn("cloneplayer(", body.lower())

    def test_gsc_monitor_has_explicit_stop_and_main_single_owner_guard(self):
        start = function_body(self.source, "ghc_campaign_start_session_monitor")
        stop = function_body(self.source, "ghc_campaign_stop_session_monitor")
        watch = function_body(self.source, "ghc_campaign_watch_native_session")
        main = function_body(self.source, "main")
        self.assertIn("level.ghc_campaign_session_monitor_stop = 0", start)
        self.assertIn("level.ghc_campaign_session_monitor_stop = 1", stop)
        self.assertIn("level.ghc_campaign_session_monitor_stop = undefined", watch)
        guard = main.index("level.ghc_campaign_session_monitor_active")
        clear = main.index("level.ghc_campaign_player_0 = undefined")
        self.assertLess(guard, clear)

    def test_main_waits_for_pending_stop_to_exit_before_alias_cleanup(self):
        main = function_body(self.source, "main")
        active = main.index("level.ghc_campaign_session_monitor_active")
        stop_check = main.index("level.ghc_campaign_session_monitor_stop", active)
        wait_loop = main.index("var_0 < 40", stop_check)
        wait = main.index("wait 0.05", wait_loop)
        timeout = main.index("return 0", wait)
        clear = main.index("level.ghc_campaign_player_0 = undefined")
        self.assertLess(active, stop_check)
        self.assertLess(stop_check, wait_loop)
        self.assertLess(wait_loop, wait)
        self.assertLess(wait, timeout)
        self.assertLess(timeout, clear)
        self.assertNotIn("level.ghc_campaign_session_monitor_stop = 0", main[:clear])

    def test_start_monitor_refuses_a_second_owner_while_stop_is_pending(self):
        start = function_body(self.source, "ghc_campaign_start_session_monitor")
        active = start.index("level.ghc_campaign_session_monitor_active")
        stop_check = start.index("level.ghc_campaign_session_monitor_stop", active)
        refuse = start.index("return 0", stop_check)
        thread = start.index("thread ghc_campaign_watch_native_session()")
        self.assertLess(active, stop_check)
        self.assertLess(stop_check, refuse)
        self.assertLess(refuse, thread)

    def test_watcher_rebinds_automatically_after_invalidation(self):
        watch = function_body(self.source, "ghc_campaign_watch_native_session")
        query = watch.index('getentarray( "player", "classname" )')
        refresh = watch.index("level.playerpiggyback = var_1", query)
        bind = watch.index("ghc_campaign_bind_native_local_pair()", query)
        self.assertLess(query, refresh)
        self.assertLess(refresh, bind)
        self.assertNotIn("main() must be called again", watch)

    def test_explicit_stop_invalidates_ready_session_before_monitor_exit(self):
        watch = function_body(self.source, "ghc_campaign_watch_native_session")
        shutdown = watch.index("Explicit shutdown is fail-closed.")
        invalidate = watch.index("ghc_campaign_invalidate_local_session()", shutdown)
        monitor_exit = watch.index("level.ghc_campaign_session_monitor_active = undefined")
        self.assertLess(shutdown, invalidate)
        self.assertLess(invalidate, monitor_exit)


if __name__ == "__main__":
    unittest.main()
