"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const Module = require("node:module");

const storage = { blobs: new Map(), sequence: 0 };
const originalLoad = Module._load;
const originalFetch = global.fetch;
const originalNow = Date.now;

function blobError(name, message) {
  const error = new Error(message);
  error.name = name;
  return error;
}

const blobMock = {
  async get(pathname) {
    const item = storage.blobs.get(pathname);
    if (!item) return { statusCode: 404 };
    const stream = new ReadableStream({
      start(controller) {
        controller.enqueue(new TextEncoder().encode(item.body));
        controller.close();
      }
    });
    return {
      statusCode: 200,
      stream,
      blob: { etag: item.etag }
    };
  },

  async put(pathname, body, options) {
    const current = storage.blobs.get(pathname);
    if (options && options.ifMatch) {
      if (!current || current.etag !== options.ifMatch) {
        throw blobError("BlobPreconditionFailedError", "etag mismatch");
      }
    } else if (current && !(options && options.allowOverwrite)) {
      throw blobError("BlobAlreadyExistsError", "blob already exists");
    }

    storage.sequence += 1;
    storage.blobs.set(pathname, {
      body: String(body),
      etag: "etag-" + storage.sequence
    });
    return {};
  },

  async list(options) {
    const all = Array.from(storage.blobs.keys())
      .filter((pathname) => pathname.startsWith(options.prefix))
      .sort()
      .map((pathname) => ({ pathname }));
    const start = options.cursor ? Number(options.cursor) : 0;
    const end = Math.min(start + (options.limit || 100), all.length);
    return {
      blobs: all.slice(start, end),
      cursor: end < all.length ? String(end) : null,
      hasMore: end < all.length
    };
  },

  async del(pathname) {
    storage.blobs.delete(pathname);
  }
};

Module._load = function(request, parent, isMain) {
  if (request === "@vercel/blob") return blobMock;
  return originalLoad.call(this, request, parent, isMain);
};

process.env.BLOB_READ_WRITE_TOKEN = "test-blob-token";
process.env.CB_TOKEN_SECRET = "test-only-secret-with-more-than-forty-characters-123456";

let handler;

function loadHandler() {
  const path = require.resolve("../api/rooms");
  delete require.cache[path];
  handler = require("../api/rooms");
}

function responseMock() {
  return {
    statusCode: 200,
    headers: {},
    payload: undefined,
    setHeader(name, value) {
      this.headers[name] = value;
      return this;
    },
    status(code) {
      this.statusCode = code;
      return this;
    },
    json(data) {
      this.payload = data;
      return this;
    }
  };
}

async function callApi(method, body, token, query) {
  const req = {
    method,
    body,
    query: query || {},
    headers: token ? { authorization: "Bearer " + token } : {}
  };
  const res = responseMock();
  await handler(req, res);
  return { status: res.statusCode, data: res.payload, headers: res.headers };
}

async function createPlayer(name, isPrivate) {
  const result = await callApi("POST", {
    action: "create",
    displayName: name,
    isPrivate: Boolean(isPrivate)
  });
  assert.equal(result.status, 201);
  return result.data;
}

async function joinPlayer(code, name) {
  const result = await callApi("POST", {
    action: "join",
    code,
    displayName: name
  });
  assert.equal(result.status, 200);
  return result.data;
}

async function setReady(session, ready) {
  return callApi("POST", { action: "set_ready", ready }, session.token);
}

async function setDisc(session, fingerprint) {
  return callApi("POST", {
    action: "set_disc",
    fingerprint
  }, session.token);
}

function makeRoom(code, now, isPrivate) {
  const playerId = "player-" + code;
  return {
    code,
    status: "waiting",
    isPrivate: Boolean(isPrivate),
    maxPlayers: 4,
    createdAt: now,
    updatedAt: now,
    hostPlayerId: playerId,
    netplayId: "",
    gameStarted: false,
    matchExpiresAt: 0,
    players: [{
      id: playerId,
      name: "Host " + code,
      ready: false,
      readyUntil: 0,
      discFingerprint: "",
      joinedAt: now,
      lastSeen: now
    }],
    signals: [],
    nextSignalId: 1
  };
}

function seedRoom(room) {
  storage.sequence += 1;
  storage.blobs.set("cb-rooms/" + room.code + ".json", {
    body: JSON.stringify(room),
    etag: "seed-" + storage.sequence
  });
}

