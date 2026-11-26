"use strict";

const crypto = require("node:crypto");
const { get, put, list, del } = require("@vercel/blob");
const { verifyGuestToken, signGuestToken, cleanDisplayName, bearerToken } = require("./_lib/security");
const { lookupRetroArchHost } = require("./_lib/netplay");

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
function pathFor(code) { return PREFIX + code + ".json"; }
async function readRoom(code) {
  const result = await get(pathFor(code), { access: ACCESS });
  if (!result || result.statusCode !== 200 || !result.stream) return null;
  return { room: JSON.parse(await new Response(result.stream).text()), etag: result.blob.etag };
}
async function createRoom(code, room) {
  await put(pathFor(code), JSON.stringify(room), { access: ACCESS, addRandomSuffix: false, contentType: "application/json" });
}
async function saveRoom(code, room, expectedEtag) {
  if (!expectedEtag) throw new Error("Missing room version for conditional write");
  await put(pathFor(code), JSON.stringify(room), { access: ACCESS, allowOverwrite: true, ifMatch: expectedEtag, contentType: "application/json" });
}
function tokenClaims(req) { return verifyGuestToken(bearerToken(req)); }
function activePlayers(room, now = Date.now()) {
  return (room.players || []).filter((player) => {
    const lastSeen = Number(player.lastSeen);
    return Number.isFinite(lastSeen) && lastSeen <= now && now - lastSeen <= PLAYER_TTL_MS;
  });
}
function discState(players) {
  const selected = players.map((player) => String(player.discFingerprint || "")).filter((value) => /^[A-F0-9]{16}$/.test(value));
  const distinct = new Set(selected);
  return {
    count: selected.length,
    match: players.length >= 2 && selected.length === players.length && distinct.size === 1,
    mismatch: distinct.size > 1
  };
}
function roomReady(players) { return players.length >= 2 && players.every((player) => player.ready === true); }
function roomSummary(room, now = Date.now()) {
  const players = activePlayers(room, now);
  const discs = discState(players);
  return {
    code: room.code,
    status: roomReady(players) ? "ready" : "waiting",
    isPrivate: Boolean(room.isPrivate),
    hostPlayerId: room.gameStarted
      ? (room.hostPlayerId || "")
      : (players.find((player) => player.id === room.hostPlayerId)?.id || players[0]?.id || ""),
    netplayId: room.netplayId || "",
    gameStarted: Boolean(room.gameStarted),
    discSelectionCount: discs.count,
    discMatch: discs.match,
    discMismatch: discs.mismatch,
    players: players.map(({ id, name, ready, discFingerprint }) => ({ id, name, ready: Boolean(ready), discFingerprint: discFingerprint || "" })),
    maxPlayers: MAX_PLAYERS
  };
}
function publicRoom(room, now = Date.now()) {
  const players = activePlayers(room, now);
  const host = players.find((player) => player.id === room.hostPlayerId) || players[0];
  return { code: room.code, host: host?.name || "Guest", players: players.length, maxPlayers: MAX_PLAYERS, createdAt: room.createdAt, status: roomReady(players) ? "ready" : "waiting" };
}
function invalidateMatch(room, { clearDisc = false, clearReady = false } = {}) {
  room.gameStarted = false;
  room.gameStartedAt = 0;
  room.netplayId = "";
  for (const player of room.players || []) {
    if (clearDisc) player.discFingerprint = "";
    if (clearReady) player.ready = false;
  }
}
function reconcileRoom(room, now = Date.now()) {
  if (room.gameStarted) return false;
  const previous = Array.isArray(room.players) ? room.players : [];
  const active = activePlayers(room, now);
  const membershipChanged = previous.length !== active.length ||
    previous.some((player, index) => player.id !== active[index]?.id);
  room.players = active;
  const hostMissing = Boolean(room.hostPlayerId) &&
    !active.some((player) => player.id === room.hostPlayerId);
  const hostUnset = !room.hostPlayerId && active.length > 0;
  if (hostMissing || hostUnset) room.hostPlayerId = active[0]?.id || "";
  const changed = membershipChanged || hostMissing || hostUnset;
  if (changed) {
    invalidateMatch(room, { clearDisc: true, clearReady: true });
    room.updatedAt = now;
  }
  return changed;
}
function findPlayer(room, playerId) { return (room.players || []).find((player) => player.id === playerId); }

