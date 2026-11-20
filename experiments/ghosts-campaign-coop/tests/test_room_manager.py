import unittest

from server.room_manager import CoopError, RoomManager


class RoomManagerTests(unittest.TestCase):
    def setUp(self):
        self.manager = RoomManager()
        self.host = self.manager.create_session("guest")
        self.guest = self.manager.create_session("guest")

    def test_guest_session_and_psn_is_explicitly_unconfigured(self):
        self.assertEqual(self.host["mode"], "guest")
        self.assertEqual(self.host["relay"]["protocol"], "GHC2")
        self.assertEqual(len(bytes.fromhex(self.host["relay"]["relay_id"])), 16)
        self.assertEqual(len(bytes.fromhex(self.host["relay"]["relay_key"])), 32)
        with self.assertRaises(CoopError) as caught:
            self.manager.create_session("psn")
        self.assertEqual(caught.exception.code, "PSN_AUTH_NOT_IMPLEMENTED")
        self.assertEqual(caught.exception.status, 501)

    def test_two_players_can_ready_and_host_can_start(self):
        room = self.manager.create_room(self.host["token"])
        code = room["room_code"]
        joined = self.manager.join_room(self.guest["token"], code)
        self.assertEqual(joined["player_count"], 2)
        self.manager.set_ready(self.host["token"], True, code)
        self.manager.set_ready(self.guest["token"], True, code)
        started = self.manager.start_room(self.host["token"], code)
        self.assertEqual(started["status"], "running")

    def test_room_rejects_third_player(self):
        third = self.manager.create_session("guest")
        room = self.manager.create_room(self.host["token"])
        self.manager.join_room(self.guest["token"], room["room_code"])
        with self.assertRaises(CoopError) as caught:
            self.manager.join_room(third["token"], room["room_code"])
        self.assertEqual(caught.exception.code, "ROOM_FULL")

    def test_non_host_cannot_start(self):
        room = self.manager.create_room(self.host["token"])
        code = room["room_code"]
        self.manager.join_room(self.guest["token"], code)
        self.manager.set_ready(self.host["token"], True)
        self.manager.set_ready(self.guest["token"], True)
        with self.assertRaises(CoopError) as caught:
            self.manager.start_room(self.guest["token"], code)
        self.assertEqual(caught.exception.code, "HOST_ONLY")

    def test_start_requires_both_players_ready(self):
        room = self.manager.create_room(self.host["token"])
        code = room["room_code"]
        self.manager.join_room(self.guest["token"], code)
        self.manager.set_ready(self.host["token"], True)
        with self.assertRaises(CoopError) as caught:
            self.manager.start_room(self.host["token"], code)
        self.assertEqual(caught.exception.code, "PLAYERS_NOT_READY")

    def test_mutations_reject_a_different_room_code(self):
        room = self.manager.create_room(self.host["token"])
        code = room["room_code"]
        wrong = ("A" if code[0] != "A" else "B") + code[1:]
        with self.assertRaises(CoopError) as caught:
            self.manager.set_ready(self.host["token"], True, wrong)
        self.assertEqual(caught.exception.code, "ROOM_NOT_FOUND")
        with self.assertRaises(CoopError) as caught:
            self.manager.leave_room(self.host["token"], wrong)
        self.assertEqual(caught.exception.code, "ROOM_NOT_FOUND")
        self.assertEqual(self.manager.get_room(self.host["token"], code)["player_count"], 1)

    def test_udp_replay_window_accepts_reordering_but_rejects_duplicates(self):
        room = self.manager.create_room(self.host["token"])
        code = room["room_code"]
        self.manager.join_room(self.guest["token"], code)
        self.manager.set_ready(self.host["token"], True)
        self.manager.set_ready(self.guest["token"], True)
        self.manager.start_room(self.host["token"])
        host_token = self.host["token"]
        self.assertIsNone(self.manager.resolve_udp(host_token, ("127.0.0.1", 1000), 10))
        self.assertIsNone(self.manager.resolve_udp(host_token, ("127.0.0.1", 1000), 10))
        self.assertIsNone(self.manager.resolve_udp(host_token, ("127.0.0.1", 1000), 9))
        result = self.manager.resolve_udp(host_token, ("127.0.0.1", 1000), 11)
        self.assertIsNotNone(result)
        self.assertEqual(result[1], 1)

    def test_udp_sequence_wraparound_is_supported(self):
        session = self.manager.sessions[self.host["token"]]
        session.udp_highest_sequence = 0xFFFFFFFE
        session.udp_sequence_window = 1
        self.assertTrue(self.manager._accept_sequence(session, 0xFFFFFFFF))
        self.assertTrue(self.manager._accept_sequence(session, 0))
        self.assertFalse(self.manager._accept_sequence(session, 0xFFFFFFFF))


if __name__ == "__main__":
    unittest.main()
