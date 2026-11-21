const SESSION_TTL_MS = 12 * 60 * 60 * 1000;
const ROOM_TTL_MS = 6 * 60 * 60 * 1000;
const ROOM_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
const MAX_NAME_LENGTH = 16;
const MAX_TOTAL_SESSIONS = 16;
const MAX_ROOMS = 4;
const MAX_ACTIVE_MATCHES = 1;
const TRANSPORT_TICKET_TTL_MS = 60 * 1000;
const PS3_SESSION_TTL_MS = 6 * 60 * 60 * 1000;

const RELAY_PROTOCOL = "cbash-v1";
const RELAY_AUTH_PREFIX = "cbash-auth.";
const RELAY_TICKET_PREFIX = "cbash-ticket.";
const RELAY_MAGIC = new Uint8Array([0x43, 0x42, 0x52, 0x31]); // CBR1
const RELAY_MAX_PAYLOAD = 1200;
const RELAY_MAX_MESSAGE = 28672;
const RELAY_MAX_BATCH_PACKETS = 32;
const RELAY_MAX_MESSAGES_PER_SECOND = 5;

function messageBytes(message) {
  if (message instanceof ArrayBuffer) return new Uint8Array(message);
  if (ArrayBuffer.isView(message)) {
    return new Uint8Array(message.buffer, message.byteOffset, message.byteLength);
  }
  return null;
}

function makeRelayEnvelope(sessionId, payload) {
  const sender = new Uint8Array(16);
  for (let i = 0; i < 16; i++) sender[i] = Number.parseInt(sessionId.slice(i * 2, i * 2 + 2), 16);
  const envelope = new Uint8Array(22 + payload.byteLength);
  envelope.set(RELAY_MAGIC, 0);
  envelope.set(sender, 4);
  envelope[20] = (payload.byteLength >>> 8) & 0xff;
  envelope[21] = payload.byteLength & 0xff;
  envelope.set(payload, 22);
  return envelope;
}


// CBU1 uplink: magic, count, then recipient ID, payload length, and CBN1 datagrams.
// An all-zero recipient ID broadcasts the datagram to every other room member.
function parseRelayBatch(payload) {
  if (!payload || payload.byteLength < 1 || payload.byteLength > RELAY_MAX_MESSAGE) return null;
  if (payload.byteLength < 4 || payload[0] !== 0x43 || payload[1] !== 0x42 || payload[2] !== 0x55 || payload[3] !== 0x31) {
    return payload.byteLength <= RELAY_MAX_PAYLOAD ? [{ recipientId: null, payload }] : null;
  }
  if (payload.byteLength < 5) return null;
  const count = payload[4];
  if (count < 1 || count > RELAY_MAX_BATCH_PACKETS) return null;
  const packets = [];
  let offset = 5;
  for (let i = 0; i < count; i++) {
    if (offset + 18 > payload.byteLength) return null;
    const recipientBytes = payload.subarray(offset, offset + 16);
    const isBroadcast = recipientBytes.every(byte => byte === 0);
    const recipientId = isBroadcast ? null : Array.from(recipientBytes, byte => byte.toString(16).padStart(2, '0')).join('');
    const length = (payload[offset + 16] << 8) | payload[offset + 17];
    offset += 18;
    if (length < 1 || length > RELAY_MAX_PAYLOAD || offset + length > payload.byteLength) return null;
    packets.push({ recipientId, payload: payload.subarray(offset, offset + length) });
    offset += length;
  }
  return offset === payload.byteLength ? packets : null;
}
const CORS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET,POST,OPTIONS",
  "Access-Control-Allow-Headers": "Authorization,Content-Type",
  "Cache-Control": "no-store",
  "X-Content-Type-Options": "nosniff"
};

function response(status, data) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { ...CORS, "Content-Type": "application/json; charset=utf-8" }
  });
}

function fail(status, code, message) {
  return { status, code, message };
}

function randomHex(bytesLength) {
  const bytes = crypto.getRandomValues(new Uint8Array(bytesLength));
  return Array.from(bytes, b => b.toString(16).padStart(2, "0")).join("");
}

async function sha256(value) {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(value));
  return Array.from(new Uint8Array(digest), b => b.toString(16).padStart(2, "0")).join("");
}

function isIPv4(value) {
  const parts = String(value || "").split(".");
  if (parts.length !== 4) return false;
  return parts.every(part => /^\d{1,3}$/.test(part) && Number(part) >= 0 && Number(part) <= 255);
}

