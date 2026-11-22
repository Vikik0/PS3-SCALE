"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");

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
