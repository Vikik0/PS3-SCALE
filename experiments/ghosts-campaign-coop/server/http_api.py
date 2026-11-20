from __future__ import annotations

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import re

from .room_manager import CoopError
from .compatibility import GAME_PROFILES, inspect_install, list_profiles


class ApiHandler(BaseHTTPRequestHandler):
    manager = None
    udp_port = 28991

    def log_message(self, fmt, *args):
        # Keep logs useful without printing bearer tokens or request bodies.
        print("[http] " + (fmt % args))

    def _json(self, status: int, body: dict):
        encoded = json.dumps(body, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(encoded)

    def _body(self) -> dict:
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            raise CoopError("BAD_CONTENT_LENGTH", "Invalid Content-Length.", 400)
        if length < 0 or length > 8192:
            raise CoopError("BODY_TOO_LARGE", "Request body must be 8 KiB or smaller.", 413)
        raw = self.rfile.read(length) if length else b"{}"
        try:
            parsed = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            raise CoopError("INVALID_JSON", "Request body must be valid JSON.", 400)
        if not isinstance(parsed, dict):
            raise CoopError("INVALID_JSON", "Request body must be a JSON object.", 400)
        return parsed

    def _token(self) -> str | None:
        value = self.headers.get("Authorization", "")
        if value.startswith("Bearer "):
            return value[7:].strip()
        return None

    def _handle(self, method: str):
        try:
            path = self.path.split("?", 1)[0]
            if method == "GET" and path == "/health":
                return self._json(200, {"ok": True, "service": "ghosts-campaign-coop-lobby", "max_players": 2, "udp_port": self.udp_port, "auth_modes": ["guest"], "psn_mode": "not_configured"})

            if method == "GET" and path == "/v1/compatibility":
                return self._json(200, list_profiles())

            match = re.fullmatch(r"/v1/compatibility/([A-Z0-9]{9})", path)
            if method == "GET" and match:
                title_id = match.group(1)
                profile = GAME_PROFILES.get(title_id)
                if not profile:
                    raise CoopError("UNKNOWN_TITLE_ID", "Title ID is not in the current Ghosts PS3 registry.", 404)
                return self._json(200, {"profile": profile.__dict__, "target_title_update": "1.16", "target_firmware": {"type": "CEX", "version": "4.93", "status": "target_only_not_hardware_verified"}})

            if method == "POST" and path == "/v1/compatibility/check":
                data = self._body()
                result = inspect_install(
                    data.get("title_id", ""),
                    data.get("title_update", ""),
                    data.get("firmware_type", ""),
                    data.get("firmware_version", ""),
                )
                return self._json(200, result)

            if method == "POST" and path == "/v1/sessions":
                data = self._body()
                result = self.manager.create_session(data.get("mode", "guest"))
                return self._json(201, result)

            token = self._token()
            if not token:
                raise CoopError("AUTH_REQUIRED", "Send Authorization: Bearer <session-token>.", 401)

            if method == "POST" and path == "/v1/rooms":
                return self._json(201, self.manager.create_room(token))

            match = re.fullmatch(r"/v1/rooms/([A-Z0-9]{6})/(join|ready|start|leave)", path)
            if match:
                code, action = match.groups()
                if action == "join" and method == "POST":
                    return self._json(200, self.manager.join_room(token, code))
                if action == "ready" and method == "POST":
                    data = self._body()
                    if not isinstance(data.get("ready"), bool):
                        raise CoopError("INVALID_READY", "ready must be true or false.", 400)
                    return self._json(200, self.manager.set_ready(token, data["ready"]))
                if action == "start" and method == "POST":
                    return self._json(200, self.manager.start_room(token))
                if action == "leave" and method == "POST":
                    return self._json(200, self.manager.leave_room(token))

            match = re.fullmatch(r"/v1/rooms/([A-Z0-9]{6})", path)
            if match and method == "GET":
                return self._json(200, self.manager.get_room(token, match.group(1)))

            raise CoopError("NOT_FOUND", "API route was not found.", 404)
        except CoopError as exc:
            return self._json(exc.status, {"error": exc.code, "message": exc.message})
        except Exception:
            return self._json(500, {"error": "INTERNAL_ERROR", "message": "Unexpected server error."})

    def do_GET(self):
        self._handle("GET")

    def do_POST(self):
        self._handle("POST")


def make_server(manager, host="0.0.0.0", port=8080, udp_port=28991):
    handler = type("BoundApiHandler", (ApiHandler,), {"manager": manager, "udp_port": udp_port})
    return ThreadingHTTPServer((host, port), handler)
