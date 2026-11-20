"""Small binary envelope for client-to-server UDP relay packets.

Client -> relay: MAGIC(4) | TOKEN_HEX(32 ASCII) | SEQUENCE(uint16 BE) | PAYLOAD
Relay -> peer:   MAGIC(4) | SENDER_SLOT(uint8) | SEQUENCE(uint16 BE) | PAYLOAD
The game-specific payload is opaque to this server.
"""
MAGIC = b"GHC1"
TOKEN_LENGTH = 32
CLIENT_HEADER_SIZE = 4 + TOKEN_LENGTH + 2
SERVER_HEADER_SIZE = 4 + 1 + 2
MAX_DATAGRAM = 1200


def decode_client_packet(packet: bytes):
    if len(packet) < CLIENT_HEADER_SIZE or len(packet) > MAX_DATAGRAM:
        return None
    if packet[:4] != MAGIC:
        return None
    try:
        token = packet[4:36].decode("ascii")
    except UnicodeDecodeError:
        return None
    if len(token) != TOKEN_LENGTH or any(ch not in "0123456789abcdef" for ch in token):
        return None
    sequence = int.from_bytes(packet[36:38], "big")
    return token, sequence, packet[38:]


def encode_server_packet(sender_slot: int, sequence: int, payload: bytes) -> bytes:
    if sender_slot not in (1, 2):
        raise ValueError("sender_slot must be 1 or 2")
    if not 0 <= sequence <= 65535:
        raise ValueError("sequence must fit uint16")
    packet = MAGIC + bytes([sender_slot]) + sequence.to_bytes(2, "big") + payload
    if len(packet) > MAX_DATAGRAM:
        raise ValueError("datagram exceeds safe MTU budget")
    return packet
