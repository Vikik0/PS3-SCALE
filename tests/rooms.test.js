"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

process.env.BLOB_READ_WRITE_TOKEN = "test-blob-token";
process.env.CB_TOKEN_SECRET_V2 = "test-only-guest-token-secret-with-more-than-forty-characters";

const blobs = new Map();
let etagSequence = 0;
let refreshOnNextRead = "";
const blobMock = {
  async get(pathname) {
    const record = blobs.get(pathname);
    if (!record) return null;
    const responseBody = record.body;
    const responseEtag = record.etag;
    if (refreshOnNextRead === pathname) {
      const refreshed = JSON.parse(record.body);
      refreshed.updatedAt = Date.now();
      refreshed.players = refreshed.players.map((player) => ({ ...player, lastSeen: Date.now() }));
      blobs.set(pathname, { body: JSON.stringify(refreshed), etag: String(++etagSequence) });
      refreshOnNextRead = "";
    }
    return { statusCode: 200, stream: new Response(responseBody).body, blob: { etag: responseEtag } };
  },
  async put(pathname, body, options = {}) {
    const previous = blobs.get(pathname);
    if (options.ifMatch && (!previous || previous.etag !== options.ifMatch)) {
      const error = new Error("Blob changed");
      error.name = "BlobPreconditionFailedError";
      throw error;
    }
    if (previous && !options.allowOverwrite && !options.addRandomSuffix) throw new Error("Blob already exists");
    blobs.set(pathname, { body: String(body), etag: String(++etagSequence) });
    return {};
  },
  async list({ prefix, limit = 100, cursor }) {
    const all = [...blobs.keys()].filter((p) => p.startsWith(prefix));
    const offset = cursor ? Number(cursor) : 0;
    const page = all.slice(offset, offset + limit);
    const next = offset + page.length;
    return {
      blobs: page.map((pathname) => ({ pathname })),
      cursor: next < all.length ? String(next) : undefined,
      hasMore: next < all.length
    };
  },
  async del(pathname, options = {}) {
    const previous = blobs.get(pathname);
    if (options.ifMatch && (!previous || previous.etag !== options.ifMatch)) {
      const error = new Error("Blob changed before conditional delete");
      error.name = "BlobPreconditionFailedError";
      throw error;
    }
    blobs.delete(pathname);
  }
};

const blobModulePath = require.resolve("@vercel/blob");
require.cache[blobModulePath] = { id: blobModulePath, filename: blobModulePath, loaded: true, exports: blobMock };
const handler = require("../api/rooms");
const originalFetch = global.fetch;

function response() {
  return {
    statusCode: 0, headers: {}, body: null,
    setHeader(key, value) { this.headers[key] = value; return this; },
    status(code) { this.statusCode = code; return this; },
    json(value) { this.body = value; return this; }
  };
}
async function call(method, body = {}, token = "", query = {}) {
  const res = response();
  await handler({ method, body, query, headers: token ? { authorization: "Bearer " + token } : {} }, res);
  return res;
}
async function confirmMatchingDisc(hostToken, guestToken) {
  const fingerprint = "0123456789ABCDEF";
  const host = await call("POST", { action: "set_disc", fingerprint }, hostToken);
  const guest = await call("POST", { action: "set_disc", fingerprint }, guestToken);
  assert.equal(host.statusCode, 200);
  assert.equal(guest.statusCode, 200);
  assert.equal(guest.body.discMatch, true);
}
test.beforeEach(() => { blobs.clear(); etagSequence = 0; refreshOnNextRead = ""; });
test.afterEach(() => { global.fetch = originalFetch; });

test("a public room supports four human seats and rejects a fifth", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  assert.equal(host.statusCode, 201);
  const code = host.body.room.code;
  const tokens = [host.body.token];
  for (const displayName of ["Player Two", "Player Three", "Player Four"]) {
    const joined = await call("POST", { action: "join", code, displayName });
    assert.equal(joined.statusCode, 200);
    tokens.push(joined.body.token);
  }
  const fifth = await call("POST", { action: "join", code, displayName: "Player Five" });
  assert.equal(fifth.statusCode, 409);
  const room = await call("GET", {}, tokens[0], { code });
  assert.equal(room.statusCode, 200);
  assert.equal(room.body.room.maxPlayers, 4);
  assert.equal(room.body.room.players.length, 4);
});

test("ANY BATTLE skips private rooms and joins an open public room", async () => {
  await call("POST", { action: "create", displayName: "Private Host", isPrivate: true });
  const publicRoom = await call("POST", { action: "create", displayName: "Public Host", isPrivate: false });
  const joined = await call("POST", { action: "any", displayName: "Quick Join" });
  assert.equal(joined.statusCode, 200);
  assert.equal(joined.body.room.code, publicRoom.body.room.code);
  assert.equal(joined.body.room.players, 2);
});

test("a four-seat room becomes ready only when all four players are ready", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const code = host.body.room.code;
  const tokens = [host.body.token];
  for (const displayName of ["Player Two", "Player Three", "Player Four"]) {
    const joined = await call("POST", { action: "join", code, displayName });
    tokens.push(joined.body.token);
  }
  for (let i = 0; i < tokens.length - 1; i++) {
    const ready = await call("POST", { action: "heartbeat", ready: true }, tokens[i]);
    assert.equal(ready.statusCode, 200);
    const room = await call("GET", {}, tokens[0], { code });
    assert.notEqual(room.body.room.status, "ready");
  }
  const lastReady = await call("POST", { action: "heartbeat", ready: true }, tokens[3]);
  assert.equal(lastReady.statusCode, 200);
  const room = await call("GET", {}, tokens[0], { code });
  assert.equal(room.body.room.status, "ready");
});


