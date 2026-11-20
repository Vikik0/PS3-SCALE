from __future__ import annotations

import socket
import threading
import time

from .protocol import decode_client_packet, encode_server_packet, MAX_DATAGRAM


class UdpRelay(threading.Thread):
    """Authenticated two-peer UDP relay; game-state payload remains opaque."""

    def __init__(self, manager, host: str = "0.0.0.0", port: int = 28991):
        super().__init__(name="ghosts-udp-relay", daemon=True)
        self.manager = manager
        self.host = host
        self.port = port
        self._stop_event = threading.Event()
        self._socket = None
        self._rate: dict[bytes, tuple[float, int]] = {}

    def stop(self):
        self._stop_event.set()
        if self._socket:
            try:
                self._socket.close()
            except OSError:
                pass

    def run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._socket = sock
        sock.bind((self.host, self.port))
        sock.settimeout(0.5)
        while not self._stop_event.is_set():
            try:
                packet, address = sock.recvfrom(MAX_DATAGRAM + 1)
            except socket.timeout:
                continue
            except OSError:
                break
            if len(packet) < 20:
                continue
            relay_id = packet[4:20]
            credentials = self.manager.relay_credentials(relay_id)
            if not credentials:
                continue
            token, relay_key = credentials
            decoded = decode_client_packet(packet, relay_key)
            if not decoded:
                continue
            _, sequence, payload = decoded
            if not self._allow_rate(relay_id):
                continue
            result = self.manager.resolve_udp(token, address, sequence)
            if not result:
                continue
            peer_address, sender_slot, peer_key = result
            if not peer_address:
                continue
            try:
                sock.sendto(encode_server_packet(sender_slot, sequence, payload, peer_key), peer_address)
            except (OSError, ValueError):
                continue

    def _allow_rate(self, relay_id: bytes) -> bool:
        """Simple per-session cap: 120 packets per rolling second."""
        now = time.monotonic()
        start, count = self._rate.get(relay_id, (now, 0))
        if now - start >= 1.0:
            self._rate[relay_id] = (now, 1)
            return True
        if count >= 120:
            return False
        self._rate[relay_id] = (start, count + 1)
        return True
