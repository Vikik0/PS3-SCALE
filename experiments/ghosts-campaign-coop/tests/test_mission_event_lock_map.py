import copy
import json
import unittest
from pathlib import Path

from tools.verify_mission_event_lock_map import validate_map


ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / "reports" / "PHASE88_MISSION_EVENT_LOCK_MAP.json"
GSC_PATH = ROOT / "client" / "ps3" / "gsc" / "ghc_campaign_local_session.gsc"


class MissionEventLockMapTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.map_data = json.loads(MAP_PATH.read_text(encoding="utf-8"))
        cls.gsc_source = GSC_PATH.read_text(encoding="utf-8")

    def test_current_nine_event_map_passes(self):
        self.assertEqual(validate_map(self.map_data, self.gsc_source), [])

    def test_duplicate_event_id_is_rejected(self):
        data = copy.deepcopy(self.map_data)
        data["events"][4]["event_lock_id"] = data["events"][2]["event_lock_id"]
        errors = validate_map(data, self.gsc_source)
        self.assertTrue(any("event_lock_id 0 reused" in error for error in errors))

    def test_out_of_range_event_id_is_rejected(self):
        data = copy.deepcopy(self.map_data)
        data["events"][2]["event_lock_id"] = 16
        errors = validate_map(data, self.gsc_source)
        self.assertTrue(any("event_lock_id outside configured range" in error for error in errors))

    def test_mission_coordinator_must_not_claim_player_lock(self):
        data = copy.deepcopy(self.map_data)
        data["events"][0]["event_lock_id"] = 7
        errors = validate_map(data, self.gsc_source)
        self.assertTrue(any("mission coordinator policy must not claim a player event lock" in error for error in errors))

    def test_shared_interaction_requires_lock_id_and_owner_policy(self):
        data = copy.deepcopy(self.map_data)
        data["events"][2]["event_lock_id"] = None
        data["events"][2]["owner"] = "mission_coordinator"
        errors = validate_map(data, self.gsc_source)
        self.assertTrue(any("shared interaction requires an integer event_lock_id" in error for error in errors))
        self.assertTrue(any("shared interaction must use first_valid_player_to_acquire" in error for error in errors))

    def test_missing_critical_mission_routine_is_rejected(self):
        data = copy.deepcopy(self.map_data)
        data["events"] = data["events"][:-1]
        errors = validate_map(data, self.gsc_source)
        self.assertTrue(any("missing mission routines" in error for error in errors))

    def test_wrong_policy_is_rejected(self):
        data = copy.deepcopy(self.map_data)
        data["events"][7]["policy"] = "SHARED_INTERACTION_SINGLE_OWNER"
        errors = validate_map(data, self.gsc_source)
        self.assertTrue(any("expected policy MISSION_COORDINATOR_ONLY" in error for error in errors))

    def test_missing_lock_api_function_is_rejected(self):
        source = self.gsc_source.replace("ghc_campaign_event_lock_acquire", "removed_event_lock_acquire")
        errors = validate_map(self.map_data, source)
        self.assertTrue(any("Local Session GSC API missing ghc_campaign_event_lock_acquire" in error for error in errors))

    def test_duplicate_mission_routine_is_rejected(self):
        data = copy.deepcopy(self.map_data)
        data["events"].append(copy.deepcopy(data["events"][0]))
        errors = validate_map(data, self.gsc_source)
        self.assertTrue(any("duplicate mission routine" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