test("four-seat retro lobby UI renders and its inline script parses", () => {
  const html = fs.readFileSync(path.join(__dirname, "../index.html"), "utf8");
  const script = html.match(/<script>([\s\S]*?)<\/script>/);
  assert.ok(script, "inline lobby script exists");
  assert.doesNotThrow(() => new Function(script[1]), "inline lobby JavaScript must parse");
  assert.match(html, /room\.maxPlayers\|\|4/);
  assert.match(html, /ANY BATTLE/);
  assert.match(html, /BATTLE BY CODE/);
  assert.match(html, /ADVENTURE MODE/);
  assert.match(html, /id="privateRoom"/);
  assert.match(html, /id="publicBtn"/);
  assert.match(html, /id="privateBtn"/);
  assert.match(html, /sessionStorage\.setItem\("cb_player_id",state\.player\.id\)/);
  assert.match(html, /data\.room\.selfPlayerId/);
  assert.match(html, /function expireRoomSession\(/);
  assert.match(html, /room\.players\.length>=2&&room\.players\.every\(p=>p\.ready\)/);
  assert.match(html, /room\.players\.length>1\?"Players joined/);
  assert.doesNotMatch(html, /\bboth\?/);
});

test("four-player signaling routes offers to the requested room member only", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const code = host.body.room.code;
  const tokens = [host.body.token];
  const playerIds = [host.body.player.id];
  for (const displayName of ["Player Two", "Player Three", "Player Four"]) {
    const joined = await call("POST", { action: "join", code, displayName });
    tokens.push(joined.body.token);
    playerIds.push(joined.body.player.id);
  }

  const offer = await call("POST", {
    action: "signal",
    toPlayerId: playerIds[3],
    type: "offer",
    payload: { sdp: "test-offer" }
  }, tokens[0]);
  assert.equal(offer.statusCode, 202);

  const targetPoll = await call("POST", { action: "poll", since: 0 }, tokens[3]);
  assert.equal(targetPoll.statusCode, 200);
  assert.equal(targetPoll.body.signals.length, 1);
  assert.equal(targetPoll.body.signals[0].from, playerIds[0]);
  assert.deepEqual(targetPoll.body.signals[0].payload, { sdp: "test-offer" });

  const unrelatedPoll = await call("POST", { action: "poll", since: 0 }, tokens[1]);
  assert.equal(unrelatedPoll.statusCode, 200);
  assert.equal(unrelatedPoll.body.signals.length, 0);
});

test("room signaling rejects missing, self and unknown recipients", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const code = host.body.room.code;
  const joined = await call("POST", { action: "join", code, displayName: "Player Two" });
  const hostId = host.body.player.id;

  const missing = await call("POST", { action: "signal", type: "offer", payload: {} }, host.body.token);
  assert.equal(missing.statusCode, 400);

  const self = await call("POST", {
    action: "signal", toPlayerId: hostId, type: "offer", payload: {}
  }, host.body.token);
  assert.equal(self.statusCode, 404);

  const unknown = await call("POST", {
    action: "signal", toPlayerId: "not-a-room-player", type: "offer", payload: {}
  }, joined.body.token);
  assert.equal(unknown.statusCode, 404);
});


test("simultaneous joins retry stale ETags without losing player seats", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const code = host.body.room.code;
  const joins = await Promise.all(["A", "B", "C"].map((displayName) =>
    call("POST", { action: "join", code, displayName })
  ));
  assert.deepEqual(joins.map((result) => result.statusCode), [200, 200, 200]);
  const room = await call("GET", {}, host.body.token, { code });
  assert.equal(room.body.room.players.length, 4);
  assert.equal(new Set(room.body.room.players.map((player) => player.id)).size, 4);
});

test("simultaneous ANY BATTLE requests fill the same public room without overwriting seats", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const joins = await Promise.all(["A", "B", "C"].map((displayName) =>
    call("POST", { action: "any", displayName })
  ));
  assert.deepEqual(joins.map((result) => result.statusCode), [200, 200, 200]);
  assert.ok(joins.every((result) => result.body.room.code === host.body.room.code));
  const room = await call("GET", {}, host.body.token, { code: host.body.room.code });
  assert.equal(room.body.room.players.length, 4);
});

test("simultaneous ready heartbeats preserve all four players' updates", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const tokens = [host.body.token];
  for (const displayName of ["B", "C", "D"]) {
    const joined = await call("POST", { action: "join", code: host.body.room.code, displayName });
    tokens.push(joined.body.token);
  }
  const updates = await Promise.all(tokens.map((token) =>
    call("POST", { action: "heartbeat", ready: true }, token)
  ));
  assert.ok(updates.every((result) => result.statusCode === 200));
  const room = await call("GET", {}, tokens[0], { code: host.body.room.code });
  assert.equal(room.body.room.status, "ready");
  assert.ok(room.body.room.players.every((player) => player.ready));
});

