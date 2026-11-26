import copy
import unittest

from tools.ps3mapi.validate_campaign_player_pair_capture import validate_capture


def player(index: int, *, primary: bool) -> dict:
    entity = f"entity-{index}"
    base = 0x10000 + index * 0x1000
    return {
        "entity_id": entity,
        "local_client_index": index,
        "controller_port": index,
        "is_splitscreen_player": True,
        "is_primary": primary,
        "gentity_address": f"0x{base:08x}",
        "gclient_address": f"0x{base + 0x100:08x}",
        "player_state_address": f"0x{base + 0x200:08x}",
        "camera_state_address": f"0x{base + 0x300:08x}",
        "hud_state_address": f"0x{base + 0x400:08x}",
        "camera_owner_entity_id": entity,
        "hud_owner_entity_id": entity,
    }


def observation(timestamp: int, *, connected: bool = True) -> dict:
    players = [player(0, primary=True), player(1, primary=False)] if connected else [player(0, primary=True)]
    return {
        "timestamp_ms": timestamp,
        "split_screen": connected,
        "player_array_count": len(players),
        "players": players,
        "level_player_entity_id": "entity-0",
        "playengineeffects_entity_id": "entity-1" if connected else None,
    }


def valid_capture() -> dict:
    return {
        "schema_version": 1,
        "capture_id": "synthetic-test-only",
        "title_id": "BLUS31270",
        "title_update": "01.16",
        "mode": "CAMPAIGN",
        "evidence_source": "synthetic-unit-test",
        "observations": [observation(i * 250) for i in range(5)],
    }