function cleanName(value) {
  const name = String(value || "Player").trim().replace(/[^a-zA-Z0-9 _-]/g, "").slice(0, MAX_NAME_LENGTH);
  return name || "Player";
}

async function readBody(request) {
  const declaredLength = Number(request.headers.get("Content-Length") || 0);
  if (declaredLength > 8192) {
    throw fail(413, "BODY_TOO_LARGE", "Request body limit is 8 KiB.");
  }
  const raw = await request.text();
  if (new TextEncoder().encode(raw).byteLength > 8192) {
    throw fail(413, "BODY_TOO_LARGE", "Request body limit is 8 KiB.");
  }
  try {
    const body = JSON.parse(raw);
    if (!body || typeof body !== "object" || Array.isArray(body)) throw new Error("object required");
    return body;
  } catch {
    throw fail(400, "INVALID_JSON", "Expected a JSON object.");
  }
}

function playerView(room, player) {
  return {
    player_id: player.sessionId,
    name: player.name,
    slot: player.slot,
    ready: player.ready,
    is_host: player.sessionId === room.hostSessionId
  };
}

function roomView(room) {
  return {
    room_code: room.code,
    status: room.status,
    mode: room.mode,
    max_players: room.maxPlayers,
    player_count: room.players.length,
    host_session_id: room.hostSessionId,
    is_private: room.isPrivate,
    created_at: new Date(room.createdAt).toISOString(),
    players: [...room.players].sort((a, b) => a.slot - b.slot).map(p => playerView(room, p)),
    host_endpoint_ready: Boolean(room.hostEndpoint)
  };
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const isApiRequest = request.method === "OPTIONS" ||
      url.pathname === "/health" ||
      url.pathname.startsWith("/v1/");

    if (!isApiRequest) {
      if (env.ASSETS) return env.ASSETS.fetch(request);
      return new Response("Not Found", { status: 404, headers: CORS });
    }

    if (request.method === "OPTIONS") {
      return new Response(null, { status: 204, headers: CORS });
    }

    const id = env.LOBBY.idFromName("crash-bash-online-lobby-v1");
    return env.LOBBY.get(id).fetch(request);
  }
};

export class CrashBashLobby {
  constructor(ctx, env) {
    this.ctx = ctx;
    this.env = env;
  }

  async load() {
    const state = (await this.ctx.storage.get("state")) || {
      sessions: {},
      rooms: {},
      transportTickets: {}
    };
    state.transportTickets ||= {};
    return state;
  }

  async save(state) {
    await this.ctx.storage.put("state", state);
  }

  async authenticate(request, state) {
    const header = request.headers.get("Authorization") || "";
    const protocols = (request.headers.get("Sec-WebSocket-Protocol") || "")
      .split(",")
      .map(value => value.trim());
    const authProtocol = protocols.find(value => value.startsWith(RELAY_AUTH_PREFIX));
    const token = header.startsWith("Bearer ")
      ? header.slice(7).trim()
      : authProtocol
        ? authProtocol.slice(RELAY_AUTH_PREFIX.length)
        : "";
    if (!/^[0-9a-f]{64}$/i.test(token)) return null;
    const session = state.sessions[await sha256(token)];
    if (!session || session.expiresAt <= Date.now()) return null;
    return session;
  }

  async authenticateTransportTicket(request, state, roomCode) {
    const protocols = (request.headers.get("Sec-WebSocket-Protocol") || "")
      .split(",")
      .map(value => value.trim());
    const ticketProtocol = protocols.find(value => value.startsWith(RELAY_TICKET_PREFIX));
    const ticket = ticketProtocol ? ticketProtocol.slice(RELAY_TICKET_PREFIX.length) : "";
    if (!/^[0-9a-f]{64}$/i.test(ticket)) return null;

    const ticketHash = await sha256(ticket);
    const record = state.transportTickets[ticketHash];
    if (!record || record.expiresAt <= Date.now() || record.roomCode !== roomCode) return null;
    const session = state.sessions[record.sessionHash];
    if (!session || session.expiresAt <= Date.now() || session.roomCode !== roomCode) return null;

    delete state.transportTickets[ticketHash];
    return session;
  }

  roomForSession(state, session) {
    if (!session.roomCode) return null;
    const room = state.rooms[session.roomCode];
    if (!room) return null;
    const player = room.players.find(p => p.sessionId === session.sessionId);
    return player ? { room, player } : null;
  }

