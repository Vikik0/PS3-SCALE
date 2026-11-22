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

function tokenClaims(req) {
  const claims = verifyGuestToken(bearerToken(req));
  if (!claims) return null;
  return claims;
}

function publicRoom(room) {
  const now = Date.now();
  const active = (room.players || []).filter((p) => now - p.lastSeen <= PLAYER_TTL_MS);
  return {
    code: room.code,
    host: room.players?.[0]?.name || "Guest",
    players: active.length,
    maxPlayers: MAX_PLAYERS,
    createdAt: room.createdAt,
    status: room.status
  };
}

async function listRooms() {
  const page = await list({ prefix: PREFIX, limit: 100 });
  const output = [];
  for (const blob of (page.blobs || []).slice(0, 40)) {
    const code = blob.pathname.slice(PREFIX.length).replace(/\.json$/, "");
    if (!/^[A-Z2-9]{6}$/.test(code)) continue;
    try {
      const record = await readRoom(code);
      if (!record) continue;
      const room = record.room;
      const active = (room.players || []).filter((p) => Date.now() - p.lastSeen <= PLAYER_TTL_MS);
      if (Date.now() - room.updatedAt > ROOM_TTL_MS || active.length === 0) {
        await del(pathFor(code), { access: ACCESS });
        continue;
      }
      if (!room.isPrivate && room.status === "waiting" && active.length < MAX_PLAYERS) {
        output.push(publicRoom(room));
      }
    } catch {}
  }
  return output.sort((a, b) => b.createdAt - a.createdAt);
}

function findPlayer(room, playerId) {
  return (room.players || []).find((p) => p.id === playerId);
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
      if (!claims || claims.room !== code) return reply(res, 401, { error: "A valid room guest token is required." });
      const record = await readRoom(code);
      if (!record) return reply(res, 404, { error: "Room not found." });
      const room = record.room;
      const player = findPlayer(room, claims.sub);
      if (!player) return reply(res, 403, { error: "You are no longer in this room." });
      const active = (room.players || []).filter((p) => Date.now() - p.lastSeen <= PLAYER_TTL_MS);
      return reply(res, 200, {
        room: {
          code: room.code,
          status: room.status,
          isPrivate: room.isPrivate,
          players: active.map(({ id, name, ready }) => ({ id, name, ready })),
          maxPlayers: MAX_PLAYERS,
          createdAt: room.createdAt,
          updatedAt: room.updatedAt
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
        const record = await readRoom(candidate.code);
        if (!record) continue;
        const room = record.room;
        room.players = (room.players || []).filter((p) => Date.now() - p.lastSeen <= PLAYER_TTL_MS);
        if (room.isPrivate || room.status !== "waiting" || room.players.length >= MAX_PLAYERS) continue;
        const playerId = crypto.randomUUID();
        const now = Date.now();
        room.players.push({ id: playerId, name, ready: false, joinedAt: now, lastSeen: now });
        room.updatedAt = now;
        try {
          await saveRoom(candidate.code, room, record.etag);
          return reply(res, 200, {
            room: publicRoom(room),
            player: { id: playerId, name },
            token: signGuestToken({ sub: playerId, room: candidate.code })
          });
        } catch (error) {
          if (error?.name !== "BlobPreconditionFailedError") throw error;
        }
      }
      return reply(res, 404, { error: "No public battle is waiting. Create a public battle first." });
    }

    if (action === "join") {
      const code = String(body.code || "").toUpperCase();
      if (!/^[A-Z2-9]{6}$/.test(code)) return reply(res, 400, { error: "Enter a valid six-character room code." });
      const record = await readRoom(code);
      if (!record) return reply(res, 404, { error: "Room not found." });
      const room = record.room;
      room.players = (room.players || []).filter((p) => Date.now() - p.lastSeen <= PLAYER_TTL_MS);
      if (room.players.length < MAX_PLAYERS) room.status = "waiting";
      if (room.status !== "waiting" || room.players.length >= MAX_PLAYERS) return reply(res, 409, { error: "Room is full or already playing." });
      const playerId = crypto.randomUUID();
      const now = Date.now();
      room.players.push({ id: playerId, name, ready: false, joinedAt: now, lastSeen: now });
      room.updatedAt = now;
      await saveRoom(code, room, record.etag);
      return reply(res, 200, {
        room: publicRoom(room),
        player: { id: playerId, name },
        token: signGuestToken({ sub: playerId, room: code })
      });
    }

    const claims = tokenClaims(req);
    if (!claims) return reply(res, 401, { error: "Guest token missing or expired." });
    const code = claims.room;
    const record = await readRoom(code);
    if (!record) return reply(res, 404, { error: "Room not found." });
    const room = record.room;
    const player = findPlayer(room, claims.sub);
    if (!player) return reply(res, 403, { error: "You are not a member of this room." });
    const now = Date.now();

    if (action === "heartbeat") {
      player.lastSeen = now;
      if (typeof body.ready === "boolean") player.ready = body.ready;
      const activePlayers = room.players.filter((p) => now - p.lastSeen <= PLAYER_TTL_MS);
      room.status = activePlayers.length === MAX_PLAYERS && activePlayers.every((p) => p.ready) ? "ready" : "waiting";
      room.updatedAt = now;
      await saveRoom(code, room, record.etag);
      return reply(res, 200, { ok: true, players: room.players.map(({ id, name, ready }) => ({ id, name, ready })) });
    }

    if (action === "signal") {
      const type = String(body.type || "");
      if (!["offer", "answer", "ice"].includes(type)) return reply(res, 400, { error: "Unsupported signal type." });
      const payload = JSON.stringify(body.payload ?? null);
      if (Buffer.byteLength(payload, "utf8") > 12000) return reply(res, 413, { error: "Signal payload too large." });
      const target = room.players.find((p) => p.id !== player.id);
      if (!target) return reply(res, 409, { error: "The other player has not joined yet." });
      room.signals.push({ id: room.nextSignalId++, from: player.id, to: target.id, type, payload: JSON.parse(payload), createdAt: now });
      room.signals = room.signals.slice(-32);
      room.updatedAt = now;
      await saveRoom(code, room, record.etag);
      return reply(res, 202, { ok: true });
    }

    if (action === "poll") {
      const since = Number.isInteger(body.since) && body.since >= 0 ? body.since : 0;
      const signals = (room.signals || []).filter((s) => s.to === player.id && s.id > since);
      return reply(res, 200, { signals, cursor: signals.length ? signals[signals.length - 1].id : since });
    }

    if (action === "leave") {
      room.players = room.players.filter((p) => p.id !== player.id);
      room.signals = (room.signals || []).filter((s) => s.from !== player.id && s.to !== player.id);
      room.status = "waiting";
      if (room.players.length === 0) await del(pathFor(code), { access: ACCESS });
      else {
        room.updatedAt = now;
        await saveRoom(code, room, record.etag);
      }
      return reply(res, 200, { ok: true });
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