test("simultaneous signaling messages are retained instead of overwriting each other", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const code = host.body.room.code;
  const tokens = [host.body.token];
  const ids = [host.body.player.id];
  for (const displayName of ["B", "C", "D"]) {
    const joined = await call("POST", { action: "join", code, displayName });
    tokens.push(joined.body.token);
    ids.push(joined.body.player.id);
  }
  const sends = await Promise.all([0, 1, 2].map((index) =>
    call("POST", {
      action: "signal",
      toPlayerId: ids[3],
      type: "ice",
      payload: { candidate: "candidate-" + index }
    }, tokens[index])
  ));
  assert.ok(sends.every((result) => result.statusCode === 202));
  const inbox = await call("POST", { action: "poll", since: 0 }, tokens[3]);
  assert.equal(inbox.body.signals.length, 3);
  assert.deepEqual(
    inbox.body.signals.map((signal) => signal.payload.candidate).sort(),
    ["candidate-0", "candidate-1", "candidate-2"]
  );
});

test("leaving the last seat closes the room instead of deleting during a join race", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const code = host.body.room.code;
  const left = await call("POST", { action: "leave" }, host.body.token);
  assert.equal(left.statusCode, 200);
  const rejoin = await call("POST", { action: "join", code, displayName: "Late Player" });
  assert.equal(rejoin.statusCode, 409);
  const list = await call("GET");
  assert.equal(list.body.rooms.some((room) => room.code === code), false);
});


test("a ready public room reopens when a disconnected seat expires", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const code = host.body.room.code;
  const tokens = [host.body.token];

  for (const displayName of ["B", "C", "D"]) {
    const joined = await call("POST", { action: "join", code, displayName });
    tokens.push(joined.body.token);
  }
  await Promise.all(tokens.map((token) =>
    call("POST", { action: "heartbeat", ready: true }, token)
  ));

  const blobPath = "cb-rooms/" + code + ".json";
  const room = JSON.parse(blobs.get(blobPath).body);
  room.players[3].lastSeen = Date.now() - 11 * 60 * 1000;
  blobs.set(blobPath, { body: JSON.stringify(room), etag: String(++etagSequence) });

  const publicRooms = await call("GET");
  const listed = publicRooms.body.rooms.find((entry) => entry.code === code);
  assert.ok(listed);
  assert.equal(listed.players, 3);
  assert.equal(listed.status, "waiting");

  const quickJoin = await call("POST", { action: "any", displayName: "Replacement" });
  assert.equal(quickJoin.statusCode, 200);
  assert.equal(quickJoin.body.room.code, code);

  const finalRoom = await call("GET", {}, host.body.token, { code });
  assert.equal(finalRoom.body.room.players.length, 4);
  assert.equal(finalRoom.body.room.status, "waiting");
  assert.equal(finalRoom.body.room.players.filter((player) => player.ready).length, 0);
});


test("room refresh identifies the current player and rejects an expired seat", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const code = host.body.room.code;

  const room = await call("GET", {}, host.body.token, { code });
  assert.equal(room.statusCode, 200);
  assert.equal(room.body.room.selfPlayerId, host.body.player.id);
  assert.equal(room.body.room.players[0].id, host.body.player.id);

  const stored = JSON.parse(blobs.get("cb-rooms/" + code + ".json").body);
  stored.players[0].lastSeen = Date.now() - 11 * 60 * 1000;
  blobs.set("cb-rooms/" + code + ".json", {
    body: JSON.stringify(stored),
    etag: String(++etagSequence)
  });

  const expired = await call("GET", {}, host.body.token, { code });
  assert.equal(expired.statusCode, 403);
  assert.match(expired.body.error, /seat has expired/i);
});

test("heartbeat prunes expired seats and returns only active players", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const code = host.body.room.code;
  const tokens = [host.body.token];
  const playerIds = [host.body.player.id];

  for (const displayName of ["B", "C", "D"]) {
    const joined = await call("POST", { action: "join", code, displayName });
    tokens.push(joined.body.token);
    playerIds.push(joined.body.player.id);
  }
  await Promise.all(tokens.map((token) =>
    call("POST", { action: "heartbeat", ready: true }, token)
  ));

  const blobPath = "cb-rooms/" + code + ".json";
  const stored = JSON.parse(blobs.get(blobPath).body);
  stored.players[3].lastSeen = Date.now() - 11 * 60 * 1000;
  blobs.set(blobPath, { body: JSON.stringify(stored), etag: String(++etagSequence) });

  const heartbeat = await call("POST", { action: "heartbeat", ready: true }, tokens[0]);
  assert.equal(heartbeat.statusCode, 200);
  assert.equal(heartbeat.body.players.length, 3);
  assert.equal(heartbeat.body.players.some((player) => player.id === playerIds[3]), false);
  assert.equal(heartbeat.body.players.filter((player) => player.ready).length, 1);

  const persisted = JSON.parse(blobs.get(blobPath).body);
  assert.equal(persisted.players.length, 3);
  assert.equal(persisted.status, "waiting");

  const expiredPlayer = await call("GET", {}, tokens[3], { code });
  assert.equal(expiredPlayer.statusCode, 403);
});

test("a restored browser session keeps the same player identity and ready state", () => {
  const html = fs.readFileSync(path.join(__dirname, "../index.html"), "utf8");
  assert.match(html, /sessionStorage\.setItem\("cb_player_id",state\.player\.id\)/);
  assert.match(html, /data\.room\.selfPlayerId/);
  assert.match(html, /state\.ready=Boolean\(me\.ready\)/);
  assert.match(html, /startRoomTimers\(3000\)/);
  assert.match(html, /Your room session expired/);
  assert.equal((html.match(/\[401,403,404,410\]\.includes\(e\.status\)/g) || []).length, 2);
});


