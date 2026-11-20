import unittest

from server.protocol import MAGIC, decode_client_packet, encode_server_packet


class ProtocolTests(unittest.TestCase):
    def test_client_packet_round_trip(self):
        token = "a1" * 16
        payload = b"player-state"
        packet = MAGIC + token.encode("ascii") + (513).to_bytes(2, "big") + payload
        decoded = decode_client_packet(packet)
        self.assertEqual(decoded, (token, 513, payload))

    def test_invalid_magic_is_rejected(self):
        self.assertIsNone(decode_client_packet(b"BAD!" + b"0" * 34))

    def test_server_packet_header(self):
        packet = encode_server_packet(2, 42, b"x")
        self.assertEqual(packet[:4], MAGIC)
        self.assertEqual(packet[4], 2)
        self.assertEqual(int.from_bytes(packet[5:7], "big"), 42)
        self.assertEqual(packet[7:], b"x")


if __name__ == "__main__":
    unittest.main()
