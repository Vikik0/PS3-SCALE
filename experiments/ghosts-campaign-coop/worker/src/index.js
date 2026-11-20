const SESSION_TTL_MS = 12 * 60 * 60 * 1000;
const ROOM_TTL_MS = 12 * 60 * 60 * 1000;
const ROOM_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
const TITLE_IDS = [
  { titleId: "BLUS31270", region: "US", distribution: "disc" },
  { titleId: "NPUB31301", region: "US", distribution: "digital" },
  { titleId: "NPUB50021", region: "US", distribution: "digital", edition: "Hardened Edition" },
  { titleId: "BLES01945", region: "EU/AU", distribution: "disc" },
  { titleId: "BLES01948", region: "EU/AU", distribution: "disc" },
  { titleId: "NPEB01832", region: "EU", distribution: "digital" },
  { titleId: "NPEB01835", region: "EU", distribution: "digital" },
  { titleId: "BLJM61125", region: "JP", distribution: "disc" },
  { titleId: "BLJM61126", region: "JP", distribution: "disc" },
  { titleId: "NPJB00528", region: "JP", distribution: "digital" }
];

const corsHeaders = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
  "Access-Control-Allow-Headers": "Authorization,Content-Type",
  "Cache-Control": "no-store",
  "X-Content-Type-Options": "nosniff"
};

function json(status, value) {
  return new Response(JSON.stringify(value), {
    status,
    headers: { ...corsHeaders, "Content-Type": "application/json; charset=utf-8" }
  });
}

function randomHex(byteLength) {
  const bytes = new Uint8Array(byteLength);
  crypto.getRandomValues(bytes);
  return Array.from(bytes, value => value.toString(16).padStart(2, "0")).join("");
}

function bytesToHex(bytes) {
  return Array.from(new Uint8Array(bytes), value => value.toString(16).padStart(2, "0")).join("");
}

async function hashToken(token) {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(token));
  return bytesToHex(digest);
}

async function readJson(request) {
  const length = Number(request.headers.get("Content-Length") || "0");
  if (length > 8192) throw { status: 413, code: "BODY_TOO_LARGE", message: "Request body must be 8 KiB or smaller." };
  try {
    const value = await request.json();
    if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("object required");
    return value;
  } catch {
    throw { status: 400, code: "INVALID_JSON", message: "Request body must be a JSON object." };
  }
}

function snapshot(room) {
  return {
    room_code: room.code,
    status: room.status,
    max_players: 2,
    player_count: room.players.length,
    players: [...room.players].sort((a, b) => a.slot - b.slot).map(player => ({
      session_id: player.sessionId,
      slot: player.slot,
      ready: player.ready,
      is_host: player.sessionId === room.hostSessionId,
      candidate_ready: Boolean(player.candidate)
    }))
  };
}

function roomError(status, code, message) {
  return { status, code, message };
}

export default {
  async fetch(request, env) {
    if (request.method === "OPTIONS") return new Response(null, { status: 204, headers: corsHeaders });
    const id = env.LOBBY.idFromName("global-lobby-v1");
    return env.LOBBY.get(id).fetch(request);
  }
};

export class LobbyDurableObject {
  constructor(ctx, env) {
    this.ctx = ctx;
    this.env = env;
  }

  async loadState() {
    return (await this.ctx.storage.get("state")) || { sessions: {}, rooms: {} };
  }

  async saveState(state) {
    await this.ctx.storage.put("state", state);
  }

  async authSession(request, state) {
    const header = request.headers.get("Authorization") || "";
    if (!header.startsWith("Bearer ")) return null;
    const token = header.slice(7).trim();
    if (!/^[0-9a-f]{64}$/i.test(token)) return null;
    const session = state.sessions[await hashToken(token)];
    if (!session || session.expiresAt <= Date.now()) return null;
    return session;
  }

  async pruneExpired(state) {
    const now = Date.now();
    for (const [hash, session] of Object.entries(state.sessions)) {
      if (session.expiresAt > now) continue;
      if (session.roomCode) this.removePlayer(state, session);
      delete state.sessions[hash];
    }
    for (const [code, room] of Object.entries(state.rooms)) {
      if (now - room.createdAt > ROOM_TTL_MS) {
        for (const player of room.players) {
          const peerSession = Object.values(state.sessions).find(item => item.sessionId === player.sessionId);
          if (peerSession) peerSession.roomCode = null;
        }
        delete state.rooms[code];
      }
    }
  }

  removePlayer(state, session) {
    const code = session.roomCode;
    if (!code) return;
    const room = state.rooms[code];
    session.roomCode = null;
    if (!room) return;
    room.players = room.players.filter(player => player.sessionId !== session.sessionId);
    if (room.players.length === 0 || room.hostSessionId === session.sessionId) {
      for (const player of room.players) {
        const peerSession = Object.values(state.sessions).find(item => item.sessionId === player.sessionId);
        if (peerSession) peerSession.roomCode = null;
      }
      delete state.rooms[code];
      return;
    }
    room.status = "waiting";
    for (const player of room.players) {
      player.ready = false;
      player.candidate = null;
    }
  }

