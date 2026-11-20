import unittest

from server.room_manager import CoopError, RoomManager


class RoomManagerTests(unittest.TestCase):
    def setUp(self):
        self.manager = RoomManager()
        self.host = self.manager.create_session("guest")
        self.guest = self.manager.create_session("guest")

    def test_guest_session_and_psn_is_explicitly_unconfigured(self):
        self.assertEqual(self.host["mode"], "guest")
        with self.assertRaises(CoopError) as caught:
            self.manager.create_session("psn")
        self.assertEqual(caught.exception.code, "PSN_AUTH_NOT_IMPLEMENTED")
        self.assertEqual(caught.exception.status, 501)

    def test_two_players_can_ready_and_host_can_start(self):
        room = self.manager.create_room(self.host["token"])
        code = room["room_code"]
        joined = self.manager.join_room(self.guest["token"], code)
        self.assertEqual(joined["player_count"], 2)
        self.manager.set_ready(self.host["token"], True)
        self.manager.set_ready(self.guest["token"], True)
        started = self.manager.start_room(self.host["token"])
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
        self.manager.join_room(self.guest["token"], room["room_code"])
        self.manager.set_ready(self.host["token"], True)
        self.manager.set_ready(self.guest["token"], True)
        with self.assertRaises(CoopError) as caught:
            self.manager.start_room(self.guest["token"])
        self.assertEqual(caught.exception.code, "HOST_ONLY")

    def test_start_requires_both_players_ready(self):
        room = self.manager.create_room(self.host["token"])
        self.manager.join_room(self.guest["token"], room["room_code"])
        self.manager.set_ready(self.host["token"], True)
        with self.assertRaises(CoopError) as caught:
            self.manager.start_room(self.host["token"])
        self.assertEqual(caught.exception.code, "PLAYERS_NOT_READY")


if __name__ == "__main__":
    unittest.main()
