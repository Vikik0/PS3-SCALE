import assert from "node:assert/strict";
import { webcrypto } from "node:crypto";
import { CrashBashLobby } from "../src/index.js";

globalThis.crypto ??= webcrypto;

const data = new Map();
const sockets = [];
const ctx = {
  storage: {
    async get(key) {
      return data.get(key);
    },
    async put(key, value) {
      data.set(key, structuredClone(value));
    }
  },
  getWebSockets() {
    return sockets;
  }
};

class FakeSocket {
  constructor(attachment) {
    this.attachment = structuredClone(attachment);
    this.sent = [];
    this.closed = null;
  }
  deserializeAttachment() {
    return structuredClone(this.attachment);
  }
  serializeAttachment(value) {
    this.attachment = structuredClone(value);
  }
  send(value) {
    this.sent.push(value);
  }
  close(code, reason) {
    this.closed = { code, reason };
  }
}

const lobby = new CrashBashLobby(ctx, {});
async function call(path, method = "GET", body, token, extraHeaders = {}) {
  const headers = new Headers();
  if (body !== undefined) headers.set("Content-Type", "application/json");
  if (token) headers.set("Authorization", "Bearer " + token);
  for (const [name, value] of Object.entries(extraHeaders)) headers.set(name, value);
  const request = new Request("https://crash-bash.test" + path, {
    method,
    headers,
    body: body === undefined ? undefined : JSON.stringify(body)
  });
  const response = await lobby.fetch(request);
  return { status: response.status, body: await response.json() };
}

const oversized = await call("/v1/sessions", "POST", { mode: "guest", padding: "x".repeat(9000) });
assert.equal(oversized.status, 413);
assert.equal(oversized.body.error, "BODY_TOO_LARGE");

const health = await call("/health");
assert.equal(health.status, 200);
assert.equal(health.body.psn_required, false);
assert.equal(health.body.max_players, 4);
assert.equal(health.body.transport, "websocket-room-relay-prototype");

const compatibility = await call("/v1/compatibility");
assert.equal(compatibility.body.game, "Crash Bash");
assert.equal(compatibility.body.local_player_capacity, 4);
assert.deepEqual(
  compatibility.body.supported_disc_serials.map(item => item.serial),
  ["SCUS-94570", "SCES-02834", "SCPS-10140", "SCPS-91319"]
);

const players = [];
for (let i = 1; i <= 5; i++) {
  const result = await call("/v1/sessions", "POST", {
    mode: "guest",
    display_name: "Player " + i
  });
  assert.equal(result.status, 201);
  assert.equal(result.body.psn_required, false);
  assert.equal(result.body.token.length, 64);
  players.push(result.body);
}

const protocolAuthRequest = new Request("https://crash-bash.test/v1/rooms/ABC123/transport", {
  headers: { "Sec-WebSocket-Protocol": "cbash-v1, cbash-auth." + players[0].token }
});
const protocolAuthSession = await lobby.authenticate(protocolAuthRequest, await lobby.load());
assert.equal(protocolAuthSession.sessionId, players[0].session_id);

const created = await call("/v1/rooms", "POST", { max_players: 4, is_private: true }, players[0].token);
assert.equal(created.status, 201);
assert.equal(created.body.player_count, 1);
assert.equal(created.body.max_players, 4);
const code = created.body.room_code;

for (let i = 1; i <= 3; i++) {
  const joined = await call("/v1/rooms/" + code + "/join", "POST", {}, players[i].token);
  assert.equal(joined.status, 200);
  assert.equal(joined.body.player_count, i + 1);
}
const full = await call("/v1/rooms/" + code + "/join", "POST", {}, players[4].token);
assert.equal(full.status, 409);
assert.equal(full.body.error, "ROOM_FULL");

const current = await call("/v1/rooms/current", "GET", undefined, players[0].token);
assert.equal(current.status, 200);
assert.equal(current.body.room.room_code, code);

const earlyStart = await call("/v1/rooms/" + code + "/start", "POST", {}, players[0].token);
assert.equal(earlyStart.status, 409);
assert.equal(earlyStart.body.error, "PLAYERS_NOT_READY");

for (const player of players.slice(0, 4)) {
  const ready = await call("/v1/rooms/" + code + "/ready", "POST", { ready: true }, player.token);
  assert.equal(ready.status, 200);
}
const started = await call("/v1/rooms/" + code + "/start", "POST", {}, players[0].token);
assert.equal(started.status, 200);
assert.equal(started.body.status, "running");

