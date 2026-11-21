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


const GHC_MAGIC_BYTES = new Uint8Array([71, 72, 67, 50]);
const GHC_CLIENT_HEADER_BYTES = 24;
const GHC_SERVER_HEADER_BYTES = 9;
const GHC_MAC_BYTES = 16;
const GHC_MAX_FRAME_BYTES = 1200;
const GHC_REPLAY_MASK = (1n << 64n) - 1n;

function ghcHexToBytes(value, length) {
  if (typeof value !== "string" || !new RegExp("^[0-9a-f]{" + (length * 2) + "}$", "i").test(value)) return null;
  const output = new Uint8Array(length);
  for (let i = 0; i < length; i++) output[i] = Number.parseInt(value.slice(i * 2, i * 2 + 2), 16);
  return output;
}

function ghcEqualBytes(left, right) {
  if (!left || !right || left.length !== right.length) return false;
  let difference = 0;
  for (let i = 0; i < left.length; i++) difference |= left[i] ^ right[i];
  return difference === 0;
}

function ghcReadU32(bytes, offset) {
  return (((bytes[offset] << 24) >>> 0) | (bytes[offset + 1] << 16) | (bytes[offset + 2] << 8) | bytes[offset + 3]) >>> 0;
}

function ghcWriteU32(bytes, offset, value) {
  bytes[offset] = (value >>> 24) & 255;
  bytes[offset + 1] = (value >>> 16) & 255;
  bytes[offset + 2] = (value >>> 8) & 255;
  bytes[offset + 3] = value & 255;
}

