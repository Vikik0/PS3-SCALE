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
      if (error?.name !== "BlobPreconditionFailedError" || attempt === maxAttempts - 1) throw error;
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
      if (!claims || claims.room !== code) return reply(res, 401, { error: "A valid room guest token is required." });
      const record = await readRoom(code);
    if (!record) return reply(res, 404, { error: "Room not found." });
    const room = record.room;
    const player = findPlayer(room, claims.sub);
    if (!player) return reply(res, 403, { error: "You are not a member of this room." });
    const now = Date.now();

    if (action === "heartbeat") {
      const outcome = await mutateRoom(code, (currentRoom) => {
        const currentPlayer = findPlayer(currentRoom, claims.sub);
        if (!currentPlayer) return { write: false, status: 403, body: { error: "You are no longer in this room." } };
        const now = Date.now();
        currentPlayer.lastSeen = now;
        if (typeof body.ready === "boolean") currentPlayer.ready = body.ready;
        const activePlayers = currentRoom.players.filter((p) => now - p.lastSeen <= PLAYER_TTL_MS);
        currentRoom.status = activePlayers.length === MAX_PLAYERS && activePlayers.every((p) => p.ready) ? "ready" : "waiting";
        currentRoom.updatedAt = now;
        return {
          status: 200,
          body: { ok: true, players: currentRoom.players.map(({ id, name, ready }) => ({ id, name, ready })) }
        };
      });
      if (!outcome.found) return reply(res, 404, { error: "Room not found." });
      return reply(res, outcome.outcome.status || 200, outcome.outcome.body);
    }

    if (action === "signal") {
      const type = String(body.type || "");
      if (!["offer", "answer", "ice"].includes(type)) return reply(res, 400, { error: "Unsupported signal type." });
      const payload = JSON.stringify(body.payload ?? null);
      if (Buffer.byteLength(payload, "utf8") > 12000) return reply(res, 413, { error: "Signal payload too large." });
      const targetPlayerId = String(body.toPlayerId || "");
      if (!targetPlayerId) return reply(res, 400, { error: "A target player ID is required for room signaling." });
      const outcome = await mutateRoom(code, (currentRoom) => {
        const currentPlayer = findPlayer(currentRoom, claims.sub);
        if (!currentPlayer) return { write: false, status: 403, body: { error: "You are no longer in this room." } };
        const now = Date.now();
        const target = currentRoom.players.find((p) =>
          p.id === targetPlayerId &&
          p.id !== currentPlayer.id &&
          now - p.lastSeen <= PLAYER_TTL_MS
        );
        if (!target) return { write: false, status: 404, body: { error: "Target player is not an active member of this room." } };
        currentRoom.signals = currentRoom.signals || [];
        currentRoom.nextSignalId = currentRoom.nextSignalId || 1;
        currentRoom.signals.push({ id: currentRoom.nextSignalId++, from: currentPlayer.id, to: target.id, type, payload: JSON.parse(payload), createdAt: now });
        currentRoom.signals = currentRoom.signals.slice(-32);
        currentRoom.updatedAt = now;
        return { status: 202, body: { ok: true } };
      });
      if (!outcome.found) return reply(res, 404, { error: "Room not found." });
      return reply(res, outcome.outcome.status || 202, outcome.outcome.body);
    }

    if (action === "poll") {
      const since = Number.isInteger(body.since) && body.since >= 0 ? body.since : 0;
      const signals = (room.signals || []).filter((s) => s.to === player.id && s.id > since);
      return reply(res, 200, { signals, cursor: signals.length ? signals[signals.length - 1].id : since });
    }

    if (action === "leave") {
      const outcome = await mutateRoom(code, (currentRoom) => {
        const currentPlayer = findPlayer(currentRoom, claims.sub);
        if (!currentPlayer) return { write: false, status: 403, body: { error: "You are no longer in this room." } };
        currentRoom.players = (currentRoom.players || []).filter((p) => p.id !== currentPlayer.id);
        currentRoom.signals = (currentRoom.signals || []).filter((signal) => signal.from !== currentPlayer.id && signal.to !== currentPlayer.id);
        currentRoom.status = currentRoom.players.length === 0 ? "closed" : "waiting";
        currentRoom.updatedAt = Date.now();
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
