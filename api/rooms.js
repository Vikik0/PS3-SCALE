"use strict";

const crypto = require("node:crypto");
const { get, put, list, del } = require("@vercel/blob");
const {
  signGuestToken,
  verifyGuestToken,
  cleanDisplayName,
  bearerToken
} = require("./_lib/security");

const ACCESS = "private";
const PREFIX = "cb-rooms/";
const ROOM_TTL_MS = 30 * 60 * 1000;
const PLAYER_TTL_MS = 10 * 60 * 1000;
const MAX_PLAYERS = 4;
const CODE_CHARS = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
const LIBRETRO_LOBBY_LIST_URL = "http://lobby.libretro.com/list";
let netplayLobbyCache = { expiresAt: 0, hostNickname: "", entry: null };

function reply(res, status, data) {
  res.setHeader("Cache-Control", "no-store");
  res.setHeader("X-Content-Type-Options", "nosniff");
  return res.status(status).json(data);
}

function makeCode() {
  const bytes = crypto.randomBytes(6);
  let result = "";
  for (let i = 0; i < 6; i += 1) result += CODE_CHARS[bytes[i] % CODE_CHARS.length];
  return result;
}

function pathFor(code) {
  return PREFIX + code + ".json";
}

async function readRoom(code) {
  const result = await get(pathFor(code), { access: ACCESS });
  if (!result || result.statusCode !== 200 || !result.stream) return null;
  return {
    room: JSON.parse(await new Response(result.stream).text()),
    etag: result.blob.etag
  };
}

async function createRoom(code, room) {
  await put(pathFor(code), JSON.stringify(room), {
    access: ACCESS,
    addRandomSuffix: false,
    contentType: "application/json"
  });
}

async function saveRoom(code, room, expectedEtag) {
  if (!expectedEtag) throw new Error("Missing room version for conditional write");
  await put(pathFor(code), JSON.stringify(room), {
    access: ACCESS,
    allowOverwrite: true,
    ifMatch: expectedEtag,
    contentType: "application/json"
  });
}

async function deleteExpiredRoom(code, expectedEtag) {
  try {
    await del(pathFor(code), { access: ACCESS, ifMatch: expectedEtag });
    return true;
  } catch (error) {
    // A heartbeat or join updated the room after the listing read it. Keep the
    // newer version; the next listing pass will evaluate its fresh timestamp.
    if (error?.name === "BlobPreconditionFailedError") return false;
    throw error;
  }
}

function tokenClaims(req) {
  return verifyGuestToken(bearerToken(req));
}

function activePlayers(room, now = Date.now()) {
  return (room.players || []).filter((player) => now - player.lastSeen <= PLAYER_TTL_MS);
}

function prunePlayers(room, now = Date.now()) {
  const previous = room.players || [];
  const active = activePlayers(room, now);
  const hostIsActive = active.some((player) => player.id === room.hostPlayerId);
  if (!room.hostPlayerId || !hostIsActive) {
    const nextHostId = active[0]?.id || "";
    if (nextHostId && nextHostId !== room.hostPlayerId) {
      room.netplayId = crypto.randomBytes(6).toString("hex").toUpperCase();
    }
    room.hostPlayerId = nextHostId;
  }
  if (active.length !== previous.length) {
    active.forEach((player) => { player.ready = false; });
    const activeIds = new Set(active.map((player) => player.id));
    room.signals = (room.signals || []).filter((signal) =>
      activeIds.has(signal.from) && activeIds.has(signal.to)
    );
    room.status = "waiting";
  }
  room.players = active;
  return active;
}

function publicRoom(room) {
  const active = activePlayers(room);
  return {
    code: room.code,
    host: active[0]?.name || "Guest",
    players: active.length,
    maxPlayers: MAX_PLAYERS,
    createdAt: room.createdAt,
    status: room.status === "ready" && (active.length < 2 || active.length !== (room.players || []).length) ? "waiting" : room.status
  };
}

