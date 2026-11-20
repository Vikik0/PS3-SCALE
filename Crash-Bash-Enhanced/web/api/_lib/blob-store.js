"use strict";

const { get, head, list, put, del } = require("@vercel/blob");

const ACCESS = "private";
const ROOM_PREFIX = "cb-rooms/";
const ROOM_TTL_MS = 15 * 60 * 1000;
const PLAYER_TTL_MS = 75 * 1000;

function roomPath(code) {
  return ROOM_PREFIX + code + ".json";
}

async function readJson(pathname) {
  const result = await get(pathname, { access: ACCESS });
  if (!result || result.statusCode !== 200 || !result.stream) return null;
  const text = await new Response(result.stream).text();
  return JSON.parse(text);
}

async function readRoom(code) {
  return readJson(roomPath(code));
}

async function writeRoom(code, room) {
  const pathname = roomPath(code);
  const body = JSON.stringify(room);

  for (let attempt = 0; attempt < 3; attempt += 1) {
    let metadata = null;
    try {
      metadata = await head(pathname, { access: ACCESS });
    } catch {
      metadata = null;
    }

    try {
      const options = metadata
        ? { access: ACCESS, allowOverwrite: true, ifMatch: metadata.etag, contentType: "application/json" }
        : { access: ACCESS, addRandomSuffix: false, contentType: "application/json" };
      await put(pathname, body, options);
      return;
    } catch (error) {
      if (attempt === 2) throw error;
    }
  }
  throw new Error("Could not write room after retries");
}

async function deleteRoom(code) {
  await del(roomPath(code), { access: ACCESS });
}

async function listRoomBlobs() {
  const page = await list({ prefix: ROOM_PREFIX, limit: 100 });
  return page.blobs || [];
}

function isExpired(room, now = Date.now()) {
  if (!room || !Number.isFinite(room.updatedAt)) return true;
  if (now - room.updatedAt > ROOM_TTL_MS) return true;
  const active = (room.players || []).filter(
    (player) => now - player.lastSeen <= PLAYER_TTL_MS
  );
  return active.length === 0;
}

module.exports = {
  readRoom,
  writeRoom,
  deleteRoom,
  listRoomBlobs,
  isExpired,
  roomPath,
  ROOM_TTL_MS,
  PLAYER_TTL_MS
};