test("expired room codes and sessions are rejected while leave remains available", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const code = host.body.room.code;
  const blobPath = "cb-rooms/" + code + ".json";
  const stored = JSON.parse(blobs.get(blobPath).body);
  stored.updatedAt = Date.now() - 31 * 60 * 1000;
  blobs.set(blobPath, { body: JSON.stringify(stored), etag: String(++etagSequence) });

  const room = await call("GET", {}, host.body.token, { code });
  assert.equal(room.statusCode, 410);
  assert.match(room.body.error, /expired/i);

  const join = await call("POST", { action: "join", code, displayName: "Late Player" });
  assert.equal(join.statusCode, 410);

  const heartbeat = await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  assert.equal(heartbeat.statusCode, 410);

  const leave = await call("POST", { action: "leave" }, host.body.token);
  assert.equal(leave.statusCode, 200);
});


test("public room discovery does not stop at the old forty-room cutoff", async () => {
  for (let index = 0; index < 45; index += 1) {
    const created = await call("POST", {
      action: "create",
      displayName: "Player " + index,
      isPrivate: false
    });
    assert.equal(created.statusCode, 201);
  }

  const listing = await call("GET");
  assert.equal(listing.statusCode, 200);
  assert.equal(listing.body.rooms.length, 45);
});


test("public room discovery follows Blob pagination beyond one hundred rooms", async () => {
  const chars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  const now = Date.now();
  for (let index = 0; index < 120; index += 1) {
    const code = "AAAA" + chars[Math.floor(index / 32)] + chars[index % 32];
    const room = {
      code,
      status: "waiting",
      isPrivate: false,
      maxPlayers: 4,
      createdAt: now - index,
      updatedAt: now,
      players: [{
        id: "player-" + index,
        name: "Host " + index,
        ready: false,
        joinedAt: now,
        lastSeen: now
      }],
      signals: [],
      nextSignalId: 1
    };
    blobs.set("cb-rooms/" + code + ".json", {
      body: JSON.stringify(room),
      etag: String(++etagSequence)
    });
  }

  const listing = await call("GET");
  assert.equal(listing.statusCode, 200);
  assert.equal(listing.body.rooms.length, 120);
});


test("public room discovery removes expired Blob records", async () => {
  const created = await call("POST", { action: "create", displayName: "Expired Host", isPrivate: false });
  const code = created.body.room.code;
  const pathname = "cb-rooms/" + code + ".json";
  const stored = JSON.parse(blobs.get(pathname).body);
  stored.updatedAt = Date.now() - 31 * 60 * 1000;
  blobs.set(pathname, { body: JSON.stringify(stored), etag: String(++etagSequence) });

  const listing = await call("GET");
  assert.equal(listing.statusCode, 200);
  assert.equal(listing.body.rooms.length, 0);
  assert.equal(blobs.has(pathname), false);
});

test("expired-room cleanup never deletes a concurrently refreshed room", async () => {
  const created = await call("POST", { action: "create", displayName: "Refreshing Host", isPrivate: false });
  const code = created.body.room.code;
  const pathname = "cb-rooms/" + code + ".json";
  const stored = JSON.parse(blobs.get(pathname).body);
  stored.updatedAt = Date.now() - 31 * 60 * 1000;
  blobs.set(pathname, { body: JSON.stringify(stored), etag: String(++etagSequence) });
  refreshOnNextRead = pathname;

  const firstListing = await call("GET");
  assert.equal(firstListing.statusCode, 200);
  assert.equal(firstListing.body.rooms.length, 0);
  assert.equal(blobs.has(pathname), true);

  const secondListing = await call("GET");
  assert.equal(secondListing.statusCode, 200);
  assert.equal(secondListing.body.rooms.length, 1);
  assert.equal(secondListing.body.rooms[0].code, code);
});


test("a two-player room becomes ready when both players are ready", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const joined = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });

  const hostReady = await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  assert.equal(hostReady.statusCode, 200);
  let room = await call("GET", {}, host.body.token, { code: host.body.room.code });
  assert.equal(room.body.room.status, "waiting");

  const guestReady = await call("POST", { action: "heartbeat", ready: true }, joined.body.token);
  assert.equal(guestReady.statusCode, 200);
  room = await call("GET", {}, host.body.token, { code: host.body.room.code });
  assert.equal(room.body.room.status, "ready");
  assert.equal(room.body.room.players.length, 2);
});


test("room view clears ready flags when a disconnected seat is detected", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const code = host.body.room.code;
  const tokens = [host.body.token];
  for (const displayName of ["B", "C", "D"]) {
    const joined = await call("POST", { action: "join", code, displayName });
    tokens.push(joined.body.token);
  }
  await Promise.all(tokens.map((token) =>
    call("POST", { action: "heartbeat", ready: true }, token)
  ));

  const pathname = "cb-rooms/" + code + ".json";
  const stored = JSON.parse(blobs.get(pathname).body);
  stored.players[3].lastSeen = Date.now() - 11 * 60 * 1000;
  blobs.set(pathname, { body: JSON.stringify(stored), etag: String(++etagSequence) });

  const room = await call("GET", {}, host.body.token, { code });
  assert.equal(room.statusCode, 200);
  assert.equal(room.body.room.status, "waiting");
  assert.equal(room.body.room.players.length, 3);
  assert.equal(room.body.room.players.filter((player) => player.ready).length, 0);
});