  closeRoomSockets(roomCode, reason = "ROOM_RESET") {
    if (typeof this.ctx.getWebSockets !== "function") return;
    for (const socket of this.ctx.getWebSockets()) {
      try {
        const attachment = socket.deserializeAttachment();
        if (attachment?.roomCode === roomCode) socket.close(1008, reason);
      } catch {
        // A socket may already be closing; it must not block room cleanup.
      }
    }
  }

  closePlayerSockets(sessionId, reason = "PLAYER_LEFT") {
    if (typeof this.ctx.getWebSockets !== "function") return;
    for (const socket of this.ctx.getWebSockets()) {
      try {
        const attachment = socket.deserializeAttachment();
        if (attachment?.sessionId === sessionId) socket.close(1008, reason);
      } catch {
        // A socket may already be closing; it must not block room cleanup.
      }
    }
  }

  broadcastRoomUpdate(room) {
    if (!room || typeof this.ctx.getWebSockets !== "function") return;
    const message = JSON.stringify({ type: "room_update", room: roomView(room) });
    for (const socket of this.ctx.getWebSockets()) {
      try {
        const attachment = socket.deserializeAttachment();
        if (attachment?.roomCode !== room.code) continue;
        attachment.matchStarted = room.status === "running";
        socket.serializeAttachment(attachment);
        socket.send(message);
      } catch {
        // Ignore peers that have already disconnected.
      }
    }
  }

  async webSocketMessage(socket, message) {
    let attachment;
    try {
      attachment = socket.deserializeAttachment();
    } catch {
      socket.close(1008, "INVALID_SESSION");
      return;
    }

    if (!attachment || !/^[A-Z0-9]{6}$/.test(attachment.roomCode || "") ||
        !/^[0-9a-f]{32}$/i.test(attachment.sessionId || "")) {
      socket.close(1008, "INVALID_SESSION");
      return;
    }

    const now = Date.now();
    if (!Number.isFinite(attachment.rateWindowStartedAt) ||
        now - attachment.rateWindowStartedAt >= 1000 ||
        now < attachment.rateWindowStartedAt) {
      attachment.rateWindowStartedAt = now;
      attachment.rateCount = 0;
    }
    if (attachment.rateCount >= RELAY_MAX_MESSAGES_PER_SECOND) {
      socket.close(1008, "RATE_LIMITED");
      return;
    }
    attachment.rateCount += 1;
    socket.serializeAttachment(attachment);

    if (typeof message === "string") {
      if (message === "PING") {
        socket.send("PONG");
        return;
      }
      if (message === "START") {
        const state = await this.load();
        const room = state.rooms[attachment.roomCode];
        if (!room || room.hostSessionId !== attachment.sessionId) {
          socket.send("ERROR:NOT_HOST");
          return;
        }
        if (room.status !== "waiting") {
          socket.send("ERROR:ROOM_NOT_WAITING");
          return;
        }
        if (room.players.length < 2 || !room.players.every(player => player.ready)) {
          socket.send("ERROR:PLAYERS_NOT_READY");
          return;
        }
        if (Object.values(state.rooms).filter(item => item.status === "running").length >= MAX_ACTIVE_MATCHES) {
          socket.send("ERROR:FREE_TIER_MATCH_LIMIT");
          return;
        }
        room.status = "running";
        room.startedAt = Date.now();
        await this.save(state);
        this.broadcastRoomUpdate(room);
        socket.send("STARTED");
        return;
      }
      socket.close(1003, "BINARY_PACKETS_REQUIRED");
      return;
    }

    // Lockstep lobby packets (join/ping/roster) must flow before START.
    // Authenticated room membership and per-session rate limits protect this pre-start phase.

    const payload = messageBytes(message);
    if (!payload) {
      socket.close(1003, 'BINARY_PACKETS_REQUIRED');
      return;
    }
    const isBatch = payload.byteLength >= 4 &&
      payload[0] === 0x43 && payload[1] === 0x42 &&
      payload[2] === 0x55 && payload[3] === 0x31;
    if (payload.byteLength > RELAY_MAX_MESSAGE ||
        (!isBatch && payload.byteLength > RELAY_MAX_PAYLOAD)) {
      socket.close(1009, 'PACKET_SIZE_INVALID');
      return;
    }
    const packets = parseRelayBatch(payload);
    if (!packets) {
      socket.close(1008, 'BATCH_INVALID');
      return;
    }
    if (typeof this.ctx.getWebSockets !== 'function') return;
    const peers = this.ctx.getWebSockets();
    for (const packet of packets) {
      const envelope = makeRelayEnvelope(attachment.sessionId, packet.payload);
      for (const peer of peers) {
        if (peer === socket) continue;
        try {
          const peerAttachment = peer.deserializeAttachment();
          if (peerAttachment?.roomCode !== attachment.roomCode || peerAttachment?.sessionId === attachment.sessionId) continue;
          if (packet.recipientId && peerAttachment?.sessionId !== packet.recipientId) continue;
          peer.send(envelope.buffer.slice(0));
        } catch {
          // Ignore peers that have already disconnected.
        }
      }
    }
  }