// The room relay wraps opaque game packets with the authenticated sender ID.
const relayA = new FakeSocket({
  roomCode: code,
  sessionId: players[0].session_id,
  rateWindowStartedAt: 0,
  rateCount: 0
});
const relayB = new FakeSocket({
  roomCode: code,
  sessionId: players[1].session_id,
  rateWindowStartedAt: 0,
  rateCount: 0
});
const otherRoom = new FakeSocket({
  roomCode: "ZZZZZZ",
  sessionId: players[2].session_id,
  rateWindowStartedAt: 0,
  rateCount: 0
});
sockets.push(relayA, relayB, otherRoom);
lobby.webSocketMessage(relayA, new Uint8Array([0x10, 0x20, 0x30]).buffer);
assert.equal(relayB.sent.length, 1);
assert.equal(otherRoom.sent.length, 0);
const relayed = new Uint8Array(relayB.sent[0]);
assert.deepEqual(Array.from(relayed.slice(0, 4)), [0x43, 0x42, 0x52, 0x31]);
assert.deepEqual(Array.from(relayed.slice(4, 20)), players[0].session_id.match(/../g).map(byte => Number.parseInt(byte, 16)));
assert.deepEqual(Array.from(relayed.slice(20, 22)), [0, 3]);
assert.deepEqual(Array.from(relayed.slice(22)), [0x10, 0x20, 0x30]);

const textSocket = new FakeSocket({
  roomCode: code,
  sessionId: players[3].session_id,
  rateWindowStartedAt: 0,
  rateCount: 0
});
lobby.webSocketMessage(textSocket, "not a binary game packet");
assert.equal(textSocket.closed.code, 1003);

const largeSocket = new FakeSocket({
  roomCode: code,
  sessionId: players[3].session_id,
  rateWindowStartedAt: 0,
  rateCount: 0
});
lobby.webSocketMessage(largeSocket, new Uint8Array(1201).buffer);
assert.equal(largeSocket.closed.code, 1009);

const rateSocket = new FakeSocket({
  roomCode: code,
  sessionId: players[3].session_id,
  rateWindowStartedAt: 0,
  rateCount: 0
});
for (let i = 0; i < 6; i++) {
  lobby.webSocketMessage(rateSocket, new Uint8Array([i]).buffer);
}
assert.equal(rateSocket.closed.code, 1008);
assert.equal(rateSocket.closed.reason, "RATE_LIMITED");

const invalidEndpoint = await call(
  "/v1/rooms/" + code + "/endpoint",
  "POST",
  { public_port: 47000 },
  players[0].token,
  { "CF-Connecting-IP": "not-an-ip" }
);
assert.equal(invalidEndpoint.status, 400);
assert.equal(invalidEndpoint.body.error, "INVALID_PUBLIC_IP");

const published = await call(
  "/v1/rooms/" + code + "/endpoint",
  "POST",
  { public_port: 47000 },
  players[0].token,
  { "CF-Connecting-IP": "198.51.100.10" }
);
assert.equal(published.status, 200);
assert.equal(published.body.host_endpoint.address, "198.51.100.10");
assert.equal(published.body.host_endpoint.port, 47000);

const guestPublish = await call(
  "/v1/rooms/" + code + "/endpoint",
  "POST",
  { public_port: 48000 },
  players[1].token,
  { "CF-Connecting-IP": "198.51.100.20" }
);
assert.equal(guestPublish.status, 403);

const peer = await call("/v1/rooms/" + code + "/peer", "GET", undefined, players[1].token);
assert.equal(peer.status, 200);
assert.equal(peer.body.players.length, 3);
assert.equal(peer.body.host_endpoint.address, "198.51.100.10");
assert.equal(peer.body.host_endpoint.port, 47000);

const endpoint = await call("/v1/rooms/" + code + "/endpoint", "GET", undefined, players[2].token);
assert.equal(endpoint.status, 200);
assert.equal(endpoint.body.host_endpoint.address, "198.51.100.10");

const left = await call("/v1/rooms/" + code + "/leave", "POST", {}, players[0].token);
assert.equal(left.status, 200);
assert.equal(relayA.closed.code, 1008);
assert.equal(relayB.closed.code, 1008);
assert.equal(otherRoom.closed, null);
const afterHostLeave = await call("/v1/rooms/" + code, "GET", undefined, players[1].token);
assert.equal(afterHostLeave.status, 200);
assert.equal(afterHostLeave.body.status, "waiting");
assert.equal(afterHostLeave.body.players.length, 3);
assert.equal(afterHostLeave.body.players.find(p => p.player_id === players[1].session_id).is_host, true);
assert.equal(afterHostLeave.body.host_endpoint_ready, false);

const wrongCode = (code[0] === "A" ? "B" : "A") + code.slice(1);
const mismatch = await call("/v1/rooms/" + wrongCode + "/ready", "POST", { ready: true }, players[1].token);
assert.equal(mismatch.status, 404);

const unauthorized = await call("/v1/rooms/" + code, "GET");
assert.equal(unauthorized.status, 401);

console.log("Crash Bash Online lobby tests: PASS");