test("room creator is the host and host role transfers when they leave", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const joined = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  let room = await call("GET", {}, joined.body.token, { code: host.body.room.code });
  assert.equal(room.body.room.hostPlayerId, host.body.player.id);
  const originalNetplayId = room.body.room.netplayId;

  const left = await call("POST", { action: "leave" }, host.body.token);
  assert.equal(left.statusCode, 200);
  room = await call("GET", {}, joined.body.token, { code: host.body.room.code });
  assert.equal(room.body.room.hostPlayerId, joined.body.player.id);
  assert.equal(room.body.room.netplayId === originalNetplayId, false);
  assert.match(room.body.room.netplayId, /^[A-F0-9]{12}$/);
  assert.equal(room.body.room.players.length, 1);
});

test("a ready two-player room stays locked and is not advertised as waiting", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  await confirmMatchingDisc(host.body.token, guest.body.token);
  await call("POST", { action: "game_start" }, host.body.token);

  let room = await call("GET", {}, host.body.token, { code: host.body.room.code });
  assert.equal(room.body.room.status, "ready");

  const listing = await call("GET");
  assert.equal(listing.body.rooms.some((entry) => entry.code === host.body.room.code), false);

  const third = await call("POST", {
    action: "join",
    code: host.body.room.code,
    displayName: "Late Player"
  });
  assert.equal(third.statusCode, 409);

  const any = await call("POST", { action: "any", displayName: "Quick Join" });
  assert.equal(any.statusCode, 404);
});

test("an expired host seat is promoted before the remaining player heartbeats", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  const initialRoom = await call("GET", {}, guest.body.token, { code: host.body.room.code });
  const originalNetplayId = initialRoom.body.room.netplayId;
  const pathname = "cb-rooms/" + host.body.room.code + ".json";
  const stored = JSON.parse(blobs.get(pathname).body);
  stored.players[0].lastSeen = Date.now() - 11 * 60 * 1000;
  blobs.set(pathname, { body: JSON.stringify(stored), etag: String(++etagSequence) });

  const heartbeat = await call("POST", { action: "heartbeat", ready: false }, guest.body.token);
  assert.equal(heartbeat.statusCode, 200);
  const room = await call("GET", {}, guest.body.token, { code: host.body.room.code });
  assert.equal(room.body.room.hostPlayerId, guest.body.player.id);
  assert.notEqual(room.body.room.netplayId, originalNetplayId);
  assert.equal(room.body.room.status, "waiting");
});


test("Netplay relay lookup is private to room members and resolves the matching host", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: true });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  const code = host.body.room.code;
  const details = await call("GET", {}, guest.body.token, { code });
  const netplayId = details.body.room.netplayId;

  assert.match(netplayId, /^[A-F0-9]{12}$/);
  const publicListing = await call("GET");
  assert.equal(publicListing.body.rooms.some((entry) => entry.code === code), false);
  const publicHost = await call("POST", { action: "create", displayName: "Public Host", isPrivate: false });
  const publicListingWithRoom = await call("GET");
  const publicEntry = publicListingWithRoom.body.rooms.find((entry) => entry.code === publicHost.body.room.code);
  assert.ok(publicEntry);
  assert.equal(Object.hasOwn(publicEntry, "netplayId"), false);

  const hostReady = await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  const guestReady = await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  assert.equal(hostReady.body.netplayId, netplayId);
  assert.equal(guestReady.body.netplayId, netplayId);
  await confirmMatchingDisc(host.body.token, guest.body.token);
  const start = await call("POST", { action: "game_start" }, host.body.token);
  assert.equal(start.statusCode, 200);
  assert.equal(start.body.ok, true);

  let requestedUrl = "";
  global.fetch = async (url, options) => {
    requestedUrl = url;
    assert.equal(options.headers.Accept, "application/json");
    return {
      ok: true,
      status: 200,
      async json() {
        return [
          { fields: { username: "some-other-room-HOST", connectable: true, ip: "203.0.113.2", port: 55435 } },
          {
            fields: {
              username: "CB-" + netplayId + "-HOST",
              connectable: true,
              host_method: 3,
              mitm_ip: "madrid.relay.retroarch.com",
              mitm_port: 55435,
              mitm_session: "AbCdEf0123456789",
              game_name: "Crash Bash",
              core_name: "PCSX-ReARMed",
              core_version: "r23l",
              retroarch_version: "1.22.2",
              updated: new Date().toISOString()
            }
          }
        ];
      }
    };
  };

  const lookup = await call("POST", { action: "netplay" }, guest.body.token);
  assert.equal(requestedUrl, "http://lobby.libretro.com/list");
  assert.equal(lookup.statusCode, 200);
  assert.equal(lookup.body.ready, true);
  assert.equal(lookup.body.mode, "relay");
  assert.equal(lookup.body.address, "madrid.relay.retroarch.com");
  assert.equal(lookup.body.port, 55435);
  assert.equal(lookup.body.mitmSession, "AbCdEf0123456789");
  assert.equal(lookup.body.hostNickname, "CB-" + netplayId + "-HOST");
  assert.equal(lookup.body.gameName, "Crash Bash");
  assert.equal(lookup.body.coreName, "PCSX-ReARMed");
});