  async webSocketClose(socket) {
    let attachment;
    try {
      attachment = socket.deserializeAttachment();
    } catch {
      return;
    }
    if (!attachment?.sessionId) return;

    if (typeof this.ctx.getWebSockets === "function") {
      for (const other of this.ctx.getWebSockets()) {
        if (other === socket) continue;
        try {
          if (other.deserializeAttachment()?.sessionId === attachment.sessionId) return;
        } catch {
          // Ignore sockets already closing.
        }
      }
    }

    const state = await this.load();
    const sessionEntry = Object.entries(state.sessions).find(([, item]) =>
      item.sessionId === attachment.sessionId
    );
    if (!sessionEntry) return;
    const [sessionHash, session] = sessionEntry;
    if (!session.roomCode) return;
    this.removePlayer(state, session);
    if (session.platform === "ps3") delete state.sessions[sessionHash];
    await this.save(state);
  }

  webSocketError() {}

  removePlayer(state, session) {
    const current = this.roomForSession(state, session);
    if (!current) {
      this.closePlayerSockets(session.sessionId, "PLAYER_LEFT");
      session.roomCode = null;
      return;
    }
    const { room, player } = current;
    this.closePlayerSockets(session.sessionId, "PLAYER_LEFT");
    room.players = room.players.filter(p => p.sessionId !== session.sessionId);
    session.roomCode = null;

    if (room.players.length === 0) {
      this.closeRoomSockets(room.code, "ROOM_EMPTY");
      delete state.rooms[room.code];
      return;
    }

    if (room.hostSessionId === player.sessionId) {
      room.hostSessionId = room.players.sort((a, b) => a.slot - b.slot)[0].sessionId;
      room.hostEndpoint = null;
    }

    room.status = "waiting";
    room.startedAt = null;
    for (const remaining of room.players) {
      remaining.ready = false;
      remaining.candidate = null;
    }
    this.broadcastRoomUpdate(room);
  }

  async prune(state) {
    const now = Date.now();
    for (const [hash, session] of Object.entries(state.sessions)) {
      if (session.expiresAt <= now) {
        this.removePlayer(state, session);
        delete state.sessions[hash];
      }
    }
    for (const [hash, ticket] of Object.entries(state.transportTickets || {})) {
      if (ticket.expiresAt <= now) {
        const session = state.sessions[ticket.sessionHash];
        if (session?.platform === "ps3" && session.roomCode) {
          this.removePlayer(state, session);
          delete state.sessions[ticket.sessionHash];
        }
        delete state.transportTickets[hash];
      }
    }
    for (const [code, room] of Object.entries(state.rooms)) {
      if (now - room.createdAt > ROOM_TTL_MS) {
        this.closeRoomSockets(code, "ROOM_EXPIRED");
        for (const player of room.players) {
          const session = Object.values(state.sessions).find(s => s.sessionId === player.sessionId);
          if (session) session.roomCode = null;
        }
        delete state.rooms[code];
      }
    }
  }

  newCode(state) {
    for (let attempt = 0; attempt < 100; attempt++) {
      const bytes = crypto.getRandomValues(new Uint8Array(6));
      const code = Array.from(bytes, b => ROOM_ALPHABET[b % ROOM_ALPHABET.length]).join("");
      if (!state.rooms[code]) return code;
    }
    throw fail(503, "ROOM_CODES_EXHAUSTED", "Unable to allocate a room code. Try again.");
  }