function lobbyEntry(username, overrides) {
  return {
    fields: {
      username,
      updated: new Date(Date.now()).toISOString(),
      is_retroarch: true,
      connectable: true,
      host_method: 3,
      ip: "203.0.113.10",
      port: 55435,
      mitm_ip: "madrid.relay.retroarch.com",
      mitm_port: 55435,
      mitm_session: "relay-session-123",
      has_password: false,
      game_name: "Crash Bash (USA)",
      core_name: "PCSX-ReARMed",
      core_version: "test-core",
      ...(overrides || {})
    }
  };
}

test.beforeEach(() => {
  storage.blobs.clear();
  storage.sequence = 0;
  global.fetch = originalFetch;
  Date.now = originalNow;
  loadHandler();
});

test.after(() => {
  Module._load = originalLoad;
  global.fetch = originalFetch;
  Date.now = originalNow;
});

test("create and join establish a four-seat room with an authoritative host", async () => {
  const host = await createPlayer("Host");
  const guest = await joinPlayer(host.room.code, "Guest");
  const room = await callApi("GET", null, host.token, { code: host.room.code });

  assert.equal(room.status, 200);
  assert.equal(room.data.room.players.length, 2);
  assert.equal(room.data.room.hostPlayerId, host.player.id);
  assert.equal(room.data.room.status, "waiting");
  assert.equal(guest.player.name, "Guest");
  assert.equal(room.data.room.players[0].discFingerprint, "");
});

test("READY is explicit, heartbeat only refreshes presence, and READY expires after 90 seconds", async () => {
  let now = originalNow();
  Date.now = () => now;

  const host = await createPlayer("Host");
  const ready = await setReady(host, true);
  assert.equal(ready.status, 200);
  assert.equal(ready.data.players[0].ready, true);

  now += 80000;
  const heartbeat = await callApi("POST", {
    action: "heartbeat",
    ready: false
  }, host.token);
  assert.equal(heartbeat.status, 200);
  assert.equal(heartbeat.data.players[0].ready, true);

  now += 11000;
  const expired = await callApi("POST", { action: "heartbeat" }, host.token);
  assert.equal(expired.status, 200);
  assert.equal(expired.data.players[0].ready, false);
  assert.equal(expired.data.players[0].discFingerprint, "");
  assert.equal(expired.data.status, "waiting");
});

test("a membership change clears every READY state and disc confirmation", async () => {
  const host = await createPlayer("Host");
  const guest = await joinPlayer(host.room.code, "Guest");

  assert.equal((await setReady(host, true)).status, 200);
  assert.equal((await setReady(guest, true)).status, 200);
  assert.equal((await setDisc(host, "0123456789ABCDEF")).status, 200);
  assert.equal((await setDisc(guest, "0123456789ABCDEF")).status, 200);

  const third = await joinPlayer(host.room.code, "Third");
  const room = await callApi("GET", null, host.token, { code: host.room.code });

  assert.equal(third.player.name, "Third");
  assert.equal(room.data.room.players.length, 3);
  assert.equal(room.data.room.players.every((player) => !player.ready), true);
  assert.equal(room.data.room.discSelectionCount, 0);
  assert.equal(room.data.room.discMatch, false);
});

test("host departure promotes the next active player and resets readiness", async () => {
  const host = await createPlayer("Host");
  const guest = await joinPlayer(host.room.code, "Guest");
  await setReady(host, true);
  await setReady(guest, true);

  const left = await callApi("POST", { action: "leave" }, host.token);
  const room = await callApi("GET", null, guest.token, { code: host.room.code });

  assert.equal(left.status, 200);
  assert.equal(room.data.room.hostPlayerId, guest.player.id);
  assert.equal(room.data.room.players.length, 1);
  assert.equal(room.data.room.players[0].ready, false);
});

test("game start requires matching disc fingerprints and rejects late joins", async () => {
  const host = await createPlayer("Host");
  const guest = await joinPlayer(host.room.code, "Guest");
  await setReady(host, true);
  await setReady(guest, true);

  const mismatchA = await setDisc(host, "0123456789ABCDEF");
  const mismatchB = await setDisc(guest, "FEDCBA9876543210");
  assert.equal(mismatchA.status, 200);
  assert.equal(mismatchB.status, 200);

  const blocked = await callApi("POST", { action: "game_start" }, host.token);
  assert.equal(blocked.status, 409);
  assert.match(blocked.data.error, /different disc/i);

  await setDisc(guest, "0123456789ABCDEF");
  const nonHost = await callApi("POST", { action: "game_start" }, guest.token);
  assert.equal(nonHost.status, 403);

  const started = await callApi("POST", { action: "game_start" }, host.token);
  assert.equal(started.status, 200);
  assert.equal(started.data.ok, true);

  const lateJoin = await callApi("POST", {
    action: "join",
    code: host.room.code,
    displayName: "Late"
  });
  assert.equal(lateJoin.status, 409);

  const room = await callApi("GET", null, guest.token, { code: host.room.code });
  assert.equal(room.data.room.gameStarted, true);
  assert.equal(room.data.room.status, "started");
  assert.equal(room.data.room.netplayId.length, 12);
});

