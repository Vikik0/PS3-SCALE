"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

process.env.BLOB_READ_WRITE_TOKEN = "test-blob-token";
process.env.CB_TOKEN_SECRET_V2 = "test-only-guest-token-secret-with-more-than-forty-characters";

const blobs = new Map();
let etagSequence = 0;
const blobMock = {
  async get(pathname) {
    const record = blobs.get(pathname);
    if (!record) return null;
    return { statusCode: 200, stream: new Response(record.body).body, blob: { etag: record.etag } };
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
  async list({ prefix }) {
    return { blobs: [...blobs.keys()].filter((p) => p.startsWith(prefix)).map((pathname) => ({ pathname })) };
  },
  async del(pathname) { blobs.delete(pathname); }
};

const blobModulePath = require.resolve("@vercel/blob");
require.cache[blobModulePath] = { id: blobModulePath, filename: blobModulePath, loaded: true, exports: blobMock };
const handler = require("../api/rooms");

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
test.beforeEach(() => { blobs.clear(); etagSequence = 0; });

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

  for (const displayName of ["B", "C", "D"]) {
    const joined = await call("POST", { action: "join", code, displayName });
    tokens.push(joined.body.token);
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
});
