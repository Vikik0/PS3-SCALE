from __future__ import annotations

import os

from .http_api import make_server
from .room_manager import RoomManager
from .udp_relay import UdpRelay


def main():
    host = os.getenv("HOST", "0.0.0.0")
    http_port = int(os.getenv("HTTP_PORT", "8080"))
    udp_port = int(os.getenv("UDP_PORT", "28991"))
    manager = RoomManager()
    relay = UdpRelay(manager, host=host, port=udp_port)
    api = make_server(manager, host=host, port=http_port, udp_port=udp_port)
    relay.start()
    print(f"Lobby API listening on {host}:{http_port}")
    print(f"UDP relay listening on {host}:{udp_port}")
    try:
        api.serve_forever()
    except KeyboardInterrupt:
        print("Shutting down...")
    finally:
        api.shutdown()
        api.server_close()
        relay.stop()
        relay.join(timeout=2)


if __name__ == "__main__":
    main()
