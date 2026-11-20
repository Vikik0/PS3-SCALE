import socket
import time
import unittest

from server.protocol import MAGIC
from server.room_manager import RoomManager
from server.udp_relay import UdpRelay


def make_packet(token: str, sequence: int, payload: bytes) -> bytes:
    return MAGIC + token.encode("ascii") + sequence.to_bytes(2, "big") + payload


class UdpRelayIntegrationTests(unittest.TestCase):
    def test_two_guest_peers_exchange_udp_payloads(self):
        manager = RoomManager()
        host = manager.create_session("guest")
        guest = manager.create_session("guest")
        room = manager.create_room(host["token"])
        manager.join_room(guest["token"], room["room_code"])
        manager.set_ready(host["token"], True)
        manager.set_ready(guest["token"], True)
        manager.start_room(host["token"])

        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.bind(("127.0.0.1", 0))
            relay_port = probe.getsockname()[1]

        relay = UdpRelay(manager, host="127.0.0.1", port=relay_port)
        host_client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        guest_client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        host_client.settimeout(2.0)
        guest_client.settimeout(2.0)
        relay.start()
        time.sleep(0.05)
        try:
            host_client.sendto(make_packet(host["token"], 1, b"host-state"), ("127.0.0.1", relay_port))
            guest_client.sendto(make_packet(guest["token"], 2, b"guest-state"), ("127.0.0.1", relay_port))
            guest_packet, _ = host_client.recvfrom(1200)
            self.assertEqual(guest_packet[:4], MAGIC)
            self.assertEqual(guest_packet[4], 2)
            self.assertEqual(int.from_bytes(guest_packet[5:7], "big"), 2)
            self.assertEqual(guest_packet[7:], b"guest-state")

            host_client.sendto(make_packet(host["token"], 3, b"host-next"), ("127.0.0.1", relay_port))
            host_packet, _ = guest_client.recvfrom(1200)
            self.assertEqual(host_packet[:4], MAGIC)
            self.assertEqual(host_packet[4], 1)
            self.assertEqual(int.from_bytes(host_packet[5:7], "big"), 3)
            self.assertEqual(host_packet[7:], b"host-next")
        finally:
            relay.stop()
            relay.join(timeout=2)
            host_client.close()
            guest_client.close()


if __name__ == "__main__":
    unittest.main()