  newRoomCode(state) {
    for (;;) {
      let code = "";
      for (let i = 0; i < 6; i++) code += ROOM_ALPHABET[Math.floor(Math.random() * ROOM_ALPHABET.length)];
      if (!state.rooms[code]) return code;
    }
  }

  findRoomPlayer(room, session) {
    return room.players.find(player => player.sessionId === session.sessionId) || null;
  }

  async fetch(request) {
    const url = new URL(request.url);
    const path = url.pathname;
    const method = request.method;
    const state = await this.loadState();

    try {
      await this.pruneExpired(state);

      if (method === "GET" && path === "/health") {
        await this.saveState(state);
        return json(200, {
          ok: true,
          service: "ghosts-coop-signalling",
          mode: "guest",
          transport: "peer-to-peer-udp",
          relay_fallback: false,
          max_players: 2
        });
      }

      if (method === "GET" && path === "/v1/compatibility") {
        return json(200, {
          game: "Call of Duty: Ghosts",
          platform: "PlayStation 3",
          target_title_update: "1.16",
          target_firmware: { type: "CEX", version: "4.93", status: "target_only_not_hardware_verified" },
          title_ids: TITLE_IDS
        });
      }

      if (method === "POST" && path === "/v1/sessions") {
        const body = await readJson(request);
        const mode = String(body.mode || "guest").toLowerCase();
        if (mode === "psn") throw roomError(501, "PSN_AUTH_NOT_IMPLEMENTED", "PSN authentication is not configured. Guest mode uses the private lobby and does not bypass PSN bans.");
        if (mode !== "guest") throw roomError(400, "INVALID_MODE", "Supported mode: guest.");
        const token = randomHex(32);
        const sessionId = randomHex(16);
        const relayId = randomHex(16);
        const relayKey = randomHex(32);
        const expiresAt = Date.now() + SESSION_TTL_MS;
        state.sessions[await hashToken(token)] = {
          sessionId,
          expiresAt,
          roomCode: null,
          relayId,
          relayKey
        };
        await this.saveState(state);
        return json(201, {
          session_id: sessionId,
          token,
          mode: "guest",
          expires_at: new Date(expiresAt).toISOString(),
          expires_in_seconds: SESSION_TTL_MS / 1000,
          relay: { protocol: "GHC2", relay_id: relayId, relay_key: relayKey }
        });
      }

      const session = await this.authSession(request, state);
      if (!session) throw roomError(401, "AUTH_REQUIRED", "A valid guest session token is required.");

      if (method === "POST" && path === "/v1/rooms") {
        if (session.roomCode) throw roomError(409, "ALREADY_IN_ROOM", "Leave the current room before creating another.");
        const code = this.newRoomCode(state);
        const room = {
          code,
          hostSessionId: session.sessionId,
          status: "waiting",
          createdAt: Date.now(),
          players: [{
            sessionId: session.sessionId,
            slot: 1,
            ready: false,
            relayId: session.relayId,
            relayKey: session.relayKey,
            candidate: null
          }]
        };
        state.rooms[code] = room;
        session.roomCode = code;
        await this.saveState(state);
        return json(201, snapshot(room));
      }

      const route = path.match(/^\/v1\/rooms\/([A-Z0-9]{6})(?:\/(join|ready|start|leave|candidate|peer))?$/);
      if (!route) throw roomError(404, "NOT_FOUND", "API route was not found.");
      const code = route[1];
      const action = route[2] || "get";
      const room = state.rooms[code];
      if (!room) throw roomError(404, "ROOM_NOT_FOUND", "Room code was not found.");

      if (action === "join" && method === "POST") {
        if (session.roomCode) throw roomError(409, "ALREADY_IN_ROOM", "Leave the current room before joining another.");
        if (room.status !== "waiting") throw roomError(409, "ROOM_ALREADY_STARTED", "This room has already started.");
        if (room.players.length >= 2) throw roomError(409, "ROOM_FULL", "This room already has two players.");
        room.players.push({
          sessionId: session.sessionId,
          slot: 2,
          ready: false,
          relayId: session.relayId,
          relayKey: session.relayKey,
          candidate: null
        });
        session.roomCode = code;
        await this.saveState(state);
        return json(200, snapshot(room));
      }

      if (action === "get" && method === "GET") {
        if (session.roomCode !== code || !this.findRoomPlayer(room, session)) {
          throw roomError(404, "ROOM_NOT_FOUND", "Room was not found for this session.");
        }
        await this.saveState(state);
        return json(200, snapshot(room));
      }

      if (action === "ready" && method === "POST") {
        if (session.roomCode !== code) throw roomError(404, "ROOM_NOT_FOUND", "Room code does not match your current room.");
        if (room.status !== "waiting") throw roomError(409, "ROOM_NOT_WAITING", "Ready state cannot change after start.");
        const body = await readJson(request);
        if (typeof body.ready !== "boolean") throw roomError(400, "INVALID_READY", "ready must be true or false.");
        const player = this.findRoomPlayer(room, session);
        if (!player) throw roomError(403, "NOT_IN_ROOM", "You are not a member of this room.");
        player.ready = body.ready;
        await this.saveState(state);
        return json(200, snapshot(room));
      }

      if (action === "start" && method === "POST") {
        if (session.roomCode !== code) throw roomError(404, "ROOM_NOT_FOUND", "Room code does not match your current room.");
        if (room.hostSessionId !== session.sessionId) throw roomError(403, "HOST_ONLY", "Only the host can start the mission.");
        if (room.players.length !== 2) throw roomError(409, "NEED_TWO_PLAYERS", "Exactly two players are required.");
        if (!room.players.every(player => player.ready)) throw roomError(409, "PLAYERS_NOT_READY", "Both players must be ready.");
        room.status = "running";
        await this.saveState(state);
        return json(200, snapshot(room));
      }

      if (action === "leave" && method === "POST") {
        if (session.roomCode !== code) throw roomError(404, "ROOM_NOT_FOUND", "Room code does not match your current room.");
        this.removePlayer(state, session);
        await this.saveState(state);
        return json(200, { left: true, room_code: code });
      }

      if (action === "candidate" && method === "POST") {
        if (session.roomCode !== code || room.status !== "running") {
          throw roomError(409, "ROOM_NOT_RUNNING", "Join a room and wait until both players have started before exchanging candidates.");
        }
        const player = this.findRoomPlayer(room, session);
        const peer = room.players.find(item => item.sessionId !== session.sessionId);
        if (!player || !peer) throw roomError(409, "NEED_TWO_PLAYERS", "A second player is required.");
        const body = await readJson(request);
        const publicAddress = String(body.public_address || "");
        const publicPort = Number(body.public_port);
        const localAddress = String(body.local_address || "");
        const localPort = Number(body.local_port);
        const natType = String(body.nat_type || "unknown").toLowerCase();
        const observedIp = request.headers.get("CF-Connecting-IP");
        if (!publicAddress || !Number.isInteger(publicPort) || publicPort < 1 || publicPort > 65535) {
          throw roomError(400, "INVALID_PUBLIC_CANDIDATE", "A public IP address and UDP port are required.");
        }
        if (!localAddress || !Number.isInteger(localPort) || localPort < 1 || localPort > 65535) {
          throw roomError(400, "INVALID_LOCAL_CANDIDATE", "A local IP address and UDP port are required.");
        }
        if (observedIp && publicAddress !== observedIp) {
          throw roomError(400, "PUBLIC_IP_MISMATCH", "The advertised public IP does not match the request source IP.");
        }
        if (!["open", "moderate", "strict", "symmetric", "unknown"].includes(natType)) {
          throw roomError(400, "INVALID_NAT_TYPE", "Unsupported NAT type.");
        }
        player.candidate = {
          public_address: publicAddress,
          public_port: publicPort,
          local_address: localAddress,
          local_port: localPort,
          nat_type: natType,
          updated_at: new Date().toISOString()
        };
        await this.saveState(state);
        return json(200, {
          own_candidate: player.candidate,
          peer_candidate: peer.candidate,
          peer: { session_id: peer.sessionId, slot: peer.slot, relay_id: peer.relayId, relay_key: peer.relayKey },
          room_status: room.status,
          transport: "peer-to-peer-udp",
          relay_fallback: false
        });
      }

      if (action === "peer" && method === "GET") {
        if (session.roomCode !== code) throw roomError(404, "ROOM_NOT_FOUND", "Room code does not match your current room.");
        const player = this.findRoomPlayer(room, session);
        const peer = room.players.find(item => item.sessionId !== session.sessionId);
        if (!player) throw roomError(403, "NOT_IN_ROOM", "You are not a member of this room.");
        if (!peer) return json(200, { room_status: room.status, peer_ready: false, peer_candidate: null, peer: null });
        return json(200, {
          room_status: room.status,
          peer_ready: peer.ready,
          peer_candidate: peer.candidate,
          peer: { session_id: peer.sessionId, slot: peer.slot, relay_id: peer.relayId, relay_key: peer.relayKey }
        });
      }

      throw roomError(404, "NOT_FOUND", "API route was not found.");
    } catch (error) {
      const status = Number(error?.status) || 500;
      const code = error?.code || "INTERNAL_ERROR";
      const message = error?.message || "Unexpected server error.";
      await this.saveState(state);
      return json(status, { error: code, message });
    }
  }
}