test("Netplay endpoint lookup waits until the room is ready", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  let fetched = false;
  global.fetch = async () => {
    fetched = true;
    throw new Error("The lobby should not be queried before the room is ready.");
  };

  const lookup = await call("POST", { action: "netplay" }, host.body.token);
  assert.equal(lookup.statusCode, 409);
  assert.equal(lookup.body.state, "room_not_ready");
  assert.equal(fetched, false);
});

test("Netplay endpoint lookup reports a waiting state before the host announces", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  await confirmMatchingDisc(host.body.token, guest.body.token);
  await call("POST", { action: "game_start" }, host.body.token);
  global.fetch = async () => ({
    ok: true,
    status: 200,
    async json() { return []; }
  });

  const lookup = await call("POST", { action: "netplay" }, guest.body.token);
  assert.equal(lookup.statusCode, 200);
  assert.equal(lookup.body.ready, false);
  assert.equal(lookup.body.state, "waiting_for_host");
});


test("Netplay endpoint uses a connectable direct host when no relay session exists", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  await confirmMatchingDisc(host.body.token, guest.body.token);
  await call("POST", { action: "game_start" }, host.body.token);
  const room = await call("GET", {}, guest.body.token, { code: host.body.room.code });

  global.fetch = async () => ({
    ok: true,
    status: 200,
    async json() {
      return [{
        fields: {
          username: "CB-" + room.body.room.netplayId + "-HOST",
          connectable: true,
          ip: "203.0.113.25",
          port: 55435,
          mitm_ip: "",
          mitm_port: 0,
          mitm_session: "",
          updated: new Date().toISOString()
        }
      }];
    }
  });

  const lookup = await call("POST", { action: "netplay" }, guest.body.token);
  assert.equal(lookup.statusCode, 200);
  assert.equal(lookup.body.ready, true);
  assert.equal(lookup.body.mode, "direct");
  assert.equal(lookup.body.address, "203.0.113.25");
  assert.equal(lookup.body.mitmSession, "");
});

test("Netplay endpoint ignores a stale public-lobby host entry", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  await confirmMatchingDisc(host.body.token, guest.body.token);
  await call("POST", { action: "game_start" }, host.body.token);
  const room = await call("GET", {}, guest.body.token, { code: host.body.room.code });

  global.fetch = async () => ({
    ok: true,
    status: 200,
    async json() {
      return [{
        fields: {
          username: "CB-" + room.body.room.netplayId + "-HOST",
          connectable: true,
          ip: "203.0.113.25",
          port: 55435,
          updated: new Date(Date.now() - 3 * 60 * 1000).toISOString()
        }
      }];
    }
  });

  const lookup = await call("POST", { action: "netplay" }, guest.body.token);
  assert.equal(lookup.statusCode, 200);
  assert.equal(lookup.body.ready, false);
  assert.equal(lookup.body.state, "host_not_connectable");
});


test("Netplay start requires every active player to confirm the same disc content", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  const invalid = await call("POST", { action: "set_disc", fingerprint: "not-a-fingerprint" }, host.body.token);
  assert.equal(invalid.statusCode, 400);
  const first = await call("POST", { action: "set_disc", fingerprint: "0123456789ABCDEF" }, host.body.token);
  assert.equal(first.body.discSelectionCount, 1);
  assert.equal(first.body.discMatch, false);
  const missing = await call("POST", { action: "game_start" }, host.body.token);
  assert.equal(missing.statusCode, 409);
  assert.equal(missing.body.state, "waiting_for_disc_selection");
  const second = await call("POST", { action: "set_disc", fingerprint: "FEDCBA9876543210" }, guest.body.token);
  assert.equal(second.body.discMismatch, true);
  const mismatch = await call("POST", { action: "netplay" }, guest.body.token);
  assert.equal(mismatch.body.state, "disc_mismatch");
  const blocked = await call("POST", { action: "game_start" }, host.body.token);
  assert.equal(blocked.statusCode, 409);
  assert.equal(blocked.body.state, "disc_mismatch");
  const corrected = await call("POST", { action: "set_disc", fingerprint: "0123456789ABCDEF" }, guest.body.token);
  assert.equal(corrected.body.discMatch, true);
  const started = await call("POST", { action: "game_start" }, host.body.token);
  assert.equal(started.statusCode, 200);
  const locked = await call("POST", { action: "set_disc", fingerprint: "0123456789ABCDEF" }, guest.body.token);
  assert.equal(locked.statusCode, 409);
});

test("clearing a disc selection blocks launch until the player confirms again", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  await confirmMatchingDisc(host.body.token, guest.body.token);

  const cleared = await call("POST", { action: "clear_disc" }, guest.body.token);
  assert.equal(cleared.statusCode, 200);
  assert.equal(cleared.body.ok, true);
  assert.equal(cleared.body.discSelectionCount, 1);
  assert.equal(cleared.body.discMatch, false);

  const blocked = await call("POST", { action: "game_start" }, host.body.token);
  assert.equal(blocked.statusCode, 409);
  assert.equal(blocked.body.state, "waiting_for_disc_selection");

  const confirmed = await call("POST", {
    action: "set_disc",
    fingerprint: "0123456789ABCDEF"
  }, guest.body.token);
  assert.equal(confirmed.statusCode, 200);
  assert.equal(confirmed.body.discMatch, true);

  const started = await call("POST", { action: "game_start" }, host.body.token);
  assert.equal(started.statusCode, 200);
  const locked = await call("POST", { action: "clear_disc" }, guest.body.token);
  assert.equal(locked.statusCode, 409);
});

