"use strict";

const crypto = require("node:crypto");
const net = require("node:net");
const { get, put, list } = require("@vercel/blob");
const {
  signGuestToken,
  verifyGuestToken,
  cleanDisplayName,
  bearerToken
} = require("./_lib/security");

const ACCESS = "private";
const PREFIX = "cb-rooms/";
const ROOM_TTL_MS = 30 * 60 * 1000;
const PLAYER_TTL_MS = 90 * 1000;
const READY_TTL_MS = 90 * 1000;
const MATCH_TTL_MS = 30 * 60 * 1000;
const MAX_PLAYERS = 4;
const MAX_PUBLIC_ROOMS = 40;
const MAX_ROOM_SCAN = 500;
const CODE_CHARS = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
const CODE_PATTERN = /^[ABCDEFGHJKLMNPQRSTUVWXYZ23456789]{6}$/;
const FINGERPRINT_PATTERN = /^[A-F0-9]{16}$/;
const NETPLAY_LOBBY_URL = "https://lobby.libretro.com/list/";
const NETPLAY_LOBBY_CACHE_MS = 1200;
const NETPLAY_ENTRY_MAX_AGE_MS = 120000;

let lobbyCache = { expiresAt: 0, entries: null };

function reply(res, status, data) {
  res.setHeader("Cache-Control", "no-store");
  res.setHeader("X-Content-Type-Options", "nosniff");
  return res.status(status).json(data);
}

function makeCode() {
  const bytes = crypto.randomBytes(6);
  let result = "";
  for (let i = 0; i < 6; i += 1) {
    result += CODE_CHARS[bytes[i] % CODE_CHARS.length];
  }
  return result;
}

function makeNetplayId() {
  return crypto.randomBytes(9).toString("base64url");
}

function pathFor(code) {
  return PREFIX + code + ".json";
}