async function listRooms() {
  const candidates = [];
  let cursor;
  do {
    const page = await list({ prefix: PREFIX, limit: 100, ...(cursor ? { cursor } : {}) });
    candidates.push(...(page.blobs || []));
    cursor = page.hasMore && page.cursor ? page.cursor : undefined;
  } while (cursor);

  const validCandidates = candidates.filter((blob) => {
    const code = blob.pathname.slice(PREFIX.length).replace(/\.json$/, "");
    return /^[A-Z2-9]{6}$/.test(code);
  });
  const output = [];

  // Read rooms in small concurrent batches across every Blob listing page. This
  // avoids arbitrary room-count cutoffs without creating one unbounded read burst.
  for (let offset = 0; offset < validCandidates.length; offset += 10) {
    const batch = await Promise.all(validCandidates.slice(offset, offset + 10).map(async (blob) => {
      const code = blob.pathname.slice(PREFIX.length).replace(/\.json$/, "");
      try {
        const record = await readRoom(code);
        if (!record) return null;
        const room = record.room;
        const now = Date.now();
        const active = activePlayers(room, now);
        if (!Number.isFinite(room.updatedAt) || now - room.updatedAt > ROOM_TTL_MS || active.length === 0) {
          await deleteExpiredRoom(code, record.etag);
          return null;
        }
        const staleReadyRoom = room.status === "ready" &&
          (active.length < 2 || active.length !== (room.players || []).length);
        if (!room.isPrivate && (room.status === "waiting" || staleReadyRoom) && active.length < MAX_PLAYERS) {
          return publicRoom(room);
        }
      } catch {
        // A corrupt or expired room must not prevent the rest of the lobby listing.
      }
      return null;
    }));
    output.push(...batch.filter(Boolean));
  }
  return output.sort((a, b) => b.createdAt - a.createdAt);
}

function findPlayer(room, playerId) {
  return (room.players || []).find((player) => player.id === playerId);
}

async function getLibretroLobbyEntry(hostNickname) {
  if (Date.now() < netplayLobbyCache.expiresAt &&
      netplayLobbyCache.hostNickname === hostNickname) {
    return netplayLobbyCache.entry;
  }
  const response = await fetch(LIBRETRO_LOBBY_LIST_URL, {
    headers: { Accept: "application/json" },
    signal: AbortSignal.timeout(5000)
  });
  if (!response.ok) throw new Error("RetroArch Netplay lobby is unavailable.");
  const entries = await response.json();
  if (!Array.isArray(entries)) throw new Error("RetroArch Netplay lobby returned an invalid list.");
  const entry = entries.find((candidate) =>
    candidate?.fields?.username === hostNickname
  ) || null;
  netplayLobbyCache = {
    expiresAt: Date.now() + 1500,
    hostNickname,
    entry
  };
  return entry;
}

function netplayEndpoint(fields) {
  if (!fields || fields.connectable !== true) return null;
  const updatedAt = Date.parse(fields.updated || fields.created || "");
  if (Number.isFinite(updatedAt) && Date.now() - updatedAt > 90000) return null;

  const validHost = (value) =>
    typeof value === "string" &&
    value.length > 0 &&
    value.length <= 127 &&
    /^[A-Za-z0-9.:-]+$/.test(value);
  const validPort = (value) => Number.isInteger(Number(value)) &&
    Number(value) >= 1 && Number(value) <= 65535;

  if (validHost(fields.mitm_ip) && validPort(fields.mitm_port) &&
      typeof fields.mitm_session === "string" &&
      /^[A-Za-z0-9+/=]{8,64}$/.test(fields.mitm_session)) {
    return {
      mode: "relay",
      address: fields.mitm_ip,
      port: Number(fields.mitm_port),
      mitmSession: fields.mitm_session
    };
  }

  if (validHost(fields.ip) && validPort(fields.port)) {
    return {
      mode: "direct",
      address: fields.ip,
      port: Number(fields.port),
      mitmSession: ""
    };
  }

  return null;
}

async function mutateRoom(code, update, maxAttempts = 6) {
  for (let attempt = 0; attempt < maxAttempts; attempt += 1) {
    const record = await readRoom(code);
    if (!record) return { found: false, saved: false };

    const outcome = update(record.room);
    if (outcome && outcome.write === false) {
      return { found: true, saved: false, room: record.room, outcome };
    }

    try {
      await saveRoom(code, record.room, record.etag);
      return { found: true, saved: true, room: record.room, outcome: outcome || {} };
    } catch (error) {
      if (error?.name !== "BlobPreconditionFailedError" || attempt === maxAttempts - 1) {
        throw error;
      }
    }
  }
  throw new Error("Room update retry limit reached.");
}

