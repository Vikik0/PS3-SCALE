import json
import threading
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from server.http_api import make_server
from server.room_manager import RoomManager


class HttpApiTests(unittest.TestCase):
    def setUp(self):
        self.manager = RoomManager()
        self.server = make_server(self.manager, host="127.0.0.1", port=0)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base = f"http://127.0.0.1:{self.server.server_port}"

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=2)

    def request(self, path, method="GET", payload=None, token=None):
        data = None if payload is None else json.dumps(payload).encode("utf-8")
        headers = {"Content-Type": "application/json"}
        if token:
            headers["Authorization"] = f"Bearer {token}"
        req = Request(self.base + path, data=data, headers=headers, method=method)
        try:
            with urlopen(req, timeout=2) as response:
                return response.status, json.loads(response.read().decode("utf-8"))
        except HTTPError as exc:
            return exc.code, json.loads(exc.read().decode("utf-8"))

    def test_health_and_guest_lobby_flow(self):
        status, health = self.request("/health")
        self.assertEqual(status, 200)
        self.assertTrue(health["ok"])

        status, host = self.request("/v1/sessions", "POST", {"mode": "guest"})
        self.assertEqual(status, 201)
        self.assertIn("relay", host)

        status, guest = self.request("/v1/sessions", "POST", {"mode": "guest"})
        self.assertEqual(status, 201)

        status, room = self.request("/v1/rooms", "POST", {}, host["token"])
        self.assertEqual(status, 201)
        code = room["room_code"]

        status, joined = self.request(
            f"/v1/rooms/{code}/join", "POST", {}, guest["token"]
        )
        self.assertEqual(status, 200)
        self.assertEqual(joined["player_count"], 2)

        status, _ = self.request(
            f"/v1/rooms/{code}/ready", "POST", {"ready": True}, host["token"]
        )
        self.assertEqual(status, 200)
        status, _ = self.request(
            f"/v1/rooms/{code}/ready", "POST", {"ready": True}, guest["token"]
        )
        self.assertEqual(status, 200)
        status, started = self.request(
            f"/v1/rooms/{code}/start", "POST", {}, host["token"]
        )
        self.assertEqual(status, 200)
        self.assertEqual(started["status"], "running")

    def test_mutation_routes_cannot_target_another_room(self):
        _, host = self.request("/v1/sessions", "POST", {"mode": "guest"})
        _, room = self.request("/v1/rooms", "POST", {}, host["token"])
        code = room["room_code"]
        wrong = ("A" if code[0] != "A" else "B") + code[1:]

        status, error = self.request(
            f"/v1/rooms/{wrong}/ready", "POST", {"ready": True}, host["token"]
        )
        self.assertEqual(status, 404)
        self.assertEqual(error["error"], "ROOM_NOT_FOUND")

        status, error = self.request(
            f"/v1/rooms/{wrong}/leave", "POST", {}, host["token"]
        )
        self.assertEqual(status, 404)
        self.assertEqual(error["error"], "ROOM_NOT_FOUND")

        status, current = self.request(f"/v1/rooms/{code}", token=host["token"])
        self.assertEqual(status, 200)
        self.assertEqual(current["player_count"], 1)


if __name__ == "__main__":
    unittest.main()