class CampaignPlayerPairCaptureTests(unittest.TestCase):
    def test_five_stable_samples_prove_native_pair_contract(self):
        result = validate_capture(valid_capture())
        self.assertEqual(result["status"], "PASS_STRUCTURAL_CAPTURE_VALIDATION")
        self.assertTrue(result["stable_pair_observation_validated"])
        self.assertTrue(result["native_local_client_fields_consistent"])
        self.assertTrue(result["controller_to_player_mapping_fields_consistent"])
        self.assertTrue(result["independent_camera_hud_fields_consistent"])
        self.assertEqual(result["best_stable_run"], 5)
        self.assertFalse(result["explicit_pair_loss_observed"])
        self.assertFalse(result["auto_rebind_cycle_observed_in_supplied_data"])
        self.assertEqual(result["evidence_authenticity"], "NOT_AUTHENTICATED_BY_THIS_TOOL")
        self.assertEqual(result["evidence_source"], "synthetic-unit-test")

    def test_disconnect_then_five_stable_samples_proves_rebind_cycle(self):
        data = valid_capture()
        data["observations"] = [observation(0, connected=False)] + [
            observation(i * 250 + 250) for i in range(5)
        ]
        result = validate_capture(data)
        self.assertTrue(result["stable_pair_observation_validated"])
        self.assertTrue(result["explicit_pair_loss_observed"])
        self.assertEqual(result["reconnect_stable_run_after_pair_loss"], 5)
        self.assertTrue(result["auto_rebind_cycle_observed_in_supplied_data"])

    def test_structural_error_does_not_falsely_claim_disconnect_and_reconnect(self):
        data = valid_capture()
        malformed = observation(1250)
        malformed["players"][1]["camera_owner_entity_id"] = "wrong-owner"
        data["observations"] = (
            [observation(i * 250) for i in range(5)]
            + [malformed]
            + [observation(1500 + i * 250) for i in range(5)]
        )
        result = validate_capture(data)
        self.assertTrue(result["stable_pair_observation_validated"])
        self.assertFalse(result["explicit_pair_loss_observed"])
        self.assertFalse(result["auto_rebind_cycle_observed_in_supplied_data"])
        self.assertIn("camera_owner_mismatch", [x["reason"] for x in result["rejected_observations"]])

    def test_four_stable_samples_are_incomplete(self):
        data = valid_capture()
        data["observations"] = data["observations"][:4]
        result = validate_capture(data)
        self.assertEqual(result["status"], "INCOMPLETE_PAIR_OBSERVATION_DATA")
        self.assertFalse(result["stable_pair_observation_validated"])

    def test_unstable_entity_identity_does_not_accumulate_stable_samples(self):
        data = valid_capture()
        data["observations"][2]["players"][1]["entity_id"] = "entity-replaced"
        data["observations"][2]["players"][1]["camera_owner_entity_id"] = "entity-replaced"
        data["observations"][2]["players"][1]["hud_owner_entity_id"] = "entity-replaced"
        result = validate_capture(data)
        self.assertFalse(result["stable_pair_observation_validated"])

    def test_samples_farther_apart_than_limit_do_not_form_a_stable_run(self):
        data = valid_capture()
        data["observations"] = [observation(i * 1500) for i in range(5)]
        result = validate_capture(data)
        self.assertFalse(result["stable_pair_observation_validated"])
        self.assertEqual(result["best_stable_run"], 1)
        self.assertEqual(len(result["continuity_breaks"]), 4)
        self.assertEqual(result["max_sample_gap_ms"], 1000)

    def test_operator_can_raise_sample_gap_limit_explicitly(self):
        data = valid_capture()
        data["observations"] = [observation(i * 1500) for i in range(5)]
        result = validate_capture(data, max_sample_gap_ms=1500)
        self.assertTrue(result["stable_pair_observation_validated"])
        self.assertEqual(result["best_stable_run"], 5)
        self.assertEqual(result["continuity_breaks"], [])

    def test_controller_port_mapping_is_reported_without_assuming_port_order(self):
        data = valid_capture()
        for sample in data["observations"]:
            sample["players"][0]["controller_port"] = 1
            sample["players"][1]["controller_port"] = 0
        result = validate_capture(data)
        self.assertTrue(result["stable_pair_observation_validated"])
        self.assertEqual(
            result["best_pair_mapping"],
            [
                {
                    "local_client_index": 0,
                    "controller_port": 1,
                    "entity_id": "entity-0",
                    "role": "PRIMARY",
                },
                {
                    "local_client_index": 1,
                    "controller_port": 0,
                    "entity_id": "entity-1",
                    "role": "SECONDARY",
                },
            ],
        )

    def test_invalid_sample_gap_limit_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "max_sample_gap_ms"):
            validate_capture(valid_capture(), max_sample_gap_ms=0)

    def test_wrong_title_id_is_rejected(self):
        data = valid_capture()
        data["title_id"] = "BLUS00000"
        with self.assertRaisesRegex(ValueError, "title_id"):
            validate_capture(data)

    def test_wrong_title_update_is_rejected(self):
        data = valid_capture()
        data["title_update"] = "01.15"
        with self.assertRaisesRegex(ValueError, "title_update"):
            validate_capture(data)

    def test_multiplayer_mode_is_rejected(self):
        data = valid_capture()
        data["mode"] = "MULTIPLAYER"
        with self.assertRaisesRegex(ValueError, "CAMPAIGN"):
            validate_capture(data)

    def test_timestamps_must_be_strictly_increasing(self):
        data = valid_capture()
        data["observations"][3]["timestamp_ms"] = data["observations"][2]["timestamp_ms"]
        with self.assertRaisesRegex(ValueError, "strictly increasing"):
            validate_capture(data)

    def test_player_array_count_mismatch_is_rejected_as_invalid_sample(self):
        data = valid_capture()
        data["observations"][1]["player_array_count"] = 3
        result = validate_capture(data)
        self.assertFalse(result["stable_pair_observation_validated"])
        self.assertIn("player_array_count_mismatch", [x["reason"] for x in result["rejected_observations"]])
        self.assertFalse(result["explicit_pair_loss_observed"])

    def test_duplicate_local_client_index_is_rejected(self):
        data = valid_capture()
        data["observations"][0]["players"][1]["local_client_index"] = 0
        result = validate_capture(data)
        self.assertFalse(result["stable_pair_observation_validated"])

    def test_duplicate_controller_port_is_rejected(self):
        data = valid_capture()
        data["observations"][0]["players"][1]["controller_port"] = 0
        result = validate_capture(data)
        self.assertFalse(result["stable_pair_observation_validated"])

    def test_duplicate_player_state_pointer_is_rejected(self):
        data = valid_capture()
        data["observations"][0]["players"][1]["player_state_address"] = data["observations"][0]["players"][0]["player_state_address"]
        result = validate_capture(data)
        self.assertFalse(result["stable_pair_observation_validated"])

    def test_swapped_campaign_aliases_are_rejected(self):
        data = valid_capture()
        data["observations"][0]["level_player_entity_id"] = "entity-1"
        result = validate_capture(data)
        self.assertFalse(result["stable_pair_observation_validated"])

    def test_camera_owner_mismatch_is_rejected(self):
        data = valid_capture()
        data["observations"][0]["players"][0]["camera_owner_entity_id"] = "entity-1"
        result = validate_capture(data)
        self.assertFalse(result["stable_pair_observation_validated"])

    def test_non_boolean_flags_are_rejected(self):
        data = valid_capture()
        data["observations"][0]["players"][0]["is_primary"] = 1
        result = validate_capture(data)
        self.assertFalse(result["stable_pair_observation_validated"])

    def test_invalid_required_sample_count_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "required_stable_samples"):
            validate_capture(valid_capture(), 1)


if __name__ == "__main__":
    unittest.main()