module.exports = async function handler(req, res) {
  if (!process.env.BLOB_READ_WRITE_TOKEN || !(process.env.CB_TOKEN_SECRET_V2 || process.env.CB_TOKEN_SECRET)) {
    return reply(res, 503, { error: "Lobby storage is not configured yet." });
  }

  try {
    if (req.method === "GET") {
      const code = String(req.query?.code || "").toUpperCase();
      if (!code) return reply(res, 200, { rooms: await listRooms() });
      if (!/^[A-Z2-9]{6}$/.test(code)) return reply(res, 400, { error: "Invalid room code." });

      const claims = tokenClaims(req);
      if (!claims || claims.room !== code) {
        return reply(res, 401, { error: "A valid room guest token is required." });
      }
      const record = await readRoom(code);
      if (!record) return reply(res, 404, { error: "Room not found." });
      if (!Number.isFinite(record.room.updatedAt) || Date.now() - record.room.updatedAt > ROOM_TTL_MS) {
        return reply(res, 410, { error: "This battle room has expired. Create a new room." });
      }
      const active = activePlayers(record.room);
      const inactiveSeats = active.length !== (record.room.players || []).length;
      const player = active.find((entry) => entry.id === claims.sub);
      if (!player) {
        return reply(res, 403, { error: "Your room seat has expired. Rejoin the room." });
      }

      return reply(res, 200, {
        room: {
          code: record.room.code,
          status: record.room.status === "ready" && (active.length < 2 || active.length !== (record.room.players || []).length) ? "waiting" : record.room.status,
          hostPlayerId: record.room.hostPlayerId || active[0]?.id || "",
          netplayId: record.room.netplayId || "",
          isPrivate: record.room.isPrivate,
          players: active.map(({ id, name, ready }) => ({ id, name, ready: inactiveSeats ? false : ready })),
          selfPlayerId: claims.sub,
          maxPlayers: MAX_PLAYERS,
          createdAt: record.room.createdAt,
          updatedAt: record.room.updatedAt
        }
      });
    }

    if (req.method !== "POST") {
      res.setHeader("Allow", "GET, POST");
      return reply(res, 405, { error: "Method not allowed." });
    }

    const body = req.body && typeof req.body === "object" ? req.body : {};
    const action = String(body.action || "");
    const name = cleanDisplayName(body.displayName);

    if (action === "create") {
      for (let attempt = 0; attempt < 5; attempt += 1) {
        const code = makeCode();
        const playerId = crypto.randomUUID();
        const now = Date.now();
        const room = {
          code,
          status: "waiting",
          isPrivate: Boolean(body.isPrivate),
          maxPlayers: MAX_PLAYERS,
          createdAt: now,
          updatedAt: now,
          hostPlayerId: playerId,
          netplayId: crypto.randomBytes(6).toString("hex").toUpperCase(),
          players: [{ id: playerId, name, ready: false, joinedAt: now, lastSeen: now }],
          signals: [],
          nextSignalId: 1
        };
        try {
          await createRoom(code, room);
          return reply(res, 201, {
            room: publicRoom(room),
            player: { id: playerId, name },
            token: signGuestToken({ sub: playerId, room: code })
          });
        } catch (error) {
          if (attempt === 4) throw error;
        }
      }
    }

    if (action === "any") {
      const candidates = await listRooms();
      for (const candidate of candidates) {
        const playerId = crypto.randomUUID();
        const outcome = await mutateRoom(candidate.code, (room) => {
          const now = Date.now();
          prunePlayers(room, now);
          if (room.isPrivate || room.status !== "waiting" || room.players.length >= MAX_PLAYERS) {
            return { write: false, skip: true };
          }
          room.players.push({ id: playerId, name, ready: false, joinedAt: now, lastSeen: now });
          room.updatedAt = now;
          return {
            status: 200,
            body: {
              room: publicRoom(room),
              player: { id: playerId, name },
              token: signGuestToken({ sub: playerId, room: candidate.code })
            }
          };
        });
        if (!outcome.found || outcome.outcome?.skip) continue;
        return reply(res, 200, outcome.outcome.body);
      }
      return reply(res, 404, { error: "No public battle is waiting. Create a public battle first." });
    }

    if (action === "join") {
      const code = String(body.code || "").toUpperCase();
      if (!/^[A-Z2-9]{6}$/.test(code)) {
        return reply(res, 400, { error: "Enter a valid six-character room code." });
      }
      const playerId = crypto.randomUUID();
      const outcome = await mutateRoom(code, (room) => {
        const now = Date.now();
        if (!Number.isFinite(room.updatedAt) || now - room.updatedAt > ROOM_TTL_MS) {
          return { write: false, status: 410, body: { error: "This battle room has expired. Create a new room." } };
        }
        prunePlayers(room, now);
        if (room.status !== "waiting" || room.players.length >= MAX_PLAYERS) {
          return { write: false, status: 409, body: { error: "Room is full or already playing." } };
        }
        room.players.push({ id: playerId, name, ready: false, joinedAt: now, lastSeen: now });
        room.updatedAt = now;
        return {
          status: 200,
          body: {
            room: publicRoom(room),
            player: { id: playerId, name },
            token: signGuestToken({ sub: playerId, room: code })
          }
        };
      });
      if (!outcome.found) return reply(res, 404, { error: "Room not found." });
      return reply(res, outcome.outcome.status || 200, outcome.outcome.body);
    }

    const claims = tokenClaims(req);
    if (!claims) return reply(res, 401, { error: "Guest token missing or expired." });
    const code = claims.room;
    const record = await readRoom(code);
    if (!record) return reply(res, 404, { error: "Room not found." });
    if (action !== "leave" &&
        (!Number.isFinite(record.room.updatedAt) || Date.now() - record.room.updatedAt > ROOM_TTL_MS)) {
      return reply(res, 410, { error: "This battle room has expired. Create a new room." });
    }
    const player = findPlayer(record.room, claims.sub);
    if (!player) return reply(res, 403, { error: "You are not a member of this room." });

    if (action === "netplay") {
      const active = activePlayers(record.room);
      const activePlayer = active.find((entry) => entry.id === claims.sub);
      if (!activePlayer) {
        return reply(res, 403, { error: "Your room seat has expired. Rejoin the room." });
      }
      if (record.room.status !== "ready" ||
          active.length < 2 ||
          active.length !== (record.room.players || []).length ||
          !active.every((entry) => entry.ready)) {
        return reply(res, 409, {
          ready: false,
          state: "room_not_ready",
          error: "Every active player must be READY before Netplay can start."
        });
      }

      const hostPlayerId = active.some((entry) => entry.id === record.room.hostPlayerId)
        ? record.room.hostPlayerId
        : active[0]?.id;
      if (!hostPlayerId) {
        return reply(res, 409, { error: "No active room host is available." });
      }

      // Rooms created before 0.6.0 used the invite code as their RetroArch nickname.
      const netplayId = record.room.netplayId || record.room.code;
      const hostNickname = "CB-" + netplayId + "-HOST";
      const hostEntry = await getLibretroLobbyEntry(hostNickname);
      if (!hostEntry) {
        return reply(res, 200, {
          ready: false,
          state: "waiting_for_host",
          hostNickname
        });
      }

      const endpoint = netplayEndpoint(hostEntry.fields);
      if (!endpoint) {
        return reply(res, 200, {
          ready: false,
          state: "host_not_connectable",
          hostNickname
        });
      }

      return reply(res, 200, {
        ready: true,
        state: "connected",
        hostNickname,
        ...endpoint,
        gameName: String(hostEntry.fields.game_name || "").slice(0, 100),
        coreName: String(hostEntry.fields.core_name || "").slice(0, 80),
        coreVersion: String(hostEntry.fields.core_version || "").slice(0, 80),
        retroArchVersion: String(hostEntry.fields.retroarch_version || "").slice(0, 40),
        updatedAt: hostEntry.fields.updated || hostEntry.fields.created || ""
      });
    }

    if (action === "heartbeat") {
      const outcome = await mutateRoom(code, (room) => {
        const currentPlayer = findPlayer(room, claims.sub);
        if (!currentPlayer) {
          return { write: false, status: 403, body: { error: "You are no longer in this room." } };
        }
        const now = Date.now();
        prunePlayers(room, now);
        const activePlayer = findPlayer(room, claims.sub);
        if (!activePlayer) {
          return { write: false, status: 403, body: { error: "Your room seat has expired. Rejoin the room." } };
        }
        activePlayer.lastSeen = now;
        if (typeof body.ready === "boolean") activePlayer.ready = body.ready;
        const active = activePlayers(room, now);
        room.status = active.length >= 2 && active.every((entry) => entry.ready) ? "ready" : "waiting";
        room.updatedAt = now;
        return {
          status: 200,
          body: {
            ok: true,
            hostPlayerId: room.hostPlayerId || active[0]?.id || "",
            netplayId: room.netplayId || "",
            players: active.map(({ id, name, ready }) => ({ id, name, ready }))
          }
        };
      });
      if (!outcome.found) return reply(res, 404, { error: "Room not found." });
      return reply(res, outcome.outcome.status || 200, outcome.outcome.body);
    }

    if (action === "signal") {
      const type = String(body.type || "");
      if (!["offer", "answer", "ice"].includes(type)) {
        return reply(res, 400, { error: "Unsupported signal type." });
      }
      const payload = JSON.stringify(body.payload ?? null);
      if (Buffer.byteLength(payload, "utf8") > 12000) {
        return reply(res, 413, { error: "Signal payload too large." });
      }
      const targetPlayerId = String(body.toPlayerId || "");
      if (!targetPlayerId) {
        return reply(res, 400, { error: "A target player ID is required for room signaling." });
      }
      const outcome = await mutateRoom(code, (room) => {
        const now = Date.now();
        prunePlayers(room, now);
        const currentPlayer = findPlayer(room, claims.sub);
        if (!currentPlayer) {
          return { write: false, status: 403, body: { error: "Your room seat has expired. Rejoin the room." } };
        }
        const target = room.players.find((entry) =>
          entry.id === targetPlayerId && entry.id !== currentPlayer.id
        );
        if (!target) {
          return { write: false, status: 404, body: { error: "Target player is not an active member of this room." } };
        }
        room.signals = room.signals || [];
        room.nextSignalId = room.nextSignalId || 1;
        room.signals.push({
          id: room.nextSignalId++,
          from: currentPlayer.id,
          to: target.id,
          type,
          payload: JSON.parse(payload),
          createdAt: now
        });
        room.signals = room.signals.slice(-32);
        room.updatedAt = now;
        return { status: 202, body: { ok: true } };
      });
      if (!outcome.found) return reply(res, 404, { error: "Room not found." });
      return reply(res, outcome.outcome.status || 202, outcome.outcome.body);
    }

    if (action === "poll") {
      const activePlayer = activePlayers(record.room).find((entry) => entry.id === player.id);
      if (!activePlayer) {
        return reply(res, 403, { error: "Your room seat has expired. Rejoin the room." });
      }
      const since = Number.isInteger(body.since) && body.since >= 0 ? body.since : 0;
      const signals = (record.room.signals || []).filter((signal) =>
        signal.to === activePlayer.id && signal.id > since
      );
      return reply(res, 200, {
        signals,
        cursor: signals.length ? signals[signals.length - 1].id : since
      });
    }

    if (action === "leave") {
      const outcome = await mutateRoom(code, (room) => {
        const currentPlayer = findPlayer(room, claims.sub);
        if (!currentPlayer) {
          return { write: false, status: 403, body: { error: "You are no longer in this room." } };
        }
        const now = Date.now();
        room.players = prunePlayers(room, now).filter((entry) => entry.id !== currentPlayer.id);
        if (room.hostPlayerId === currentPlayer.id) {
          room.hostPlayerId = room.players[0]?.id || "";
          if (room.hostPlayerId) {
            room.netplayId = crypto.randomBytes(6).toString("hex").toUpperCase();
          }
        }
        room.players.forEach((entry) => { entry.ready = false; });
        room.signals = (room.signals || []).filter((signal) =>
          signal.from !== currentPlayer.id &&
          signal.to !== currentPlayer.id &&
          room.players.some((entry) => entry.id === signal.from) &&
          room.players.some((entry) => entry.id === signal.to)
        );
        room.status = room.players.length === 0 ? "closed" : "waiting";
        room.updatedAt = Date.now();
        return { status: 200, body: { ok: true } };
      });
      if (!outcome.found) return reply(res, 404, { error: "Room not found." });
      return reply(res, outcome.outcome.status || 200, outcome.outcome.body);
    }

    return reply(res, 400, { error: "Unknown action." });
  } catch (error) {
    console.error("Crash Bash room API error:", error?.message || error);
    if (error?.name === "BlobPreconditionFailedError") {
      return reply(res, 409, { error: "The room changed at the same time. Refresh and retry." });
    }
    return reply(res, 500, { error: "Lobby request failed. Retry in a moment." });
  }
};
