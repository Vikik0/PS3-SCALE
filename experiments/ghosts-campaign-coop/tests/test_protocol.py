import unittest

from server.protocol import (
    MAGIC,
    decode_client_packet,
    decode_server_packet,
    encode_client_packet,
    encode_server_packet,
)


class ProtocolTests(unittest.TestCase):
    def setUp(self):
        self.relay_id = bytes(range(16))
        self.relay_key = b"k" * 32

    def test_authenticated_client_packet_round_trip(self):
        payload = b"player-state"
        packet = encode_client_packet(self.relay_id, self.relay_key, 513, payload)
        decoded = decode_client_packet(packet, self.relay_key)
        self.assertEqual(decoded, (self.relay_id, 513, payload))
        self.assertEqual(packet[:4], MAGIC)

    def test_session_bearer_token_is_not_in_udp_packet(self):
        token = b"0123456789abcdef0123456789abcdef"
        packet = encode_client_packet(self.relay_id, self.relay_key, 1, token)
        self.assertNotIn(token, packet[:24])

    def test_tampered_client_payload_is_rejected(self):
        packet = bytearray(encode_client_packet(self.relay_id, self.relay_key, 1, b"state"))
        packet[25] ^= 0x01
        self.assertIsNone(decode_client_packet(bytes(packet), self.relay_key))

    def test_wrong_client_key_is_rejected(self):
        packet = encode_client_packet(self.relay_id, self.relay_key, 1, b"state")
        self.assertIsNone(decode_client_packet(packet, b"z" * 32))

    def test_invalid_magic_and_oversize_are_rejected(self):
        self.assertIsNone(decode_client_packet(b"BAD!" + b"0" * 80, self.relay_key))
        packet = encode_client_packet(self.relay_id, self.relay_key, 1, b"x")
        self.assertIsNone(decode_client_packet(packet + b"x" * 1200, self.relay_key))

    def test_authenticated_server_packet_round_trip(self):
        packet = encode_server_packet(2, 42, b"peer-state", self.relay_key)
        self.assertEqual(
            decode_server_packet(packet, self.relay_key),
            (2, 42, b"peer-state"),
        )

    def test_tampered_server_packet_is_rejected(self):
        packet = bytearray(encode_server_packet(1, 3, b"state", self.relay_key))
        packet[8] ^= 0x01
        self.assertIsNone(decode_server_packet(bytes(packet), self.relay_key))

    def test_invalid_sender_slot_is_rejected(self):
        packet = bytearray(encode_server_packet(1, 3, b"state", self.relay_key))
        packet[4] = 9
        from server.protocol import _mac
        packet[-16:] = _mac(self.relay_key, bytes(packet[:-16]))
        self.assertIsNone(decode_server_packet(bytes(packet), self.relay_key))


if __name__ == "__main__":
    unittest.main()