test("marking a player not ready clears their previous disc confirmation", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  await confirmMatchingDisc(host.body.token, guest.body.token);

  const unready = await call("POST", { action: "heartbeat", ready: false }, guest.body.token);
  assert.equal(unready.statusCode, 200);
  assert.equal(unready.body.discSelectionCount, 1);
  assert.equal(unready.body.discMatch, false);

  const readyAgain = await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  assert.equal(readyAgain.statusCode, 200);
  assert.equal(readyAgain.body.discMatch, false);
  const roomAfterReady = await call("GET", {}, guest.body.token, { code: host.body.room.code });
  assert.equal(roomAfterReady.body.room.status, "ready");
  assert.equal(roomAfterReady.body.room.discMatch, false);

  const blocked = await call("POST", { action: "game_start" }, host.body.token);
  assert.equal(blocked.statusCode, 409);
  assert.equal(blocked.body.state, "waiting_for_disc_selection");
});

test("a membership change clears every remaining disc confirmation", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guestOne = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest One" });
  const guestTwo = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest Two" });
  for (const token of [host.body.token, guestOne.body.token, guestTwo.body.token]) {
    await call("POST", { action: "heartbeat", ready: true }, token);
  }
  for (const token of [host.body.token, guestOne.body.token, guestTwo.body.token]) {
    await call("POST", { action: "set_disc", fingerprint: "0123456789ABCDEF" }, token);
  }

  const left = await call("POST", { action: "leave" }, guestTwo.body.token);
  assert.equal(left.statusCode, 200);
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guestOne.body.token);

  const room = await call("GET", {}, host.body.token, { code: host.body.room.code });
  assert.equal(room.body.room.status, "ready");
  assert.equal(room.body.room.players.length, 2);
  assert.equal(room.body.room.discSelectionCount, 0);
  assert.equal(room.body.room.discMatch, false);

  const blocked = await call("POST", { action: "game_start" }, host.body.token);
  assert.equal(blocked.statusCode, 409);
  assert.equal(blocked.body.state, "waiting_for_disc_selection");
});

test("a stale READY lease expires without deleting the player's room seat", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  await confirmMatchingDisc(host.body.token, guest.body.token);

  const pathname = "cb-rooms/" + host.body.room.code + ".json";
  const stored = JSON.parse(blobs.get(pathname).body);
  const guestRecord = stored.players.find((entry) => entry.id === guest.body.player.id);
  guestRecord.lastSeen = Date.now() - 90001;
  blobs.set(pathname, { body: JSON.stringify(stored), etag: String(++etagSequence) });

  const room = await call("GET", {}, guest.body.token, { code: host.body.room.code });
  assert.equal(room.statusCode, 200);
  assert.equal(room.body.room.players.length, 2);
  assert.equal(room.body.room.players.find((entry) => entry.id === guest.body.player.id).ready, false);
  assert.equal(room.body.room.players.find((entry) => entry.id === guest.body.player.id).discFingerprint, "");
  assert.equal(room.body.room.status, "waiting");
  assert.equal(room.body.room.discSelectionCount, 1);
  assert.equal(room.body.room.discMatch, false);

  const blockedUntilReady = await call("POST", { action: "game_start" }, host.body.token);
  assert.equal(blockedUntilReady.statusCode, 409);
  assert.match(blockedUntilReady.body.error, /Every active player must be READY/);

  const readyAgain = await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  assert.equal(readyAgain.statusCode, 200);
  assert.equal(readyAgain.body.discMatch, false);
  const blockedUntilConfirmed = await call("POST", { action: "game_start" }, host.body.token);
  assert.equal(blockedUntilConfirmed.statusCode, 409);
  assert.equal(blockedUntilConfirmed.body.state, "waiting_for_disc_selection");
});

test("only the host can start Netplay and a started room rejects late joins", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);

  await confirmMatchingDisc(host.body.token, guest.body.token);
  const guestStart = await call("POST", { action: "game_start" }, guest.body.token);
  assert.equal(guestStart.statusCode, 403);

  const hostStart = await call("POST", { action: "game_start" }, host.body.token);
  assert.equal(hostStart.statusCode, 200);
  assert.equal(hostStart.body.ok, true);
  assert.ok(hostStart.body.gameStartedAt);

  const lateJoin = await call("POST", {
    action: "join",
    code: host.body.room.code,
    displayName: "Late Player"
  });
  assert.equal(lateJoin.statusCode, 409);

  const listing = await call("GET");
  assert.equal(listing.body.rooms.some((entry) => entry.code === host.body.room.code), false);
});

test("started rooms retain player seats for a full match window", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  await confirmMatchingDisc(host.body.token, guest.body.token);
  await call("POST", { action: "game_start" }, host.body.token);

  const pathname = "cb-rooms/" + host.body.room.code + ".json";
  const stored = JSON.parse(blobs.get(pathname).body);
  stored.players.find((player) => player.id === host.body.player.id).lastSeen =
    Date.now() - 15 * 60 * 1000;
  blobs.set(pathname, { body: JSON.stringify(stored), etag: String(++etagSequence) });

  const heartbeat = await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  assert.equal(heartbeat.statusCode, 200);
  assert.equal(heartbeat.body.players.length, 2);
  assert.equal(heartbeat.body.players.some((player) => player.id === host.body.player.id), true);
  assert.equal(heartbeat.body.hostPlayerId, host.body.player.id);

  const listing = await call("GET");
  assert.equal(listing.body.rooms.some((entry) => entry.code === host.body.room.code), false);
});