async function listRooms() {
  const output = [];
  const seen = new Set();
  let cursor = null;
  do {
    const options = { prefix: PREFIX, limit: 100 };
    if (cursor) options.cursor = cursor;
    const page = await list(options);
    const blobs = page.blobs || [];
    for (let offset = 0; offset < blobs.length; offset += 20) {
      const batch = await Promise.all(blobs.slice(offset, offset + 20).map(async (blob) => {
        const code = blob.pathname.slice(PREFIX.length).replace(/\.json$/, "");
        if (!/^[A-Z2-9]{6}$/.test(code) || seen.has(code)) return null;
        seen.add(code);
        try {
          const record = await readRoom(code);
          if (!record) return null;
          const room = record.room;
          const now = Date.now();
          const players = activePlayers(room, now);
          const updatedAt = Number(room.updatedAt);
          if (!Number.isFinite(updatedAt) || now - updatedAt > ROOM_TTL_MS || players.length === 0) {
            await del(pathFor(code), { access: ACCESS }).catch(() => {});
            return null;
          }
          if (!room.isPrivate && !room.gameStarted && !roomReady(players) && players.length < MAX_PLAYERS) return publicRoom(room, now);
        } catch {}
        return null;
      }));
      output.push(...batch.filter(Boolean));
    }
    cursor = page.cursor || null;
  } while (cursor);
  return output.sort((a, b) => b.createdAt - a.createdAt);
}

