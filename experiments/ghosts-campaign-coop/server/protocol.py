"""Authenticated compact UDP envelope for the two-player relay.

Client -> relay:
  MAGIC(4) | RELAY_ID(16 raw bytes) | SEQUENCE(uint32 BE) | PAYLOAD | HMAC-SHA256/16
Relay -> client:
  MAGIC(4) | SENDER_SLOT(uint8) | SEQUENCE(uint32 BE) | PAYLOAD | HMAC-SHA256/16

The bearer session token is never sent over UDP. The relay key is issued once by
the HTTPS session endpoint and is used only for message authentication.
"""
from __future__ import annotations

import hashlib
import hmac

MAGIC = b"GHC2"
RELAY_ID_SIZE = 16
MAC_SIZE = 16
SEQUENCE_SIZE = 4
CLIENT_HEADER_SIZE = 4 + RELAY_ID_SIZE + SEQUENCE_SIZE
SERVER_HEADER_SIZE = 4 + 1 + SEQUENCE_SIZE
CLIENT_OVERHEAD = CLIENT_HEADER_SIZE + MAC_SIZE
SERVER_OVERHEAD = SERVER_HEADER_SIZE + MAC_SIZE
MAX_DATAGRAM = 1200


def _mac(key: bytes, data: bytes) -> bytes:
    return hmac.new(key, data, hashlib.sha256).digest()[:MAC_SIZE]


def encode_client_packet(relay_id: bytes, relay_key: bytes, sequence: int, payload: bytes) -> bytes:
    if len(relay_id) != RELAY_ID_SIZE or len(relay_key) != 32:
        raise ValueError("relay_id must be 16 bytes and relay_key must be 32 bytes")
    if not 0 <= sequence <= 0xFFFFFFFF:
        raise ValueError("sequence must fit uint32")
    header = MAGIC + relay_id + sequence.to_bytes(SEQUENCE_SIZE, "big")
    packet = header + payload
    packet += _mac(relay_key, packet)
    if len(packet) > MAX_DATAGRAM:
        raise ValueError("datagram exceeds safe MTU budget")
    return packet


def decode_client_packet(packet: bytes, relay_key: bytes):
    if len(packet) < CLIENT_OVERHEAD or len(packet) > MAX_DATAGRAM:
        return None
    if packet[:4] != MAGIC or len(relay_key) != 32:
        return None
    signed, supplied_mac = packet[:-MAC_SIZE], packet[-MAC_SIZE:]
    if not hmac.compare_digest(_mac(relay_key, signed), supplied_mac):
        return None
    relay_id = packet[4:4 + RELAY_ID_SIZE]
    sequence_start = 4 + RELAY_ID_SIZE
    sequence = int.from_bytes(packet[sequence_start:sequence_start + SEQUENCE_SIZE], "big")
    return relay_id, sequence, packet[CLIENT_HEADER_SIZE:-MAC_SIZE]


def encode_server_packet(sender_slot: int, sequence: int, payload: bytes, recipient_key: bytes) -> bytes:
    if sender_slot not in (1, 2):
        raise ValueError("sender_slot must be 1 or 2")
    if not 0 <= sequence <= 0xFFFFFFFF:
        raise ValueError("sequence must fit uint32")
    if len(recipient_key) != 32:
        raise ValueError("recipient_key must be 32 bytes")
    header = MAGIC + bytes([sender_slot]) + sequence.to_bytes(SEQUENCE_SIZE, "big")
    packet = header + payload
    packet += _mac(recipient_key, packet)
    if len(packet) > MAX_DATAGRAM:
        raise ValueError("datagram exceeds safe MTU budget")
    return packet


def decode_server_packet(packet: bytes, recipient_key: bytes):
    if len(packet) < SERVER_OVERHEAD or len(packet) > MAX_DATAGRAM:
        return None
    if packet[:4] != MAGIC or len(recipient_key) != 32:
        return None
    signed, supplied_mac = packet[:-MAC_SIZE], packet[-MAC_SIZE:]
    if not hmac.compare_digest(_mac(recipient_key, signed), supplied_mac):
        return None
    slot = packet[4]
    if slot not in (1, 2):
        return None
    sequence_start = 5
    sequence = int.from_bytes(packet[sequence_start:sequence_start + SEQUENCE_SIZE], "big")
    return slot, sequence, packet[SERVER_HEADER_SIZE:-MAC_SIZE]