async function ghcHmac16(keyBytes, signedBytes) {
  const key = await crypto.subtle.importKey("raw", keyBytes, { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const signature = new Uint8Array(await crypto.subtle.sign("HMAC", key, signedBytes));
  return signature.slice(0, GHC_MAC_BYTES);
}

async function ghcDecodeClientFrame(packet, expectedRelayIdHex, relayKeyHex) {
  if (!(packet instanceof Uint8Array) || packet.length < 40 || packet.length > GHC_MAX_FRAME_BYTES) return null;
  if (!ghcEqualBytes(packet.subarray(0, 4), GHC_MAGIC_BYTES)) return null;
  const expectedRelayId = ghcHexToBytes(expectedRelayIdHex, 16);
  const relayKey = ghcHexToBytes(relayKeyHex, 32);
  if (!expectedRelayId || !relayKey || !ghcEqualBytes(packet.subarray(4, 20), expectedRelayId)) return null;
  const signed = packet.subarray(0, packet.length - GHC_MAC_BYTES);
  const suppliedMac = packet.subarray(packet.length - GHC_MAC_BYTES);
  const expectedMac = await ghcHmac16(relayKey, signed);
  if (!ghcEqualBytes(suppliedMac, expectedMac)) return null;
  return {
    sequence: ghcReadU32(packet, 20),
    payload: packet.slice(GHC_CLIENT_HEADER_BYTES, packet.length - GHC_MAC_BYTES)
  };
}

async function ghcEncodeServerFrame(senderSlot, sequence, payload, recipientKeyHex) {
  const recipientKey = ghcHexToBytes(recipientKeyHex, 32);
  if (!recipientKey || (senderSlot !== 1 && senderSlot !== 2)) throw new Error("Invalid GHC2 relay frame parameters");
  const output = new Uint8Array(GHC_SERVER_HEADER_BYTES + payload.length + GHC_MAC_BYTES);
  output.set(GHC_MAGIC_BYTES, 0);
  output[4] = senderSlot;
  ghcWriteU32(output, 5, sequence);
  output.set(payload, GHC_SERVER_HEADER_BYTES);
  const mac = await ghcHmac16(recipientKey, output.subarray(0, output.length - GHC_MAC_BYTES));
  output.set(mac, output.length - GHC_MAC_BYTES);
  return output;
}

function ghcAcceptSequence(window, sequence) {
  sequence >>>= 0;
  if (!window.initialized) {
    window.initialized = true;
    window.highest = sequence;
    window.bitmap = 1n;
    return true;
  }
  const forward = (sequence - window.highest) >>> 0;
  if (forward === 0) return false;
  if (forward < 0x80000000) {
    window.bitmap = forward >= 64 ? 1n : ((window.bitmap << BigInt(forward)) | 1n) & GHC_REPLAY_MASK;
    window.highest = sequence;
    return true;
  }
  const behind = (window.highest - sequence) >>> 0;
  if (behind >= 64) return false;
  const bit = 1n << BigInt(behind);
  if ((window.bitmap & bit) !== 0n) return false;
  window.bitmap |= bit;
  return true;
}

async function ghcToBytes(data) {
  if (data instanceof ArrayBuffer) return new Uint8Array(data);
  if (ArrayBuffer.isView(data)) return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
  if (data && typeof data.arrayBuffer === "function") return new Uint8Array(await data.arrayBuffer());
  return null;
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
    this.relaySockets = new Map();
    this.relayRates = new Map();
    this.replayWindows = new Map();
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


  allowRelayRate(sessionId) {
    const now = Date.now();
    const current = this.relayRates.get(sessionId) || { startedAt: now, count: 0 };
    if (now - current.startedAt >= 1000) {
      this.relayRates.set(sessionId, { startedAt: now, count: 1 });
      return true;
    }
    if (current.count >= 120) return false;
    current.count += 1;
    this.relayRates.set(sessionId, current);
    return true;
  }

  closeRelayConnection(connection, code = 1000, reason = "Relay connection closed") {
    if (this.relaySockets.get(connection.sessionId) !== connection) return;
    this.relaySockets.delete(connection.sessionId);
    this.replayWindows.delete(connection.sessionId);
    this.relayRates.delete(connection.sessionId);
    try {
      if (connection.socket.readyState === 1) connection.socket.close(code, reason);
    } catch {}
    const peerConnection = [...this.relaySockets.values()].find(item => item.roomCode === connection.roomCode);
    if (peerConnection) {
      this.relaySockets.delete(peerConnection.sessionId);
      this.replayWindows.delete(peerConnection.sessionId);
      this.relayRates.delete(peerConnection.sessionId);
      try {
        if (peerConnection.socket.readyState === 1) peerConnection.socket.close(1012, "Peer disconnected");
      } catch {}
    }
  }

  async handleRelayMessage(connection, data) {
    const packet = await ghcToBytes(data);
    if (!packet) {
      this.closeRelayConnection(connection, 1003, "Binary GHC2 packets are required");
      return;
    }
    if (packet.length > GHC_MAX_FRAME_BYTES) {
      this.closeRelayConnection(connection, 1009, "GHC2 packet exceeds 1200 bytes");
      return;
    }

    const state = await this.loadState();
    const session = Object.values(state.sessions).find(item => item.sessionId === connection.sessionId);
    const room = state.rooms[connection.roomCode];
    if (!session || session.expiresAt <= Date.now() || session.roomCode !== connection.roomCode || !room || room.status !== "running") {
      this.closeRelayConnection(connection, 1008, "Session or room is no longer active");
      return;
    }
    const player = this.findRoomPlayer(room, session);
    const peer = room.players.find(item => item.sessionId !== session.sessionId);
    if (!player || !peer) {
      this.closeRelayConnection(connection, 1008, "Room membership is invalid");
      return;
    }

    const decoded = await ghcDecodeClientFrame(packet, session.relayId, session.relayKey);
    if (!decoded) {
      this.closeRelayConnection(connection, 1008, "Invalid GHC2 authentication");
      return;
    }
    let replayWindow = this.replayWindows.get(session.sessionId);
    if (!replayWindow) {
      replayWindow = { initialized: false, highest: 0, bitmap: 0n };
      this.replayWindows.set(session.sessionId, replayWindow);
    }
    if (!ghcAcceptSequence(replayWindow, decoded.sequence)) return;
    if (!this.allowRelayRate(session.sessionId)) return;

    const peerConnection = this.relaySockets.get(peer.sessionId);
    if (!peerConnection || peerConnection.socket.readyState !== 1) return;
    if (Number(peerConnection.socket.bufferedAmount || 0) > 65536) return;
    const forwarded = await ghcEncodeServerFrame(player.slot, decoded.sequence, decoded.payload, peer.relayKey);
    peerConnection.socket.send(forwarded);
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
          transport: "peer-to-peer-udp-with-websocket-fallback",
          relay_fallback: true,
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

      const relayRoute = path.match(/^\/v1\/rooms\/([A-Z0-9]{6})\/relay$/);
      if (relayRoute && method === "GET") {
        if ((request.headers.get("Upgrade") || "").toLowerCase() !== "websocket") {
          throw roomError(426, "WEBSOCKET_UPGRADE_REQUIRED", "Connect to this endpoint using a secure WebSocket upgrade.");
        }
        if (typeof WebSocketPair === "undefined") {
          throw roomError(501, "WEBSOCKET_UNAVAILABLE", "WebSocket relay is unavailable in this runtime.");
        }
        const code = relayRoute[1];
        const room = state.rooms[code];
        if (!room || session.roomCode !== code) {
          throw roomError(404, "ROOM_NOT_FOUND", "Room was not found for this session.");
        }
        if (room.status !== "running" || room.players.length !== 2) {
          throw roomError(409, "ROOM_NOT_RUNNING", "Both players must be ready and the room must be running.");
        }
        const player = this.findRoomPlayer(room, session);
        const peer = room.players.find(item => item.sessionId !== session.sessionId);
        if (!player || !peer) throw roomError(403, "NOT_IN_ROOM", "A valid two-player room membership is required.");

        const pair = new WebSocketPair();
        const clientSocket = pair[0];
        const serverSocket = pair[1];
        const previous = this.relaySockets.get(session.sessionId);
        if (previous) this.closeRelayConnection(previous, 1012, "Connection replaced");
        serverSocket.accept();
        serverSocket.binaryType = "arraybuffer";
        const connection = {
          sessionId: session.sessionId,
          roomCode: code,
          slot: player.slot,
          socket: serverSocket
        };
        this.relaySockets.set(session.sessionId, connection);
        serverSocket.addEventListener("message", event => this.handleRelayMessage(connection, event.data).catch(() => {
          this.closeRelayConnection(connection, 1011, "Relay processing failed");
        }));
        serverSocket.addEventListener("close", () => this.closeRelayConnection(connection));
        serverSocket.addEventListener("error", () => this.closeRelayConnection(connection, 1011, "WebSocket error"));
        return new Response(null, { status: 101, webSocket: clientSocket });
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
          relay_fallback: true,
          relay_websocket_path: "/v1/rooms/" + code + "/relay"
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
          peer: { session_id: peer.sessionId, slot: peer.slot, relay_id: peer.relayId, relay_key: peer.relayKey },
          relay_fallback: true,
          relay_websocket_path: "/v1/rooms/" + code + "/relay"
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
