"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");

process.env.CB_TOKEN_SECRET = "test-only-secret-with-more-than-forty-characters-123456";
process.env.BLOB_READ_WRITE_TOKEN = "test-blob-token";

const store = new Map();
let etagCounter = 0;

const blobPath = require.resolve("@vercel/blob");
require(blobPath);
require.cache[blobPath].exports = {
  async get(path) {
    const entry = store.get(path);
    if (!entry) return { statusCode: 404 };
    return {
      statusCode: 200,
      stream: new Blob([entry.data]).stream(),
      blob: { etag: entry.etag }
    };
  },
  async put(path, data, options = {}) {
    const current = store.get(path);
    if (options.ifMatch && (!current || current.etag !== options.ifMatch)) {
      const error = new Error("Blob precondition failed");
      error.name = "BlobPreconditionFailedError";
      throw error;
    }
    if (current && !options.allowOverwrite) {
      const error = new Error("Blob already exists");
      error.name = "BlobAlreadyExistsError";
      throw error;
    }
    store.set(path, {
      data: String(data),
      etag: "test-etag-" + (++etagCounter)
    });
  },
  async list(options = {}) {
    return {
      blobs: Array.from(store.keys())
        .filter((pathname) => pathname.startsWith(options.prefix || ""))
        .map((pathname) => ({ pathname }))
    };
  },
  async del(path) {
    store.delete(path);
  }
};

const handler = require("../api/rooms");

function responseMock() {
  return {
    statusCode: 0,
    headers: {},
    body: null,
    setHeader(name, value) {
      this.headers[name] = value;
    },
    status(code) {
      this.statusCode = code;
      return this;
    },
    json(value) {
      this.body = value;
      return this;
    }
  };
}

async function invoke(method, body, token, query) {
  const req = {
    method,
    body: body || {},
    query: query || {},
    headers: token ? { authorization: "Bearer " + token } : {}
  };
  const res = responseMock();
  await handler(req, res);
  return res;
}

test("room API creates private/public rooms, joins by invite code, updates ready state and cleans up", async () => {
  store.clear();

  const privateHost = await invoke("POST", {
    action: "create",
    displayName: "Private Host",
    isPrivate: true
  });
  assert.equal(privateHost.statusCode, 201);
  assert.match(privateHost.body.room.code, /^[A-Z2-9]{6}$/);
  assert.ok(privateHost.body.token);

  const privateCode = privateHost.body.room.code;
  const privateDetails = await invoke("GET", {}, privateHost.body.token, { code: privateCode });
  assert.equal(privateDetails.statusCode, 200);
  assert.equal(privateDetails.body.room.isPrivate, true);
  assert.equal(privateDetails.body.room.players.length, 1);

  const privateGuest = await invoke("POST", {
    action: "join",
    code: privateCode,
    displayName: "Private Guest"
  });
  assert.equal(privateGuest.statusCode, 200);
  assert.equal(privateGuest.body.room.code, privateCode);

  const joinedDetails = await invoke("GET", {}, privateHost.body.token, { code: privateCode });
  assert.equal(joinedDetails.body.room.players.length, 2);

  const publicHost = await invoke("POST", {
    action: "create",
    displayName: "Public Host",
    isPrivate: false
  });
  assert.equal(publicHost.statusCode, 201);

  const publicList = await invoke("GET");
  assert.equal(publicList.statusCode, 200);
  assert.equal(publicList.body.rooms.length, 1);
  assert.equal(publicList.body.rooms[0].code, publicHost.body.room.code);

  const hostReady = await invoke("POST", { action: "heartbeat", ready: true }, privateHost.body.token);
  assert.equal(hostReady.statusCode, 200);
  const guestReady = await invoke("POST", { action: "heartbeat", ready: true }, privateGuest.body.token);
  assert.equal(guestReady.statusCode, 200);

  const readyRoom = await invoke("GET", {}, privateHost.body.token, { code: privateCode });
  assert.equal(readyRoom.body.room.status, "ready");
  assert.equal(readyRoom.body.room.players.every((player) => player.ready), true);

  const afterReadyList = await invoke("GET");
  assert.equal(afterReadyList.body.rooms.length, 1);
  assert.equal(afterReadyList.body.rooms[0].code, publicHost.body.room.code);

  const guestLeave = await invoke("POST", { action: "leave" }, privateGuest.body.token);
  assert.equal(guestLeave.statusCode, 200);
  const hostLeave = await invoke("POST", { action: "leave" }, privateHost.body.token);
  assert.equal(hostLeave.statusCode, 200);

  const publicLeave = await invoke("POST", { action: "leave" }, publicHost.body.token);
  assert.equal(publicLeave.statusCode, 200);
  const finalList = await invoke("GET");
  assert.deepEqual(finalList.body.rooms, []);
});