test("client Netplay discovery returns the exact fresh RetroArch relay endpoint", async () => {
  const host = await createPlayer("Host");
  const guest = await joinPlayer(host.room.code, "Guest");
  await setReady(host, true);
  await setReady(guest, true);
  await setDisc(host, "0123456789ABCDEF");
  await setDisc(guest, "0123456789ABCDEF");
  await callApi("POST", { action: "game_start" }, host.token);

  const room = await callApi("GET", null, guest.token, { code: host.room.code });
  const nickname = "CB-" + room.data.room.netplayId + "-HOST";
  global.fetch = async (url, options) => {
    assert.equal(url, "https://lobby.libretro.com/list/");
    assert.equal(options.headers.Accept, "application/json");
    return new Response(JSON.stringify([lobbyEntry(nickname)]), {
      status: 200,
      headers: { "Content-Type": "application/json" }
    });
  };

  const result = await callApi("POST", { action: "netplay" }, guest.token);
  assert.equal(result.status, 200);
  assert.equal(result.data.ready, true);
  assert.equal(result.data.mode, "relay");
  assert.equal(result.data.address, "madrid.relay.retroarch.com");
  assert.equal(result.data.port, 55435);
  assert.equal(result.data.mitmSession, "relay-session-123");
  assert.equal(result.data.coreName, "PCSX-ReARMed");
});

test("Netplay discovery supports direct connections and refuses password-protected hosts", async () => {
  const host = await createPlayer("Host");
  const guest = await joinPlayer(host.room.code, "Guest");
  await setReady(host, true);
  await setReady(guest, true);
  await setDisc(host, "0123456789ABCDEF");
  await setDisc(guest, "0123456789ABCDEF");
  await callApi("POST", { action: "game_start" }, host.token);

  const room = await callApi("GET", null, guest.token, { code: host.room.code });
  const nickname = "CB-" + room.data.room.netplayId + "-HOST";
  global.fetch = async () => new Response(JSON.stringify([
    lobbyEntry(nickname, {
      host_method: 0,
      mitm_ip: "",
      mitm_port: 0,
      mitm_session: "",
      ip: "198.51.100.25",
      port: 55435
    })
  ]), { status: 200 });

  const direct = await callApi("POST", { action: "netplay" }, guest.token);
  assert.equal(direct.data.ready, true);
  assert.equal(direct.data.mode, "direct");
  assert.equal(direct.data.address, "198.51.100.25");
  assert.equal(direct.data.mitmSession, "");

  loadHandler();
  global.fetch = async () => new Response(JSON.stringify([
    lobbyEntry(nickname, { has_password: true })
  ]), { status: 200 });

  const password = await callApi("POST", { action: "netplay" }, guest.token);
  assert.equal(password.data.ready, false);
  assert.equal(password.data.state, "host_password_required");
});

test("invalid disc fingerprints are rejected before they can enter room state", async () => {
  const host = await createPlayer("Host");
  await setReady(host, true);
  const result = await setDisc(host, "../../etc/passwd");
  assert.equal(result.status, 400);
});

test("public matchmaking follows Blob pagination beyond the first 100 records", async () => {
  const now = Date.now();
  for (let i = 0; i < 100; i += 1) {
    const code = "AAAA" + String(i).padStart(2, "0");
    seedRoom(makeRoom(code, now, true));
  }
  const publicRoom = makeRoom("ZZZZ23", now, false);
  seedRoom(publicRoom);

  const result = await callApi("GET");
  assert.equal(result.status, 200);
  assert.equal(result.data.rooms.length, 1);
  assert.equal(result.data.rooms[0].code, "ZZZZ23");
});

test("health configuration accepts the versioned token secret and reports protocol 0.7.0", async () => {
  process.env.CB_TOKEN_SECRET_V2 = "versioned-secret-with-more-than-forty-characters-987654";
  const healthPath = require.resolve("../api/health");
  delete require.cache[healthPath];
  const health = require("../api/health");
  const res = responseMock();
  health({}, res);

  assert.equal(res.statusCode, 200);
  assert.equal(res.payload.version, "0.7.0");
  assert.equal(res.payload.storageConfigured, true);
  assert.equal(res.payload.tokenConfigured, true);
  assert.equal(res.payload.gameplayTransport, "RetroArch Netplay");
  delete process.env.CB_TOKEN_SECRET_V2;
});
