"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const Module = require("node:module");

const rooms = new Map();
let revision = 0;
let lookupCalls = [];
let lookupResult = { ready: false, state: "waiting_for_host_start" };
const blobMock = {
  async get(pathname) {
    const entry = rooms.get(pathname);
    if (!entry) return null;
    return {
      statusCode: 200,
      stream: new Blob([JSON.stringify(entry.room)]).stream(),
      blob: { etag: entry.etag }
    };
  },
  async put(pathname, body, options = {}) {
    const existing = rooms.get(pathname);
    if (options.allowOverwrite) {
      if (!existing || existing.etag !== options.ifMatch) {
        const error = new Error("Conditional write conflict");
        error.name = "BlobPreconditionFailedError";
        throw error;
      }
    } else if (existing) {
      const error = new Error("Room already exists");
      error.name = "BlobPreconditionFailedError";
      throw error;
    }
    rooms.set(pathname, { room: JSON.parse(String(body)), etag: "etag-" + (++revision) });
    return {};
  },
  async list(options = {}) {
    const blobs = [...rooms.keys()]
      .filter((pathname) => pathname.startsWith(options.prefix || ""))
      .map((pathname) => ({ pathname }));
    return { blobs, cursor: null };
  },
  async del(pathname) {
    rooms.delete(pathname);
  }
};

process.env.BLOB_READ_WRITE_TOKEN = "test-blob-token";
process.env.CB_TOKEN_SECRET = "test-only-room-secret-that-is-longer-than-forty-characters";

const originalLoad = Module._load;
Module._load = function (request, parent, isMain) {
  if (request === "@vercel/blob") return blobMock;
  if (request === "./_lib/netplay" && parent?.filename?.endsWith("/web/api/rooms.js")) {
    return {
      async lookupRetroArchHost(nickname, options) {
        lookupCalls.push({ nickname, options });
        return structuredClone(lookupResult);
      }
    };
  }
  return originalLoad.call(this, request, parent, isMain);
};
const handler = require("../api/rooms");
Module._load = originalLoad;

function makeResponse() {
  return {
    statusCode: 200,
    headers: {},
    setHeader(name, value) { this.headers[name] = value; },
    status(code) { this.statusCode = code; return this; },
    json(body) { this.body = body; return this; }
  };
}

async function call(body, token = "", method = "POST", query = {}) {
  const response = makeResponse();
  const req = {
    method,
    body,
    query,
    headers: token ? { authorization: "Bearer " + token } : {}
  };
  await handler(req, response);
  return response;
}

async function createPlayer(name) {
  const response = await call({ action: "create", displayName: name });
  assert.equal(response.statusCode, 201);
  return { ...response.body.player, token: response.body.token, code: response.body.room.code };
}

async function joinPlayer(code, name) {
  const response = await call({ action: "join", code, displayName: name });
  assert.equal(response.statusCode, 200);
  return { ...response.body.player, token: response.body.token, code };
}

async function ready(player, value = true) {
  return call({ action: "set_ready", ready: value }, player.token);
}

async function confirmDisc(player, fingerprint) {
  return call({ action: "set_disc", fingerprint }, player.token);
}

test.beforeEach(() => {
  rooms.clear();
  revision = 0;
  lookupCalls = [];
  lookupResult = { ready: true, state: "ready", address: "relay.libretro.com", port: 55435, mode: "relay", mitmSession: "AbCdEf0123456789+/==" };
});

test("room launch locks membership and returns the exact host identity used by Netplay", async () => {
  const host = await createPlayer("Host Player");
  const guest = await joinPlayer(host.code, "Guest Player");

  assert.equal((await ready(host)).statusCode, 200);
  assert.equal((await ready(guest)).statusCode, 200);
  assert.equal((await confirmDisc(host, "AAAAAAAAAAAAAAAA")).statusCode, 200);
  assert.equal((await confirmDisc(guest, "BBBBBBBBBBBBBBBB")).statusCode, 200);

  let response = await call({ action: "game_start" }, host.token);
  assert.equal(response.statusCode, 409, "different disc fingerprints must block launch");

  assert.equal((await confirmDisc(guest, "AAAAAAAAAAAAAAAA")).statusCode, 200);
  response = await call({ action: "game_start" }, host.token);
  assert.equal(response.statusCode, 200);
  const netplayId = response.body.room.netplayId;
  assert.match(netplayId, /^[A-F0-9]{16}$/);
  assert.equal(response.body.room.gameStarted, true);

  const hostLookup = await call({ action: "netplay" }, host.token);
  assert.equal(hostLookup.body.state, "waiting_for_host_start");
  const clientLookup = await call({ action: "netplay" }, guest.token);
  assert.equal(clientLookup.statusCode, 200);
  assert.equal(clientLookup.body.ready, true);
  assert.equal(lookupCalls.length, 1);
  assert.equal(lookupCalls[0].nickname, "CB-" + netplayId + "-HOST");

  assert.equal((await ready(guest, false)).statusCode, 409, "READY cannot mutate a locked match");
  const heartbeat = await call({ action: "heartbeat", ready: false }, guest.token);
  assert.equal(heartbeat.statusCode, 200);
  assert.equal(heartbeat.body.players.find((player) => player.id === guest.id).ready, true,
    "a stale heartbeat must not undo the locked READY state");
  assert.equal((await call({ action: "clear_disc" }, guest.token)).statusCode, 409);
  assert.equal((await call({ action: "leave" }, guest.token)).statusCode, 409);
  assert.equal((await call({ action: "join", code: host.code, displayName: "Late Player" })).statusCode, 409);

  const realNow = Date.now;
  try {
    const startedAt = Date.now();
    Date.now = () => startedAt + 30001;
    assert.equal((await call({ action: "game_start_cancel" }, host.token)).statusCode, 409,
      "launch rollback must expire after the documented 30 seconds");
  } finally {
    Date.now = realNow;
  }
});

test("an expired host is pruned and the first active player is promoted", async () => {
  const host = await createPlayer("Expired Host");
  const guest = await joinPlayer(host.code, "Active Guest");
  const pathname = "cb-rooms/" + host.code + ".json";
  const record = rooms.get(pathname);
  const now = Date.now();
  record.room.players[0].lastSeen = now - 10 * 60 * 1000 - 1;

  const response = await call({ action: "heartbeat" }, guest.token);
  assert.equal(response.statusCode, 200);
  assert.equal(response.body.hostPlayerId, guest.id);
  assert.equal(response.body.players.length, 1);
  assert.equal(response.body.players[0].ready, false);
});

test("host departure promotes a live member and resets stale readiness", async () => {
  const host = await createPlayer("Leaving Host");
  const guest = await joinPlayer(host.code, "Next Host");
  await ready(host);
  await ready(guest);

  assert.equal((await call({ action: "leave" }, host.token)).statusCode, 200);
  const response = await call(null, guest.token, "GET", { code: host.code });
  assert.equal(response.statusCode, 200);
  assert.equal(response.body.room.hostPlayerId, guest.id);
  assert.equal(response.body.room.status, "waiting");
  assert.equal(response.body.room.players.length, 1);
});