module.exports = async function handler(req, res) {
  if (!process.env.BLOB_READ_WRITE_TOKEN || !process.env.CB_TOKEN_SECRET) return reply(res, 503, { error: "Lobby storage is not configured yet." });
  try {
    if (req.method === "GET") {
      const code = String(req.query?.code || "").toUpperCase();
      if (!code) return reply(res, 200, { rooms: await listRooms() });
      if (!/^[A-Z2-9]{6}$/.test(code)) return reply(res, 400, { error: "Invalid room code." });
      const claims = tokenClaims(req);
      if (!claims || claims.room !== code) return reply(res, 401, { error: "A valid room guest token is required." });
      const record = await readRoom(code);
      if (!record) return reply(res, 404, { error: "Room not found." });
      const now = Date.now();
      const player = findPlayer(record.room, claims.sub);
      if (!player || now - Number(player.lastSeen) > PLAYER_TTL_MS) return reply(res, 403, { error: "Your room seat expired. Join the room again." });
      return reply(res, 200, { room: roomSummary(record.room, now) });
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
          code, status: "waiting", isPrivate: Boolean(body.isPrivate), maxPlayers: MAX_PLAYERS,
          createdAt: now, updatedAt: now, hostPlayerId: playerId, netplayId: "",
          gameStarted: false, gameStartedAt: 0,
          players: [{ id: playerId, name, ready: false, discFingerprint: "", joinedAt: now, lastSeen: now }],
          signals: [], nextSignalId: 1
        };
        try {
          await createRoom(code, room);
          return reply(res, 201, { room: publicRoom(room, now), player: { id: playerId, name }, token: signGuestToken({ sub: playerId, room: code }) });
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
        const now = Date.now();
        if (room.gameStarted) continue;
        reconcileRoom(room, now);
        if (room.isPrivate || roomReady(room.players) || room.players.length >= MAX_PLAYERS) continue;
        const playerId = crypto.randomUUID();
        room.players.push({ id: playerId, name, ready: false, discFingerprint: "", joinedAt: now, lastSeen: now });
        invalidateMatch(room, { clearDisc: true, clearReady: true });
        room.updatedAt = now;
        try {
          await saveRoom(candidate.code, room, record.etag);
          return reply(res, 200, { room: publicRoom(room, now), player: { id: playerId, name }, token: signGuestToken({ sub: playerId, room: candidate.code }) });
        } catch (error) {
          if (error?.name !== "BlobPreconditionFailedError") throw error;
        }
      }
      return reply(res, 404, { error: "No public battle is waiting. Create a public battle first." });
    }

    if (action === "join") {
      const code = String(body.code || "").toUpperCase();
      if (!/^[A-Z2-9]{6}$/.test(code)) return reply(res, 400, { error: "Enter a valid six-character room code." });
      for (let attempt = 0; attempt < 3; attempt += 1) {
        const record = await readRoom(code);
        if (!record) return reply(res, 404, { error: "Room not found." });
        const room = record.room;
        const now = Date.now();
        if (room.gameStarted) return reply(res, 409, { error: "Room has already started the match." });
        reconcileRoom(room, now);
        if (room.players.length >= MAX_PLAYERS) return reply(res, 409, { error: "Room is full." });
        const playerId = crypto.randomUUID();
        room.players.push({ id: playerId, name, ready: false, discFingerprint: "", joinedAt: now, lastSeen: now });
        invalidateMatch(room, { clearDisc: true, clearReady: true });
        room.updatedAt = now;
        try {
          await saveRoom(code, room, record.etag);
          return reply(res, 200, { room: publicRoom(room, now), player: { id: playerId, name }, token: signGuestToken({ sub: playerId, room: code }) });
        } catch (error) {
          if (error?.name !== "BlobPreconditionFailedError" || attempt === 2) throw error;
        }
      }
      return reply(res, 409, { error: "Room changed while joining. Refresh and retry." });
    }

    const claims = tokenClaims(req);
    if (!claims) return reply(res, 401, { error: "Guest token missing or expired." });
    const code = claims.room;
    const record = await readRoom(code);
    if (!record) return reply(res, 404, { error: "Room not found." });
    const room = record.room;
    const now = Date.now();
    let player = findPlayer(room, claims.sub);
    if (!player || now - Number(player.lastSeen) > PLAYER_TTL_MS) return reply(res, 403, { error: "Your room seat expired. Join the room again." });
    if (!room.gameStarted && action !== "poll" && action !== "netplay") reconcileRoom(room, now);
    player = findPlayer(room, claims.sub);
    if (!player) return reply(res, 403, { error: "Your room seat expired. Join the room again." });

    if (action === "heartbeat" || action === "set_ready") {
      if (action === "set_ready" && room.gameStarted) return reply(res, 409, { error: "The match has already started; READY is locked." });
      if (action === "set_ready" && typeof body.ready !== "boolean") return reply(res, 400, { error: "READY state must be true or false." });
      player.lastSeen = now;
      if (!room.gameStarted && typeof body.ready === "boolean" && player.ready !== body.ready) {
        player.ready = body.ready;
        invalidateMatch(room, { clearDisc: true });
      }
      room.updatedAt = now;
      const summary = roomSummary(room, now);
      await saveRoom(code, room, record.etag);
      return reply(res, 200, { ok: true, ...summary });
    }

    if (action === "set_disc") {
      const fingerprint = String(body.fingerprint || "");
      if (!/^[A-F0-9]{16}$/.test(fingerprint)) return reply(res, 400, { error: "Disc fingerprint must be 16 uppercase hexadecimal characters." });
      if (room.gameStarted) return reply(res, 409, { error: "Match has already started." });
      const players = activePlayers(room, now);
      if (!roomReady(players) || !player.ready) return reply(res, 409, { error: "Every active player must be READY before disc confirmation." });
      player.lastSeen = now;
      player.discFingerprint = fingerprint;
      invalidateMatch(room);
      room.updatedAt = now;
      await saveRoom(code, room, record.etag);
      return reply(res, 200, { ok: true });
    }

    if (action === "clear_disc") {
      if (room.gameStarted) return reply(res, 409, { error: "Match has already started; disc confirmation is locked." });
      player.lastSeen = now;
      player.discFingerprint = "";
      invalidateMatch(room);
      room.updatedAt = now;
      await saveRoom(code, room, record.etag);
      return reply(res, 200, { ok: true });
    }

    if (action === "game_start") {
      const players = activePlayers(room, now);
      if (player.id !== room.hostPlayerId) return reply(res, 403, { error: "Only the room host can start the match." });
      if (!roomReady(players)) return reply(res, 409, { error: "Every active player must be READY before launch." });
      const discs = discState(players);
      if (discs.mismatch) return reply(res, 409, { error: "Disc fingerprints do not match." });
      if (!discs.match) return reply(res, 409, { error: "Every active player must confirm the same disc before launch." });
      if (!room.gameStarted) {
        room.gameStarted = true;
        room.gameStartedAt = now;
        room.netplayId = crypto.randomBytes(8).toString("hex").toUpperCase();
        room.updatedAt = now;
        await saveRoom(code, room, record.etag);
      }
      return reply(res, 200, { ok: true, room: roomSummary(room, now) });
    }

    if (action === "game_start_cancel") {
      if (player.id !== room.hostPlayerId) return reply(res, 403, { error: "Only the room host can cancel the launch." });
      if (!room.gameStarted) return reply(res, 409, { error: "There is no active launch lock to cancel." });
      if (now - Number(room.gameStartedAt || 0) > 30000) return reply(res, 409, { error: "The 30-second launch recovery window has expired; the match remains locked." });
      invalidateMatch(room);
      room.updatedAt = now;
      await saveRoom(code, room, record.etag);
      return reply(res, 200, { ok: true });
    }

    if (action === "netplay") {
      const players = activePlayers(room, now);
      if (!roomReady(players)) return reply(res, 409, { error: "Every active player must be READY before Netplay discovery." });
      const discs = discState(players);
      if (discs.mismatch) return reply(res, 200, { ready: false, state: "disc_mismatch" });
      if (!discs.match) return reply(res, 200, { ready: false, state: "waiting_for_disc_selection" });
      if (!room.gameStarted || !room.netplayId || player.id === room.hostPlayerId) return reply(res, 200, { ready: false, state: "waiting_for_host_start" });
      try {
        const nickname = "CB-" + String(room.netplayId).slice(0, 16) + "-HOST";
        return reply(res, 200, await lookupRetroArchHost(nickname, { timeoutMs: 5000 }));
      } catch (error) {
        console.warn("RetroArch lobby lookup failed:", error?.message || error);
        return reply(res, 200, { ready: false, state: "waiting_for_host_start" });
      }
    }

    if (action === "signal") {
      const type = String(body.type || "");
      if (!["offer", "answer", "ice"].includes(type)) return reply(res, 400, { error: "Unsupported signal type." });
      const payload = JSON.stringify(body.payload ?? null);
      if (Buffer.byteLength(payload, "utf8") > 12000) return reply(res, 413, { error: "Signal payload too large." });
      const targetPlayerId = String(body.toPlayerId || "");
      if (!targetPlayerId) return reply(res, 400, { error: "A target player ID is required for room signaling." });
      const target = room.players.find((candidate) => candidate.id === targetPlayerId && candidate.id !== player.id && now - candidate.lastSeen <= PLAYER_TTL_MS);
      if (!target) return reply(res, 404, { error: "Target player is not an active member of this room." });
      room.signals = room.signals || [];
      room.signals.push({ id: room.nextSignalId++, from: player.id, to: target.id, type, payload: JSON.parse(payload), createdAt: now });
      room.signals = room.signals.slice(-32);
      room.updatedAt = now;
      await saveRoom(code, room, record.etag);
      return reply(res, 202, { ok: true });
    }

    if (action === "poll") {
      const since = Number.isInteger(body.since) && body.since >= 0 ? body.since : 0;
      const signals = (room.signals || []).filter((signal) => signal.to === player.id && signal.id > since);
      return reply(res, 200, { signals, cursor: signals.length ? signals[signals.length - 1].id : since });
    }

    if (action === "leave") {
      if (room.gameStarted) return reply(res, 409, { error: "The match is locked. Close the game before leaving its room." });
      const wasHost = player.id === room.hostPlayerId;
      room.players = (room.players || []).filter((candidate) => candidate.id !== player.id);
      room.signals = (room.signals || []).filter((signal) => signal.from !== player.id && signal.to !== player.id);
      if (room.players.length === 0) {
        await del(pathFor(code), { access: ACCESS });
      } else {
        if (wasHost) room.hostPlayerId = room.players[0].id;
        invalidateMatch(room, { clearDisc: true, clearReady: true });
        room.updatedAt = now;
        await saveRoom(code, room, record.etag);
      }
      return reply(res, 200, { ok: true });
    }

    return reply(res, 400, { error: "Unknown action." });
  } catch (error) {
    console.error("Crash Bash room API error:", error?.message || error);
    if (error?.name === "BlobPreconditionFailedError") return reply(res, 409, { error: "The room changed at the same time. Refresh and retry." });
    return reply(res, 500, { error: "Lobby request failed. Retry in a moment." });
  }
};
