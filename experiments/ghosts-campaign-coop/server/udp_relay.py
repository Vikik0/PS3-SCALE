from __future__ import annotations

import socket
import threading
import time

from .protocol import decode_client_packet, encode_server_packet, MAX_DATAGRAM


class UdpRelay(threading.Thread):
    """Minimal two-peer UDP relay. It does not implement game-state simulation."""

    def __init__(self, manager, host: str = "0.0.0.0", port: int = 28991):
        super().__init__(name="ghosts-udp-relay", daemon=True)
        self.manager = manager
        self.host = host
        self.port = port
        self._stop_event = threading.Event()
        self._socket = None
        self._rate: dict[str, tuple[float, int]] = {}

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
            decoded = decode_client_packet(packet)
            if not decoded:
                continue
            token, sequence, payload = decoded
            if not self._allow_rate(token):
                continue
            result = self.manager.resolve_udp(token, address)
            if not result:
                continue
            peer_address, sender_slot = result
            if not peer_address:
                continue
            try:
                sock.sendto(encode_server_packet(sender_slot, sequence, payload), peer_address)
            except OSError:
                continue

    def _allow_rate(self, token: str) -> bool:
        """Simple per-session cap: 120 packets per rolling second."""
        now = time.monotonic()
        start, count = self._rate.get(token, (now, 0))
        if now - start >= 1.0:
            self._rate[token] = (now, 1)
            return True
        if count >= 120:
            return False
        self._rate[token] = (start, count + 1)
        return True
