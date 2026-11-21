"""Thread-safe two-player guest lobby and authenticated UDP relay sessions."""
from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone
import secrets
import threading
import uuid

SESSION_TTL = timedelta(hours=12)
ROOM_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
UDP_SEQUENCE_MASK = 0xFFFFFFFF
UDP_SEQUENCE_HALF = 0x80000000
UDP_REPLAY_WINDOW = 64


class CoopError(Exception):
    def __init__(self, code: str, message: str, status: int = 400):
        super().__init__(message)
        self.code = code
        self.message = message
        self.status = status


@dataclass
class Session:
    token: str
    session_id: str
    mode: str
    expires_at: datetime
    relay_id: bytes
    relay_key: bytes
    room_code: str | None = None
    udp_addr: tuple[str, int] | None = None
    udp_highest_sequence: int | None = None
    udp_sequence_window: int = 0


@dataclass
class Player:
    session_id: str
    token: str
    slot: int
    ready: bool = False


@dataclass
class Room:
    code: str
    host_session_id: str
    players: list[Player] = field(default_factory=list)
    status: str = "waiting"
    created_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))


class RoomManager:
    """Process-local lobby state. Use a shared durable store before multi-instance deployment."""

    def __init__(self) -> None:
        self._lock = threading.RLock()
        self.sessions: dict[str, Session] = {}
        self.rooms: dict[str, Room] = {}
        self.relay_sessions: dict[bytes, str] = {}

    def create_session(self, mode: str = "guest") -> dict:
        mode = (mode or "guest").lower()
        if mode == "psn":
            raise CoopError(
                "PSN_AUTH_NOT_IMPLEMENTED",
                "PSN identity is not configured. Guest mode uses the private service; this project does not bypass PSN bans or impersonate PSN.",
                501,
            )
        if mode != "guest":
            raise CoopError("INVALID_MODE", "Supported prototype mode: guest.", 400)
        with self._lock:
            self._prune_expired_locked()
            token = secrets.token_hex(16)
            session_id = str(uuid.uuid4())
            relay_id = secrets.token_bytes(16)
            relay_key = secrets.token_bytes(32)
            expires_at = datetime.now(timezone.utc) + SESSION_TTL
            session = Session(token, session_id, "guest", expires_at, relay_id, relay_key)
            self.sessions[token] = session
            self.relay_sessions[relay_id] = token
            return {
                "session_id": session_id,
                "token": token,
                "mode": "guest",
                "expires_at": expires_at.isoformat(),
                "expires_in_seconds": int(SESSION_TTL.total_seconds()),
                "relay": {
                    "protocol": "GHC2",
                    "relay_id": relay_id.hex(),
                    "relay_key": relay_key.hex(),
                },
            }

    def _prune_expired_locked(self) -> None:
        now = datetime.now(timezone.utc)
        expired = [token for token, session in self.sessions.items() if session.expires_at <= now]
        for token in expired:
            session = self.sessions.pop(token)
            self._leave_locked(session)
            self.relay_sessions.pop(session.relay_id, None)

    def _session(self, token: str | None) -> Session:
        if not token:
            raise CoopError("AUTH_REQUIRED", "A session token is required.", 401)
        session = self.sessions.get(token)
        if not session:
            raise CoopError("INVALID_SESSION", "Session token is invalid or expired.", 401)
        if session.expires_at <= datetime.now(timezone.utc):
            self.sessions.pop(token, None)
            self._leave_locked(session)
            self.relay_sessions.pop(session.relay_id, None)
            raise CoopError("SESSION_EXPIRED", "Session expired. Create a new guest session.", 401)
        return session

    def create_room(self, token: str) -> dict:
        with self._lock:
            session = self._session(token)
            if session.room_code:
                raise CoopError("ALREADY_IN_ROOM", "Leave the current room before creating another.", 409)
            code = self._new_code()
            room = Room(code=code, host_session_id=session.session_id)
            room.players.append(Player(session.session_id, token, 1))
            self.rooms[code] = room
            session.room_code = code
            return self._snapshot(room)

    def join_room(self, token: str, code: str) -> dict:
        with self._lock:
            session = self._session(token)
            if session.room_code:
                raise CoopError("ALREADY_IN_ROOM", "Leave the current room before joining another.", 409)
            room = self.rooms.get(code.upper())
            if not room:
                raise CoopError("ROOM_NOT_FOUND", "Room code was not found.", 404)
            if room.status != "waiting":
                raise CoopError("ROOM_ALREADY_STARTED", "This room has already started.", 409)
            if len(room.players) >= 2:
                raise CoopError("ROOM_FULL", "This room already has two players.", 409)
            room.players.append(Player(session.session_id, token, 2))
            session.room_code = room.code
            return self._snapshot(room)

    def set_ready(self, token: str, ready: bool, expected_room_code: str | None = None) -> dict:
        with self._lock:
            session = self._session(token)
            room = self._member_room(session, expected_room_code)
            if room.status != "waiting":
                raise CoopError("ROOM_NOT_WAITING", "Ready state cannot change after the session starts.", 409)
            player = self._player(room, session.session_id)
            player.ready = bool(ready)
            return self._snapshot(room)

    def start_room(self, token: str, expected_room_code: str | None = None) -> dict:
        with self._lock:
            session = self._session(token)
            room = self._member_room(session, expected_room_code)
            if room.host_session_id != session.session_id:
                raise CoopError("HOST_ONLY", "Only the room host can start the mission.", 403)
            if len(room.players) != 2:
                raise CoopError("NEED_TWO_PLAYERS", "A co-op session requires exactly two players.", 409)
            if not all(player.ready for player in room.players):
                raise CoopError("PLAYERS_NOT_READY", "Both players must be ready before starting.", 409)
            room.status = "running"
            return self._snapshot(room)

    def get_room(self, token: str, code: str) -> dict:
        with self._lock:
            session = self._session(token)
            room = self.rooms.get(code.upper())
            if not room or not any(p.session_id == session.session_id for p in room.players):
                raise CoopError("ROOM_NOT_FOUND", "Room was not found for this session.", 404)
            return self._snapshot(room)

    def leave_room(self, token: str, expected_room_code: str | None = None) -> dict:
        with self._lock:
            session = self._session(token)
            if expected_room_code and session.room_code != expected_room_code.upper():
                raise CoopError("ROOM_NOT_FOUND", "That room code does not match your current room.", 404)
            old_code = session.room_code
            self._leave_locked(session)
            return {"left": old_code is not None, "room_code": old_code}

    def relay_credentials(self, relay_id: bytes) -> tuple[str, bytes] | None:
        with self._lock:
            token = self.relay_sessions.get(relay_id)
            if not token:
                return None
            session = self.sessions.get(token)
            if not session or session.expires_at <= datetime.now(timezone.utc):
                return None
            return token, session.relay_key

    def resolve_udp(
        self, token: str, address: tuple[str, int], sequence: int
    ) -> tuple[tuple[str, int] | None, int, bytes] | None:
        """Register an authenticated sender endpoint and return peer endpoint, slot and peer key."""
        with self._lock:
            session = self.sessions.get(token)
            if not session or session.expires_at <= datetime.now(timezone.utc) or not session.room_code:
                return None
            if not self._accept_sequence(session, sequence):
                return None
            room = self.rooms.get(session.room_code)
            if not room or room.status != "running" or len(room.players) != 2:
                return None
            own = next((p for p in room.players if p.session_id == session.session_id), None)
            peer = next((p for p in room.players if p.session_id != session.session_id), None)
            if not own or not peer:
                return None
            session.udp_addr = address
            peer_session = self.sessions.get(peer.token)
            if not peer_session or peer_session.expires_at <= datetime.now(timezone.utc):
                return None
            return peer_session.udp_addr, own.slot, peer_session.relay_key

    @staticmethod
    def _accept_sequence(session: Session, sequence: int) -> bool:
        if not 0 <= sequence <= UDP_SEQUENCE_MASK:
            return False
        highest = session.udp_highest_sequence
        if highest is None:
            session.udp_highest_sequence = sequence
            session.udp_sequence_window = 1
            return True

        forward = (sequence - highest) & UDP_SEQUENCE_MASK
        if forward == 0:
            return False
        if forward < UDP_SEQUENCE_HALF:
            if forward >= UDP_REPLAY_WINDOW:
                session.udp_sequence_window = 1
            else:
                session.udp_sequence_window = (
                    (session.udp_sequence_window << forward) | 1
                ) & ((1 << UDP_REPLAY_WINDOW) - 1)
            session.udp_highest_sequence = sequence
            return True

        behind = (highest - sequence) & UDP_SEQUENCE_MASK
        if behind >= UDP_REPLAY_WINDOW:
            return False
        bit = 1 << behind
        if session.udp_sequence_window & bit:
            return False
        session.udp_sequence_window |= bit
        return True

    def _member_room(self, session: Session, expected_room_code: str | None = None) -> Room:
        if not session.room_code:
            raise CoopError("NOT_IN_ROOM", "Join or create a room first.", 409)
        if expected_room_code and session.room_code != expected_room_code.upper():
            raise CoopError("ROOM_NOT_FOUND", "That room code does not match your current room.", 404)
        room = self.rooms.get(session.room_code)
        if not room or not any(p.session_id == session.session_id for p in room.players):
            session.room_code = None
            raise CoopError("NOT_IN_ROOM", "You are no longer a member of that room.", 409)
        return room

    @staticmethod
    def _player(room: Room, session_id: str) -> Player:
        player = next((p for p in room.players if p.session_id == session_id), None)
        if not player:
            raise CoopError("NOT_IN_ROOM", "You are not a member of this room.", 403)
        return player

    def _leave_locked(self, session: Session) -> None:
        code = session.room_code
        if not code:
            return
        room = self.rooms.get(code)
        session.room_code = None
        session.udp_addr = None
        session.udp_highest_sequence = None
        session.udp_sequence_window = 0
        if not room:
            return
        room.players = [p for p in room.players if p.session_id != session.session_id]
        if not room.players or room.host_session_id == session.session_id:
            for player in room.players:
                peer_session = self.sessions.get(player.token)
                if peer_session:
                    peer_session.room_code = None
                    peer_session.udp_addr = None
                    peer_session.udp_highest_sequence = None
                    peer_session.udp_sequence_window = 0
            self.rooms.pop(code, None)
        else:
            room.status = "waiting"
            for player in room.players:
                player.ready = False
                peer_session = self.sessions.get(player.token)
                if peer_session:
                    peer_session.udp_addr = None
                    peer_session.udp_highest_sequence = None
                    peer_session.udp_sequence_window = 0

    def _new_code(self) -> str:
        while True:
            code = "".join(secrets.choice(ROOM_ALPHABET) for _ in range(6))
            if code not in self.rooms:
                return code

    @staticmethod
    def _snapshot(room: Room) -> dict:
        return {
            "room_code": room.code,
            "status": room.status,
            "max_players": 2,
            "player_count": len(room.players),
            "players": [
                {
                    "session_id": p.session_id,
                    "slot": p.slot,
                    "ready": p.ready,
                    "is_host": p.session_id == room.host_session_id,
                }
                for p in sorted(room.players, key=lambda item: item.slot)
            ],
        }