async function readRoom(code) {
  const result = await get(pathFor(code), { access: ACCESS });
  if (!result || result.statusCode !== 200 || !result.stream) return null;
  const room = JSON.parse(await new Response(result.stream).text());
  const etag = result.blob && result.blob.etag;
  if (!etag) throw new Error("Room storage did not return a version tag");
  return { room, etag };
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

function tokenClaims(req) {
  return verifyGuestToken(bearerToken(req));
}

function findPlayer(room, playerId) {
  return (room.players || []).find((player) => player.id === playerId);
}

function resetReadiness(players) {
  for (const player of players || []) {
    player.ready = false;
    player.readyUntil = 0;
    player.discFingerprint = "";
  }
}

function promoteHost(room) {
  if (!room.hostPlayerId || !findPlayer(room, room.hostPlayerId)) {
    room.hostPlayerId = room.players && room.players.length
      ? room.players[0].id
      : "";
  }
}

function activePlayers(room, now) {
  if (room.gameStarted && Number(room.matchExpiresAt) > now) {
    return room.players || [];
  }
  return (room.players || []).filter((player) =>
    Number.isFinite(player.lastSeen) && now - player.lastSeen <= PLAYER_TTL_MS
  );
}

function recomputeStatus(room, now) {
  if (room.gameStarted && Number(room.matchExpiresAt) > now) {
    room.status = "started";
    return;
  }
  const players = activePlayers(room, now);
  room.status = players.length >= 2 && players.every((player) => player.ready)
    ? "ready"
    : players.length > 0 ? "waiting" : "closed";
}

function normalizeRoom(room, now) {
  let changed = false;
  if (!Array.isArray(room.players)) {
    room.players = [];
    changed = true;
  }

  if (room.gameStarted && Number(room.matchExpiresAt) > now) {
    if (room.status !== "started") {
      room.status = "started";
      changed = true;
    }
    return changed;
  }

  if (room.gameStarted) {
    room.gameStarted = false;
    room.matchExpiresAt = 0;
    room.netplayId = "";
    changed = true;
  }

  const previousCount = room.players.length;
  const active = room.players.filter((player) =>
    Number.isFinite(player.lastSeen) && now - player.lastSeen <= PLAYER_TTL_MS
  );
  if (active.length !== previousCount) {
    room.players = active;
    resetReadiness(room.players);
    changed = true;
    promoteHost(room);
  }

  for (const player of room.players) {
    if (typeof player.ready !== "boolean") {
      player.ready = false;
      changed = true;
    }
    if (typeof player.discFingerprint !== "string") {
      player.discFingerprint = "";
      changed = true;
    }
    if (player.ready && (!Number.isFinite(player.readyUntil) || player.readyUntil <= now)) {
      player.ready = false;
      player.readyUntil = 0;
      player.discFingerprint = "";
      changed = true;
    }
  }

  const oldHost = room.hostPlayerId || "";
  promoteHost(room);
  if ((room.hostPlayerId || "") !== oldHost) changed = true;

  const oldStatus = room.status;
  recomputeStatus(room, now);
  if (room.status !== oldStatus) changed = true;

  if (changed) room.updatedAt = now;
  return changed;
}

function discState(room, now) {
  const players = activePlayers(room, now);
  const selected = players
    .map((player) => player.discFingerprint || "")
    .filter((fingerprint) => FINGERPRINT_PATTERN.test(fingerprint));
  const distinct = new Set(selected);
  return {
    discSelectionCount: selected.length,
    discMatch: players.length >= 2 &&
      selected.length === players.length &&
      distinct.size === 1,
    discMismatch: distinct.size > 1
  };
}

function roomSnapshot(room, now) {
  const players = activePlayers(room, now);
  const discs = discState(room, now);
  return {
    code: room.code,
    status: room.status,
    isPrivate: Boolean(room.isPrivate),
    hostPlayerId: room.hostPlayerId || "",
    netplayId: room.netplayId || "",
    gameStarted: Boolean(room.gameStarted && Number(room.matchExpiresAt) > now),
    discSelectionCount: discs.discSelectionCount,
    discMatch: discs.discMatch,
    discMismatch: discs.discMismatch,
    players: players.map((player) => ({
      id: player.id,
      name: player.name,
      ready: Boolean(player.ready),
      discFingerprint: player.discFingerprint || ""
    })),
    maxPlayers: MAX_PLAYERS,
    createdAt: room.createdAt,
    updatedAt: room.updatedAt
  };
}

function publicRoom(room, now) {
  const players = activePlayers(room, now);
  const host = findPlayer(room, room.hostPlayerId) || players[0];
  return {
    code: room.code,
    host: host ? host.name : "Guest",
    players: players.length,
    maxPlayers: MAX_PLAYERS,
    createdAt: room.createdAt,
    status: room.status
  };
}

async function loadRoom(code) {
  const record = await readRoom(code);
  if (!record) return null;
  const now = Date.now();
  if (!Number.isFinite(record.room.updatedAt) ||
      now - record.room.updatedAt > ROOM_TTL_MS) {
    return null;
  }
  const changed = normalizeRoom(record.room, now);
  if (record.room.players.length === 0) return null;
  if (changed) {
    await saveRoom(code, record.room, record.etag);
    return readRoom(code);
  }
  return record;
}

async function listRooms() {
  const output = [];
  let cursor;
  let scanned = 0;

  do {
    const page = await list({
      prefix: PREFIX,
      limit: 100,
      ...(cursor ? { cursor } : {})
    });
    const blobs = page.blobs || [];

    for (let offset = 0; offset < blobs.length && scanned < MAX_ROOM_SCAN; offset += 10) {
      const batch = blobs.slice(offset, offset + 10);
      const records = await Promise.all(batch.map(async (blob) => {
        const code = blob.pathname.slice(PREFIX.length).replace(/\.json$/, "");
        if (!CODE_PATTERN.test(code)) return null;
        try {
          const record = await loadRoom(code);
          if (!record || record.room.isPrivate || record.room.gameStarted) return null;
          const now = Date.now();
          const players = activePlayers(record.room, now);
          if (players.length === 0 || players.length >= MAX_PLAYERS) return null;
          return publicRoom(record.room, now);
        } catch (error) {
          if (error && error.name === "BlobPreconditionFailedError") return null;
          throw error;
        }
      }));
      for (const room of records) {
        if (room) output.push(room);
      }
      scanned += batch.length;
      if (output.length >= MAX_PUBLIC_ROOMS || scanned >= MAX_ROOM_SCAN) break;
    }

    if (output.length >= MAX_PUBLIC_ROOMS || scanned >= MAX_ROOM_SCAN || !page.hasMore) {
      break;
    }
    cursor = page.cursor;
  } while (cursor);

  return output
    .sort((a, b) => b.createdAt - a.createdAt)
    .slice(0, MAX_PUBLIC_ROOMS);
}

function validPort(value) {
  const port = Number(value);
  return Number.isInteger(port) && port >= 1 && port <= 65535 ? port : 0;
}

function validHost(value) {
  if (typeof value !== "string" || value.length < 1 || value.length > 253) return false;
  if (net.isIP(value)) return true;
  if (!/^[A-Za-z0-9.-]+$/.test(value)) return false;
  if (value.startsWith(".") || value.endsWith(".") || value.includes("..")) return false;
  return value.split(".").every((part) =>
    part.length > 0 && part.length <= 63 &&
    /^[A-Za-z0-9](?:[A-Za-z0-9-]*[A-Za-z0-9])?$/.test(part)
  );
}

async function getLobbyEntries() {
  const now = Date.now();
  if (lobbyCache.entries && lobbyCache.expiresAt > now) {
    return lobbyCache.entries;
  }

  const response = await fetch(NETPLAY_LOBBY_URL, {
    method: "GET",
    headers: { Accept: "application/json" },
    cache: "no-store",
    signal: AbortSignal.timeout(3000)
  });
  if (!response.ok) throw new Error("RetroArch lobby returned HTTP " + response.status);
  const payload = await response.json();
  const entries = Array.isArray(payload) ? payload : [];
  lobbyCache = { entries, expiresAt: now + NETPLAY_LOBBY_CACHE_MS };
  return entries;
}

async function findNetplayEndpoint(netplayId) {
  const nickname = "CB-" + String(netplayId || "").slice(0, 12) + "-HOST";
  if (nickname.length !== 20 || !/^CB-[A-Za-z0-9_-]{12}-HOST$/.test(nickname)) return null;

  const entries = await getLobbyEntries();
  const entry = entries.find((item) =>
    item && item.fields && item.fields.username === nickname
  );
  if (!entry) return null;

  const fields = entry.fields;
  const now = Date.now();
  const updatedAt = Date.parse(fields.updated || "");
  if (!Number.isFinite(updatedAt) ||
      updatedAt > now + 30000 ||
      now - updatedAt > NETPLAY_ENTRY_MAX_AGE_MS ||
      fields.is_retroarch !== true) {
    return null;
  }

  if (fields.has_password) return { passwordRequired: true };
  const mitmSession = typeof fields.mitm_session === "string"
    ? fields.mitm_session
    : "";
  const relayHost = typeof fields.mitm_ip === "string" ? fields.mitm_ip : "";
  const relayPort = validPort(fields.mitm_port);

  if (mitmSession && validHost(relayHost) && relayPort && fields.connectable === true) {
    return {
      address: relayHost,
      port: relayPort,
      mode: "relay",
      mitmSession,
      gameName: String(fields.game_name || "Crash Bash").slice(0, 99),
      coreName: String(fields.core_name || "PCSX-ReARMed").slice(0, 99),
      coreVersion: String(fields.core_version || "").slice(0, 99)
    };
  }

  const directHost = typeof fields.ip === "string" ? fields.ip : "";
  const directPort = validPort(fields.port);
  if (fields.connectable === true && validHost(directHost) && directPort) {
    return {
      address: directHost,
      port: directPort,
      mode: "direct",
      mitmSession: "",
      gameName: String(fields.game_name || "Crash Bash").slice(0, 99),
      coreName: String(fields.core_name || "PCSX-ReARMed").slice(0, 99),
      coreVersion: String(fields.core_version || "").slice(0, 99)
    };
  }

  return null;
}

function resetRoomForMembershipChange(room, now) {
  resetReadiness(room.players);
  room.updatedAt = now;
  recomputeStatus(room, now);
}

function makePlayer(name, now) {
  return {
    id: crypto.randomUUID(),
    name,
    ready: false,
    readyUntil: 0,
    discFingerprint: "",
    joinedAt: now,
    lastSeen: now
  };
}

module.exports = async function handler(req, res) {
  const tokenSecret = process.env.CB_TOKEN_SECRET_V2 || process.env.CB_TOKEN_SECRET;
  if (!process.env.BLOB_READ_WRITE_TOKEN || !tokenSecret || tokenSecret.length < 40) {
    return reply(res, 503, { error: "Lobby storage or guest-token signing is not configured." });
  }

  try {
    if (req.method === "GET") {
      const code = String(req.query && req.query.code || "").toUpperCase();
      if (!code) return reply(res, 200, { rooms: await listRooms() });
      if (!CODE_PATTERN.test(code)) return reply(res, 400, { error: "Invalid room code." });

      const claims = tokenClaims(req);
      if (!claims || claims.room !== code) {
        return reply(res, 401, { error: "A valid room guest token is required." });
      }

      const record = await loadRoom(code);
      if (!record) return reply(res, 404, { error: "Room not found or expired." });
      const player = findPlayer(record.room, claims.sub);
      if (!player) return reply(res, 403, { error: "You are no longer in this room." });

      return reply(res, 200, { room: roomSnapshot(record.room, Date.now()) });
    }

    if (req.method !== "POST") {
      res.setHeader("Allow", "GET, POST");
      return reply(res, 405, { error: "Method not allowed." });
    }

    let body = req.body && typeof req.body === "object" ? req.body : {};
    if (typeof req.body === "string") {
      try {
        body = JSON.parse(req.body);
      } catch {
        return reply(res, 400, { error: "Invalid JSON request body." });
      }
    }
    const action = String(body.action || "");
    const name = cleanDisplayName(body.displayName);

    if (action === "create") {
      for (let attempt = 0; attempt < 5; attempt += 1) {
        const code = makeCode();
        const player = makePlayer(name, Date.now());
        const room = {
          code,
          status: "waiting",
          isPrivate: Boolean(body.isPrivate),
          maxPlayers: MAX_PLAYERS,
          createdAt: Date.now(),
          updatedAt: Date.now(),
          hostPlayerId: player.id,
          netplayId: "",
          gameStarted: false,
          matchExpiresAt: 0,
          players: [player],
          signals: [],
          nextSignalId: 1
        };
        try {
          await createRoom(code, room);
          return reply(res, 201, {
            room: publicRoom(room, Date.now()),
            player: { id: player.id, name: player.name },
            token: signGuestToken({ sub: player.id, room: code })
          });
        } catch (error) {
          if (error && error.name === "BlobAlreadyExistsError" && attempt < 4) continue;
          if (attempt === 4) throw error;
          throw error;
        }
      }
    }

    if (action === "any") {
      const candidates = await listRooms();
      for (const candidate of candidates) {
        const record = await loadRoom(candidate.code);
        if (!record) continue;
        const room = record.room;
        const now = Date.now();
        if (room.isPrivate || room.gameStarted || room.players.length >= MAX_PLAYERS) continue;

        const player = makePlayer(name, now);
        room.players.push(player);
        if (!room.hostPlayerId) room.hostPlayerId = player.id;
        resetRoomForMembershipChange(room, now);
        try {
          await saveRoom(candidate.code, room, record.etag);
          return reply(res, 200, {
            room: publicRoom(room, now),
            player: { id: player.id, name: player.name },
            token: signGuestToken({ sub: player.id, room: candidate.code })
          });
        } catch (error) {
          if (error && error.name === "BlobPreconditionFailedError") continue;
          throw error;
        }
      }
      return reply(res, 404, { error: "No public battle is waiting. Create a public battle first." });
    }

    if (action === "join") {
      const code = String(body.code || "").toUpperCase();
      if (!CODE_PATTERN.test(code)) {
        return reply(res, 400, { error: "Enter a valid six-character room code." });
      }
      const record = await loadRoom(code);
      if (!record) return reply(res, 404, { error: "Room not found or expired." });
      const room = record.room;
      const now = Date.now();
      if (room.gameStarted || room.status === "started") {
        return reply(res, 409, { error: "The match has already started. Late joins are disabled." });
      }
      if (room.players.length >= MAX_PLAYERS) {
        return reply(res, 409, { error: "Room is full." });
      }

      const player = makePlayer(name, now);
      room.players.push(player);
      if (!room.hostPlayerId) room.hostPlayerId = player.id;
      resetRoomForMembershipChange(room, now);
      await saveRoom(code, room, record.etag);
      return reply(res, 200, {
        room: publicRoom(room, now),
        player: { id: player.id, name: player.name },
        token: signGuestToken({ sub: player.id, room: code })
      });
    }

    const claims = tokenClaims(req);
    if (!claims) return reply(res, 401, { error: "Guest token missing or expired." });
    const code = claims.room;
    const record = await loadRoom(code);
    if (!record) return reply(res, 404, { error: "Room not found or expired." });
    const room = record.room;
    const player = findPlayer(room, claims.sub);
    if (!player) return reply(res, 403, { error: "You are not a member of this room." });
    const now = Date.now();

    if (action === "heartbeat") {
      player.lastSeen = now;
      room.updatedAt = now;
      recomputeStatus(room, now);
      await saveRoom(code, room, record.etag);
      return reply(res, 200, roomSnapshot(room, now));
    }

    if (action === "set_ready") {
      if (room.gameStarted) return reply(res, 409, { error: "The match has already started." });
      if (typeof body.ready !== "boolean") {
        return reply(res, 400, { error: "READY must be a boolean value." });
      }
      player.ready = body.ready;
      player.readyUntil = body.ready ? now + READY_TTL_MS : 0;
      if (!body.ready) player.discFingerprint = "";
      player.lastSeen = now;
      room.updatedAt = now;
      recomputeStatus(room, now);
      await saveRoom(code, room, record.etag);
      return reply(res, 200, roomSnapshot(room, now));
    }

    if (action === "set_disc") {
      if (room.gameStarted) return reply(res, 409, { error: "The match has already started." });
      const fingerprint = String(body.fingerprint || "");
      if (!FINGERPRINT_PATTERN.test(fingerprint)) {
        return reply(res, 400, { error: "Disc fingerprint must be 16 uppercase hexadecimal characters." });
      }
      if (!player.ready || !Number.isFinite(player.readyUntil) || player.readyUntil <= now) {
        return reply(res, 409, { error: "Mark READY again before confirming the disc." });
      }
      player.discFingerprint = fingerprint;
      player.lastSeen = now;
      room.updatedAt = now;
      await saveRoom(code, room, record.etag);
      const discs = discState(room, now);
      return reply(res, 200, { ok: true, ...discs });
    }

    if (action === "clear_disc") {
      if (room.gameStarted) return reply(res, 409, { error: "The match has already started." });
      player.discFingerprint = "";
      player.lastSeen = now;
      room.updatedAt = now;
      await saveRoom(code, room, record.etag);
      return reply(res, 200, { ok: true });
    }

    if (action === "game_start") {
      if (room.gameStarted && room.hostPlayerId === player.id) {
        return reply(res, 200, { ok: true, alreadyStarted: true });
      }
      if (room.hostPlayerId !== player.id) {
        return reply(res, 403, { error: "Only the room host can start the match." });
      }
      const players = activePlayers(room, now);
      const discs = discState(room, now);
      if (players.length < 2 || !players.every((activePlayer) =>
        activePlayer.ready &&
        Number.isFinite(activePlayer.readyUntil) &&
        activePlayer.readyUntil > now
      )) {
        return reply(res, 409, { error: "At least two active players must all be READY." });
      }
      if (discs.discMismatch) {
        return reply(res, 409, { error: "Players selected different disc images." });
      }
      if (!discs.discMatch) {
        return reply(res, 409, { error: "Every active player must confirm the same disc before launch." });
      }

      room.gameStarted = true;
      room.status = "started";
      room.netplayId = makeNetplayId();
      room.matchExpiresAt = now + MATCH_TTL_MS;
      room.updatedAt = now;
      for (const activePlayer of players) activePlayer.lastSeen = now;
      await saveRoom(code, room, record.etag);
      return reply(res, 200, { ok: true });
    }

    if (action === "game_start_cancel") {
      if (room.hostPlayerId !== player.id) {
        return reply(res, 403, { error: "Only the room host can unlock the match." });
      }
      if (!room.gameStarted) return reply(res, 200, { ok: true, alreadyUnlocked: true });
      room.gameStarted = false;
      room.matchExpiresAt = 0;
      room.netplayId = "";
      room.updatedAt = now;
      normalizeRoom(room, now);
      await saveRoom(code, room, record.etag);
      return reply(res, 200, { ok: true });
    }

    if (action === "netplay") {
      const discs = discState(room, now);
      if (discs.discMismatch) {
        return reply(res, 200, { ready: false, state: "disc_mismatch" });
      }
      if (!room.gameStarted || Number(room.matchExpiresAt) <= now) {
        return reply(res, 200, {
          ready: false,
          state: discs.discMatch ? "waiting_for_host" : "waiting_for_disc_selection"
        });
      }
      if (room.hostPlayerId === player.id) {
        return reply(res, 200, { ready: false, state: "host" });
      }

      try {
        const endpoint = await findNetplayEndpoint(room.netplayId);
        if (endpoint && endpoint.passwordRequired) {
          return reply(res, 200, { ready: false, state: "host_password_required" });
        }
        if (!endpoint) return reply(res, 200, { ready: false, state: "waiting_for_host" });
        return reply(res, 200, {
          ready: true,
          state: "ready",
          address: endpoint.address,
          port: endpoint.port,
          mode: endpoint.mode,
          mitmSession: endpoint.mitmSession,
          gameName: endpoint.gameName,
          coreName: endpoint.coreName,
          coreVersion: endpoint.coreVersion
        });
      } catch (error) {
        console.error("Crash Bash Netplay lobby lookup failed:", error && error.message || error);
        return reply(res, 200, { ready: false, state: "lobby_unavailable" });
      }
    }

    if (action === "signal") {
      const type = String(body.type || "");
      if (!["offer", "answer", "ice"].includes(type)) {
        return reply(res, 400, { error: "Unsupported signal type." });
      }
      const payload = JSON.stringify(body.payload === undefined ? null : body.payload);
      if (Buffer.byteLength(payload, "utf8") > 12000) {
        return reply(res, 413, { error: "Signal payload too large." });
      }
      const targetPlayerId = String(body.toPlayerId || "");
      const target = room.players.find((candidate) =>
        candidate.id === targetPlayerId &&
        candidate.id !== player.id &&
        activePlayers(room, now).some((activePlayer) => activePlayer.id === candidate.id)
      );
      if (!target) {
        return reply(res, 404, { error: "Target player is not an active member of this room." });
      }
      room.signals = Array.isArray(room.signals) ? room.signals : [];
      room.nextSignalId = Number.isInteger(room.nextSignalId) ? room.nextSignalId : 1;
      room.signals.push({
        id: room.nextSignalId++,
        from: player.id,
        to: target.id,
        type,
        payload: JSON.parse(payload),
        createdAt: now
      });
      room.signals = room.signals.slice(-32);
      room.updatedAt = now;
      await saveRoom(code, room, record.etag);
      return reply(res, 202, { ok: true });
    }

    if (action === "poll") {
      const since = Number.isInteger(body.since) && body.since >= 0 ? body.since : 0;
      const signals = (room.signals || []).filter((signal) =>
        signal.to === player.id && signal.id > since
      );
      return reply(res, 200, {
        signals,
        cursor: signals.length ? signals[signals.length - 1].id : since
      });
    }

    if (action === "leave") {
      room.players = room.players.filter((candidate) => candidate.id !== player.id);
      room.signals = (room.signals || []).filter((signal) =>
        signal.from !== player.id && signal.to !== player.id
      );
      if (!room.gameStarted) {
        resetReadiness(room.players);
        promoteHost(room);
        recomputeStatus(room, now);
      } else if (room.players.length === 0) {
        room.status = "closed";
      }
      if (room.players.length === 0) {
        room.status = "closed";
        room.closedAt = now;
      }
      room.updatedAt = now;
      await saveRoom(code, room, record.etag);
      return reply(res, 200, { ok: true });
    }

    return reply(res, 400, { error: "Unknown room action." });
  } catch (error) {
    console.error("Crash Bash room API error:", error && error.message || error);
    if (error && error.name === "BlobPreconditionFailedError") {
      return reply(res, 409, { error: "The room changed at the same time. Refresh and retry." });
    }
    return reply(res, 500, { error: "Lobby request failed. Retry in a moment." });
  }
};