  async fetch(request) {
    const url = new URL(request.url);
    const path = url.pathname;
    const method = request.method;
    const state = await this.load();

    try {
      await this.prune(state);

      if (method === "GET" && path === "/health") {
        await this.save(state);
        return response(200, {
          ok: true,
          service: "crash-bash-online-lobby",
          auth: "guest",
          psn_required: false,
          max_players: 4,
          max_open_rooms: MAX_ROOMS,
          max_total_sessions: MAX_TOTAL_SESSIONS,
          max_active_matches: MAX_ACTIVE_MATCHES,
          transport: "websocket-room-relay-prototype",
          gameplay_sync: "client-integration-pending"
        });
      }

      if (method === "GET" && path === "/v1/compatibility") {
        return response(200, {
          game: "Crash Bash",
          original_platform: "PlayStation",
          target_platform: "PlayStation 3",
          online_mode: "community matchmaking prototype",
          psn_required: false,
          local_player_capacity: 4,
          ps3_emulator_target: "PS1 emulator process; exact firmware/process revision pending hardware verification",
          supported_disc_serials: [
            { serial: "SCUS-94570", region: "NTSC-U" },
            { serial: "SCES-02834", region: "PAL" },
            { serial: "SCPS-10140", region: "NTSC-J" },
            { serial: "SCPS-91319", region: "NTSC-J PSone Books" }
          ],
          network_gameplay: "room WebSocket relay prototype; PS3 and browser game transports are not integrated yet"
        });
      }

      if (method === "GET" && path === "/v1/rooms/public") {
        const rooms = Object.values(state.rooms)
          .filter(room => !room.isPrivate &&
                          room.status === "waiting" &&
                          room.players.length < room.maxPlayers)
          .sort((a, b) => a.createdAt - b.createdAt)
          .map(room => ({
            room_code: room.code,
            mode: room.mode,
            max_players: room.maxPlayers,
            player_count: room.players.length,
            host_name: room.players.find(player => player.sessionId === room.hostSessionId)?.name || "Host",
            created_at: new Date(room.createdAt).toISOString()
          }));
        // This endpoint is read-only: avoid a Durable Object storage write for every
        // public-room refresh. The state was not mutated while building this list.
        return response(200, { rooms });
      }

      if (method === "POST" && path === "/v1/sessions") {
        if (Object.keys(state.sessions).length >= MAX_TOTAL_SESSIONS) {
          throw fail(503, "FREE_TIER_SESSION_LIMIT", "The free service has reached its shared browser and PS3 session limit. Try again later.");
        }
        const body = await readBody(request);
        const mode = String(body.mode || "guest").toLowerCase();
        if (mode === "psn") throw fail(501, "PSN_NOT_REQUIRED", "Use Guest Mode. This community lobby does not use PSN authentication.");
        if (mode !== "guest") throw fail(400, "INVALID_MODE", "Only Guest Mode is supported.");
        const token = randomHex(32);
        const sessionId = randomHex(16);
        const expiresAt = Date.now() + SESSION_TTL_MS;
        const session = {
          sessionId,
          name: cleanName(body.display_name),
          expiresAt,
          roomCode: null
        };
        state.sessions[await sha256(token)] = session;
        await this.save(state);
        return response(201, {
          session_id: sessionId,
          display_name: session.name,
          token,
          mode: "guest",
          psn_required: false,
          expires_at: new Date(expiresAt).toISOString()
        });
      }

      if (method === "POST" && path === "/v1/rooms/host-guest") {
        if (Object.keys(state.sessions).length >= MAX_TOTAL_SESSIONS) {
          throw fail(503, "FREE_TIER_SESSION_LIMIT", "The free service has reached its shared browser and PS3 session limit. Try again later.");
        }
        if (Object.keys(state.rooms).length >= MAX_ROOMS) {
          throw fail(503, "FREE_TIER_ROOM_LIMIT", "The free service supports up to four open rooms. Try again later.");
        }

        const body = await readBody(request);
        const name = cleanName(body.display_name || "PS3 Host");
        const code = this.newCode(state);
        const token = randomHex(32);
        const sessionId = randomHex(16);
        const sessionHash = await sha256(token);
        const expiresAt = Date.now() + PS3_SESSION_TTL_MS;
        const session = { sessionId, name, expiresAt, roomCode: code, platform: "ps3" };
        state.sessions[sessionHash] = session;

        const room = {
          code,
          hostSessionId: sessionId,
          status: "waiting",
          mode: "party",
          maxPlayers: 4,
          isPrivate: true,
          createdAt: Date.now(),
          startedAt: null,
          hostEndpoint: null,
          players: [{ sessionId, name, slot: 1, ready: true, candidate: null }]
        };
        state.rooms[code] = room;

        const ticket = randomHex(32);
        const ticketHash = await sha256(ticket);
        const ticketExpiresAt = Date.now() + TRANSPORT_TICKET_TTL_MS;
        state.transportTickets[ticketHash] = {
          sessionHash,
          roomCode: code,
          expiresAt: ticketExpiresAt
        };
        await this.save(state);
        this.broadcastRoomUpdate(room);
        return response(201, {
          room_code: code,
          session_id: sessionId,
          host_session_id: sessionId,
          display_name: name,
          ticket,
          ticket_expires_at: new Date(ticketExpiresAt).toISOString(),
          expires_at: new Date(expiresAt).toISOString(),
          psn_required: false,
          room: roomView(room)
        });
      }

      const guestJoinRoute = path.match(/^\/v1\/rooms\/([A-Z0-9]{6})\/join-guest$/);
      if (method === "POST" && guestJoinRoute) {
        if (Object.keys(state.sessions).length >= MAX_TOTAL_SESSIONS) {
          throw fail(503, "FREE_TIER_SESSION_LIMIT", "The free service has reached its shared browser and PS3 session limit. Try again later.");
        }
        const code = guestJoinRoute[1];
        const room = state.rooms[code];
        if (!room) throw fail(404, "ROOM_NOT_FOUND", "Room code was not found.");
        if (room.status !== "waiting") throw fail(409, "ROOM_ALREADY_STARTED", "This match has already started.");
        if (room.players.length >= room.maxPlayers) throw fail(409, "ROOM_FULL", "This room is full.");

        const body = await readBody(request);
        const name = cleanName(body.display_name || "PS3 Player");
        const token = randomHex(32);
        const sessionId = randomHex(16);
        const sessionHash = await sha256(token);
        const expiresAt = Date.now() + PS3_SESSION_TTL_MS;
        const session = { sessionId, name, expiresAt, roomCode: code, platform: "ps3" };
        state.sessions[sessionHash] = session;

        const occupied = new Set(room.players.map(player => player.slot));
        let slot = 1;
        while (occupied.has(slot)) slot++;
        room.players.push({ sessionId, name, slot, ready: true, candidate: null });

        const ticket = randomHex(32);
        const ticketHash = await sha256(ticket);
        const ticketExpiresAt = Date.now() + TRANSPORT_TICKET_TTL_MS;
        state.transportTickets[ticketHash] = {
          sessionHash,
          roomCode: code,
          expiresAt: ticketExpiresAt
        };
        await this.save(state);
        this.broadcastRoomUpdate(room);
        return response(201, {
          session_id: sessionId,
          display_name: name,
          ticket,
          ticket_expires_at: new Date(ticketExpiresAt).toISOString(),
          expires_at: new Date(expiresAt).toISOString(),
          psn_required: false,
          room: roomView(room)
        });
      }

      let session = await this.authenticate(request, state);
      const transportRoute = path.match(/^\/v1\/rooms\/([A-Z0-9]{6})\/transport$/);
      if (transportRoute && method === "GET") {
        if ((request.headers.get("Upgrade") || "").toLowerCase() !== "websocket") {
          throw fail(426, "WEBSOCKET_REQUIRED", "This endpoint requires a WebSocket upgrade.");
        }
        const protocols = (request.headers.get("Sec-WebSocket-Protocol") || "")
          .split(",")
          .map(value => value.trim());
        if (!protocols.includes(RELAY_PROTOCOL)) {
          throw fail(400, "RELAY_PROTOCOL_REQUIRED", "Request the cbash-v1 WebSocket subprotocol.");
        }

        const code = transportRoute[1];
        const room = state.rooms[code];
        if (!room) throw fail(404, "ROOM_NOT_FOUND", "Room code was not found.");
        if (!session) {
          session = await this.authenticateTransportTicket(request, state, code);
          if (session) await this.save(state);
        }
        if (!session) throw fail(401, "AUTH_REQUIRED", "A guest session or one-time PS3 transport ticket is required.");
        if (session.roomCode !== code ||
            !room.players.some(player => player.sessionId === session.sessionId)) {
          throw fail(403, "NOT_IN_ROOM", "Join this room before opening its game transport.");
        }
        const pair = new WebSocketPair();
        const client = pair[0];
        const server = pair[1];
        server.serializeAttachment({
          roomCode: code,
          sessionId: session.sessionId,
          matchStarted: room.status === "running",
          rateWindowStartedAt: 0,
          rateCount: 0
        });
        this.ctx.acceptWebSocket(server);
        server.send(JSON.stringify({ type: "room_update", room: roomView(room) }));
        if (typeof this.ctx.getWebSockets === "function") {
          for (const existing of this.ctx.getWebSockets()) {
            if (existing === server) continue;
            try {
              if (existing.deserializeAttachment()?.sessionId === session.sessionId) {
                existing.close(1008, "REPLACED");
              }
            } catch {
              // Ignore stale sockets while replacing a previous connection.
            }
          }
        }
        return new Response(null, {
          status: 101,
          webSocket: client,
          headers: { "Sec-WebSocket-Protocol": RELAY_PROTOCOL }
        });
      }

      if (!session) throw fail(401, "AUTH_REQUIRED", "Create a Guest Mode session first.");

      if (method === "GET" && path === "/v1/rooms/current") {
        const current = this.roomForSession(state, session);
        if (!current) return response(200, { room: null });
        await this.save(state);
        return response(200, { room: roomView(current.room) });
      }

      if (method === "POST" && path === "/v1/rooms") {
        if (session.roomCode) throw fail(409, "ALREADY_IN_ROOM", "Leave your current room first.");
        if (Object.keys(state.rooms).length >= MAX_ROOMS) {
          throw fail(503, "FREE_TIER_ROOM_LIMIT", "The free service supports up to four open rooms. Try again later.");
        }
        const body = await readBody(request);
        const maxPlayers = Number(body.max_players || 4);
        if (!Number.isInteger(maxPlayers) || maxPlayers < 2 || maxPlayers > 4) {
          throw fail(400, "INVALID_PLAYER_LIMIT", "max_players must be between 2 and 4.");
        }
        const code = this.newCode(state);
        const room = {
          code,
          hostSessionId: session.sessionId,
          status: "waiting",
          mode: String(body.mode || "party").slice(0, 24),
          maxPlayers,
          isPrivate: body.is_private !== false,
          createdAt: Date.now(),
          startedAt: null,
          hostEndpoint: null,
          players: [{
            sessionId: session.sessionId,
            name: session.name,
            slot: 1,
            ready: false,
            candidate: null
          }]
        };
        state.rooms[code] = room;
        session.roomCode = code;
        await this.save(state);
        this.broadcastRoomUpdate(room);
        return response(201, roomView(room));
      }

      const route = path.match(/^\/v1\/rooms\/([A-Z0-9]{6})(?:\/(join|ready|start|leave|peer|endpoint))?$/);
      if (!route) throw fail(404, "NOT_FOUND", "API route was not found.");
      const code = route[1];
      const action = route[2] || "get";
      const room = state.rooms[code];
      if (!room) throw fail(404, "ROOM_NOT_FOUND", "Room code was not found.");

      if (action === "join" && method === "POST") {
        if (session.roomCode === code) {
          const existingPlayer = room.players.find(player => player.sessionId === session.sessionId);
          if (existingPlayer) {
            await this.save(state);
            return response(200, roomView(room));
          }
          session.roomCode = null;
        }
        if (session.roomCode) throw fail(409, "ALREADY_IN_ROOM", "Leave your current room first.");
        if (room.status !== "waiting") throw fail(409, "ROOM_ALREADY_STARTED", "This match has already started.");
        if (room.players.length >= room.maxPlayers) throw fail(409, "ROOM_FULL", "This room is full.");
        const occupied = new Set(room.players.map(p => p.slot));
        let slot = 1;
        while (occupied.has(slot)) slot++;
        room.players.push({
          sessionId: session.sessionId,
          name: session.name,
          slot,
          ready: false,
          candidate: null
        });
        session.roomCode = code;
        await this.save(state);
        this.broadcastRoomUpdate(room);
        return response(200, roomView(room));
      }

      if (action === "get" && method === "GET") {
        if (session.roomCode !== code || !this.roomForSession(state, session)) {
          throw fail(404, "ROOM_NOT_FOUND", "This room is not associated with your session.");
        }
        await this.save(state);
        return response(200, roomView(room));
      }

      if (action === "ready" && method === "POST") {
        if (session.roomCode !== code) throw fail(404, "ROOM_NOT_FOUND", "Room code does not match your session.");
        if (room.status !== "waiting") throw fail(409, "ROOM_NOT_WAITING", "Ready state is locked after the match starts.");
        const body = await readBody(request);
        if (typeof body.ready !== "boolean") throw fail(400, "INVALID_READY", "ready must be a boolean.");
        const player = room.players.find(p => p.sessionId === session.sessionId);
        if (!player) throw fail(403, "NOT_IN_ROOM", "You are not a member of this room.");
        player.ready = body.ready;
        await this.save(state);
        this.broadcastRoomUpdate(room);
        return response(200, roomView(room));
      }

      if (action === "start" && method === "POST") {
        if (session.roomCode !== code) throw fail(404, "ROOM_NOT_FOUND", "Room code does not match your session.");
        if (room.hostSessionId !== session.sessionId) throw fail(403, "HOST_ONLY", "Only the room host can start the match.");
        if (room.players.length < 2) throw fail(409, "NEED_MORE_PLAYERS", "At least two players are required.");
        if (!room.players.every(p => p.ready)) throw fail(409, "PLAYERS_NOT_READY", "Every player must be ready.");
        const activeMatches = Object.values(state.rooms).filter(candidate =>
          candidate.status === "running" && candidate.code !== code
        ).length;
        if (activeMatches >= MAX_ACTIVE_MATCHES) {
          throw fail(409, "FREE_TIER_MATCH_LIMIT", "The free service allows one active match at a time. Try again when the current match ends.");
        }
        room.status = "running";
        room.startedAt = Date.now();
        await this.save(state);
        this.broadcastRoomUpdate(room);
        return response(200, roomView(room));
      }

      if (action === "leave" && method === "POST") {
        if (session.roomCode !== code) throw fail(404, "ROOM_NOT_FOUND", "Room code does not match your session.");
        this.removePlayer(state, session);
        await this.save(state);
        return response(200, { left: true, room_code: code });
      }

      if (action === "peer" && method === "GET") {
        if (session.roomCode !== code) throw fail(404, "ROOM_NOT_FOUND", "Room code does not match your session.");
        const player = room.players.find(p => p.sessionId === session.sessionId);
        if (!player) throw fail(403, "NOT_IN_ROOM", "You are not a member of this room.");
        return response(200, {
          room_status: room.status,
          host_endpoint: room.hostEndpoint,
          players: room.players.filter(p => p.sessionId !== session.sessionId).map(p => ({
            player_id: p.sessionId,
            name: p.name,
            slot: p.slot,
            ready: p.ready
          }))
        });
      }

      if (action === "endpoint" && method === "POST") {
        if (session.roomCode !== code) throw fail(404, "ROOM_NOT_FOUND", "Room code does not match your session.");
        if (room.hostSessionId !== session.sessionId) throw fail(403, "HOST_ONLY", "Only the room host can publish the game endpoint.");
        const body = await readBody(request);
        const port = Number(body.public_port);
        const observedAddress = request.headers.get("CF-Connecting-IP") || "";
        const suppliedAddress = String(body.public_address || "");
        const address = observedAddress || suppliedAddress;
        if (!Number.isInteger(port) || port < 1 || port > 65535) {
          throw fail(400, "INVALID_UDP_PORT", "public_port must be between 1 and 65535.");
        }
        if (!isIPv4(address)) {
          throw fail(400, "INVALID_PUBLIC_IP", "A public IPv4 address is required for the first PS3 transport.");
        }
        if (observedAddress && suppliedAddress && suppliedAddress !== observedAddress) {
          throw fail(400, "PUBLIC_IP_MISMATCH", "The advertised IP does not match the request source IP.");
        }
        room.hostEndpoint = {
          address,
          port,
          protocol: "udp",
          published_at: new Date().toISOString()
        };
        await this.save(state);
        return response(200, { host_endpoint: room.hostEndpoint });
      }

      if (action === "endpoint" && method === "GET") {
        if (session.roomCode !== code) throw fail(404, "ROOM_NOT_FOUND", "Room code does not match your session.");
        if (!room.players.some(p => p.sessionId === session.sessionId)) {
          throw fail(403, "NOT_IN_ROOM", "You are not a member of this room.");
        }
        return response(200, { host_endpoint: room.hostEndpoint });
      }

      throw fail(404, "NOT_FOUND", "API route was not found.");
    } catch (error) {
      const status = Number(error?.status) || 500;
      const code = error?.code || "INTERNAL_ERROR";
      const message = error?.message || "Unexpected server error.";
      await this.save(state);
      return response(status, { error: code, message });
    }
  }
}
