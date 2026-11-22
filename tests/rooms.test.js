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


test("four-seat lobby UI renders without stale two-player state", () => {
  const html = fs.readFileSync(path.join(__dirname, "../index.html"), "utf8");
  assert.match(html, /room\.maxPlayers\|\|4/);
  assert.match(html, /ANY BATTLE/);
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