test("client Netplay discovery waits for the host to mark game start", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);

  await confirmMatchingDisc(host.body.token, guest.body.token);
  let fetched = false;
  global.fetch = async () => {
    fetched = true;
    return { ok: true, status: 200, async json() { return []; } };
  };
  const lookup = await call("POST", { action: "netplay" }, guest.body.token);
  assert.equal(lookup.statusCode, 200);
  assert.equal(lookup.body.ready, false);
  assert.equal(lookup.body.state, "waiting_for_host_start");
  assert.equal(fetched, false);
});


test("a started Netplay match keeps its host identity when the host leaves the lobby app", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: true });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  await confirmMatchingDisc(host.body.token, guest.body.token);
  await call("POST", { action: "game_start" }, host.body.token);
  const room = await call("GET", {}, guest.body.token, { code: host.body.room.code });
  const netplayId = room.body.room.netplayId;
  assert.equal(room.body.room.gameStarted, true);

  await call("POST", { action: "leave" }, host.body.token);
  const afterLeave = await call("GET", {}, guest.body.token, { code: host.body.room.code });
  assert.equal(afterLeave.body.room.netplayId, netplayId);
  assert.equal(afterLeave.body.room.gameStarted, true);
  const guestHeartbeat = await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  assert.equal(guestHeartbeat.body.gameStarted, true);
  assert.equal(guestHeartbeat.body.netplayId, netplayId);
  assert.equal(guestHeartbeat.body.players.length, 1);

  const guestCancel = await call("POST", { action: "game_start_cancel" }, guest.body.token);
  assert.equal(guestCancel.statusCode, 403);
  const stillStarted = await call("GET", {}, guest.body.token, { code: host.body.room.code });
  assert.equal(stillStarted.body.room.gameStarted, true);

  global.fetch = async () => ({
    ok: true,
    status: 200,
    async json() {
      return [{
        fields: {
          username: "CB-" + netplayId + "-HOST",
          connectable: true,
          mitm_ip: "madrid.relay.retroarch.com",
          mitm_port: 55435,
          mitm_session: "AbCdEf0123456789",
          updated: new Date().toISOString()
        }
      }];
    }
  });

  const lookup = await call("POST", { action: "netplay" }, guest.body.token);
  assert.equal(lookup.statusCode, 200);
  assert.equal(lookup.body.ready, true);
  assert.equal(lookup.body.address, "madrid.relay.retroarch.com");
});


test("host can roll back the room lock when RetroArch fails to launch", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: false });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);

  await confirmMatchingDisc(host.body.token, guest.body.token);
  const started = await call("POST", { action: "game_start" }, host.body.token);
  assert.equal(started.statusCode, 200);
  const beforeCancel = await call("GET", {}, guest.body.token, { code: host.body.room.code });
  assert.equal(beforeCancel.body.room.gameStarted, true);

  const cancelled = await call("POST", { action: "game_start_cancel" }, host.body.token);
  assert.equal(cancelled.statusCode, 200);
  assert.equal(cancelled.body.gameStartCancelled, true);

  const afterCancel = await call("GET", {}, guest.body.token, { code: host.body.room.code });
  assert.equal(afterCancel.body.room.gameStarted, false);
  assert.equal(afterCancel.body.room.players.length, 2);
});


test("a started Netplay match keeps its relay identity when the host seat expires", async () => {
  const host = await call("POST", { action: "create", displayName: "Host", isPrivate: true });
  const guest = await call("POST", { action: "join", code: host.body.room.code, displayName: "Guest" });
  await call("POST", { action: "heartbeat", ready: true }, host.body.token);
  await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  await confirmMatchingDisc(host.body.token, guest.body.token);
  await call("POST", { action: "game_start" }, host.body.token);

  const pathname = "cb-rooms/" + host.body.room.code + ".json";
  const stored = JSON.parse(blobs.get(pathname).body);
  const netplayId = stored.netplayId;
  stored.players.find((player) => player.id === host.body.player.id).lastSeen =
    Date.now() - 31 * 60 * 1000;
  blobs.set(pathname, { body: JSON.stringify(stored), etag: String(++etagSequence) });

  const heartbeat = await call("POST", { action: "heartbeat", ready: true }, guest.body.token);
  assert.equal(heartbeat.statusCode, 200);
  assert.equal(heartbeat.body.gameStarted, true);
  assert.equal(heartbeat.body.netplayId, netplayId);
  assert.equal(heartbeat.body.players.length, 1);

  global.fetch = async () => ({
    ok: true,
    status: 200,
    async json() {
      return [{
        fields: {
          username: "CB-" + netplayId + "-HOST",
          connectable: true,
          mitm_ip: "madrid.relay.retroarch.com",
          mitm_port: 55435,
          mitm_session: "AbCdEf0123456789",
          updated: new Date().toISOString()
        }
      }];
    }
  });

  const lookup = await call("POST", { action: "netplay" }, guest.body.token);
  assert.equal(lookup.body.ready, true);
  assert.equal(lookup.body.address, "madrid.relay.retroarch.com");
});
