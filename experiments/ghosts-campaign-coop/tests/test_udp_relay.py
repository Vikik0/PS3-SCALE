import socket
import time
import unittest

from server.protocol import decode_server_packet, encode_client_packet
from server.room_manager import RoomManager
from server.udp_relay import UdpRelay


class UdpRelayIntegrationTests(unittest.TestCase):
    def test_two_guest_peers_exchange_authenticated_udp_payloads(self):
        manager = RoomManager()
        host = manager.create_session("guest")
        guest = manager.create_session("guest")
        room = manager.create_room(host["token"])
        manager.join_room(guest["token"], room["room_code"])
        manager.set_ready(host["token"], True)
        manager.set_ready(guest["token"], True)
        manager.start_room(host["token"])

        host_id = bytes.fromhex(host["relay"]["relay_id"])
        host_key = bytes.fromhex(host["relay"]["relay_key"])
        guest_id = bytes.fromhex(guest["relay"]["relay_id"])
        guest_key = bytes.fromhex(guest["relay"]["relay_key"])

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
            # An invalid MAC must not register a peer endpoint or reach the other client.
            forged = encode_client_packet(host_id, b"x" * 32, 1, b"forged")
            host_client.sendto(forged, ("127.0.0.1", relay_port))
            time.sleep(0.05)

            host_client.sendto(
                encode_client_packet(host_id, host_key, 1, b"host-state"),
                ("127.0.0.1", relay_port),
            )
            guest_client.sendto(
                encode_client_packet(guest_id, guest_key, 1, b"guest-state"),
                ("127.0.0.1", relay_port),
            )
            guest_packet, _ = host_client.recvfrom(1200)
            self.assertEqual(
                decode_server_packet(guest_packet, host_key),
                (2, 1, b"guest-state"),
            )

            host_client.sendto(
                encode_client_packet(host_id, host_key, 2, b"host-next"),
                ("127.0.0.1", relay_port),
            )
            host_packet, _ = guest_client.recvfrom(1200)
            self.assertEqual(
                decode_server_packet(host_packet, guest_key),
                (1, 2, b"host-next"),
            )
        finally:
            relay.stop()
            relay.join(timeout=2)
            host_client.close()
            guest_client.close()


if __name__ == "__main__":
    unittest.main()
