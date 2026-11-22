import assert from "node:assert/strict";
import { webcrypto } from "node:crypto";
import { LobbyDurableObject } from "../src/index.js";

globalThis.crypto ??= webcrypto;

const store = new Map();
const ctx = {
  storage: {
    async get(key) {
      return store.get(key);
    },
    async put(key, value) {
      store.set(key, structuredClone(value));
    }
  }
};

const lobby = new LobbyDurableObject(ctx, {});
async function call(path, method = "GET", payload = undefined, token = undefined) {
  const headers = new Headers();
  if (payload !== undefined) headers.set("Content-Type", "application/json");
  if (token) headers.set("Authorization", "Bearer " + token);
  const request = new Request("https://worker.test" + path, {
    method,
    headers,
    body: payload === undefined ? undefined : JSON.stringify(payload)
  });
  const response = await lobby.fetch(request);
  return { status: response.status, body: await response.json() };
}


class FakeSocket {
  constructor() {
    this.readyState = 1;
    this.bufferedAmount = 0;
    this.listeners = new Map();
    this.sent = [];
    this.closeCode = null;
    this.closeReason = null;
  }
  accept() {}
  addEventListener(type, handler) {
    const list = this.listeners.get(type) || [];
    list.push(handler);
    this.listeners.set(type, list);
  }
  send(data) {
    this.sent.push(data instanceof Uint8Array ? new Uint8Array(data) : new Uint8Array(data));
  }
  close(code = 1000, reason = "") {
    if (this.readyState !== 1) return;
    this.readyState = 3;
    this.closeCode = code;
    this.closeReason = reason;
    for (const handler of this.listeners.get("close") || []) handler({});
  }
  async emitMessage(data) {
    for (const handler of this.listeners.get("message") || []) await handler({ data });
  }
}

class FakeWebSocketPair {
  static pairs = [];
  constructor() {
    this[0] = new FakeSocket();
    this[1] = new FakeSocket();
    FakeWebSocketPair.pairs.push(this);
  }
}

function testHexToBytes(value) {
  return new Uint8Array(value.match(/.{2}/g).map(part => Number.parseInt(part, 16)));
}

async function testHmac16(keyHex, data) {
  const key = await crypto.subtle.importKey("raw", testHexToBytes(keyHex), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  return new Uint8Array(await crypto.subtle.sign("HMAC", key, data)).slice(0, 16);
}

async function makeClientFrame(relayIdHex, relayKeyHex, sequence, payloadText) {
  const payload = new TextEncoder().encode(payloadText);
  const frame = new Uint8Array(24 + payload.length + 16);
  frame.set(new TextEncoder().encode("GHC2"), 0);
  frame.set(testHexToBytes(relayIdHex), 4);
  frame[20] = (sequence >>> 24) & 255;
  frame[21] = (sequence >>> 16) & 255;
  frame[22] = (sequence >>> 8) & 255;
  frame[23] = sequence & 255;
  frame.set(payload, 24);
  frame.set(await testHmac16(relayKeyHex, frame.subarray(0, frame.length - 16)), frame.length - 16);
  return frame;
}

async function readServerFrame(frame, relayKeyHex) {
  assert.equal(new TextDecoder().decode(frame.subarray(0, 4)), "GHC2");
  const expectedMac = await testHmac16(relayKeyHex, frame.subarray(0, frame.length - 16));
  assert.deepEqual(frame.subarray(frame.length - 16), expectedMac);
  const sequence = (((frame[5] << 24) >>> 0) | (frame[6] << 16) | (frame[7] << 8) | frame[8]) >>> 0;
  return {
    slot: frame[4],
    sequence,
    payload: new TextDecoder().decode(frame.subarray(9, frame.length - 16))
  };
}

const health = await call("/health");
assert.equal(health.status, 200);
assert.equal(health.body.transport, "peer-to-peer-udp-with-websocket-fallback");
assert.equal(health.body.relay_fallback, true);

const compatibility = await call("/v1/compatibility");
assert.equal(compatibility.status, 200);
assert.equal(compatibility.body.title_ids.length, 10);

const host = await call("/v1/sessions", "POST", { mode: "guest" });
const guest = await call("/v1/sessions", "POST", { mode: "guest" });
const third = await call("/v1/sessions", "POST", { mode: "guest" });
assert.equal(host.status, 201);
assert.equal(guest.status, 201);
assert.equal(host.body.token.length, 64);
assert.equal(host.body.relay.relay_key.length, 64);

const psn = await call("/v1/sessions", "POST", { mode: "psn" });
assert.equal(psn.status, 501);

const created = await call("/v1/rooms", "POST", {}, host.body.token);
assert.equal(created.status, 201);
const code = created.body.room_code;
assert.equal(created.body.player_count, 1);

const joined = await call("/v1/rooms/" + code + "/join", "POST", {}, guest.body.token);
assert.equal(joined.status, 200);
assert.equal(joined.body.player_count, 2);

const full = await call("/v1/rooms/" + code + "/join", "POST", {}, third.body.token);
assert.equal(full.status, 409);
assert.equal(full.body.error, "ROOM_FULL");

const wrongCode = (code[0] === "A" ? "B" : "A") + code.slice(1);
const wrongReady = await call("/v1/rooms/" + wrongCode + "/ready", "POST", { ready: true }, host.body.token);
assert.equal(wrongReady.status, 404);

const hostReady = await call("/v1/rooms/" + code + "/ready", "POST", { ready: true }, host.body.token);
const guestReady = await call("/v1/rooms/" + code + "/ready", "POST", { ready: true }, guest.body.token);
assert.equal(hostReady.status, 200);
assert.equal(guestReady.status, 200);

const started = await call("/v1/rooms/" + code + "/start", "POST", {}, host.body.token);
assert.equal(started.status, 200);
assert.equal(started.body.status, "running");

const hostCandidate = {
  public_address: "198.51.100.10",
  public_port: 41000,
  local_address: "192.168.1.10",
  local_port: 41000,
  nat_type: "moderate"
};
const guestCandidate = {
  public_address: "198.51.100.20",
  public_port: 42000,
  local_address: "192.168.1.20",
  local_port: 42000,
  nat_type: "open"
};
const hostPublish = await call("/v1/rooms/" + code + "/candidate", "POST", hostCandidate, host.body.token);
assert.equal(hostPublish.status, 200);
assert.equal(hostPublish.body.peer_candidate, null);

assert.equal(hostPublish.body.relay_fallback, true);
assert.equal(hostPublish.body.relay_websocket_path, "/v1/rooms/" + code + "/relay");

const guestPublish = await call("/v1/rooms/" + code + "/candidate", "POST", guestCandidate, guest.body.token);
assert.equal(guestPublish.status, 200);
assert.equal(guestPublish.body.peer_candidate.public_address, hostCandidate.public_address);
assert.equal(guestPublish.body.peer_candidate.public_port, hostCandidate.public_port);
assert.equal(guestPublish.body.peer_candidate.local_address, hostCandidate.local_address);
assert.equal(guestPublish.body.peer_candidate.nat_type, hostCandidate.nat_type);
assert.ok(guestPublish.body.peer_candidate.updated_at);
assert.equal(guestPublish.body.peer.relay_id, host.body.relay.relay_id);
assert.equal(guestPublish.body.peer.relay_key, host.body.relay.relay_key);

const peer = await call("/v1/rooms/" + code + "/peer", "GET", undefined, host.body.token);
assert.equal(peer.status, 200);
assert.equal(peer.body.peer_candidate.public_address, guestCandidate.public_address);
assert.equal(peer.body.peer_candidate.public_port, guestCandidate.public_port);
assert.equal(peer.body.peer_candidate.local_address, guestCandidate.local_address);
assert.equal(peer.body.peer_candidate.nat_type, guestCandidate.nat_type);
assert.ok(peer.body.peer_candidate.updated_at);
assert.equal(peer.body.peer.relay_id, guest.body.relay.relay_id);
assert.equal(peer.body.peer.relay_key, guest.body.relay.relay_key);


const originalResponse = globalThis.Response;
const originalWebSocketPair = globalThis.WebSocketPair;
globalThis.Response = class {
  constructor(body, init = {}) {
    this.body = body;
    this.status = init.status || 200;
    this.webSocket = init.webSocket || null;
    this.headers = new Headers(init.headers || {});
  }
};
globalThis.WebSocketPair = FakeWebSocketPair;
try {
  const hostUpgrade = await lobby.fetch({
    url: "https://worker.test/v1/rooms/" + code + "/relay",
    method: "GET",
    headers: new Headers({ Upgrade: "websocket", Authorization: "Bearer " + host.body.token })
  });
  assert.equal(hostUpgrade.status, 101);
  const hostPair = FakeWebSocketPair.pairs[0];

  const guestUpgrade = await lobby.fetch({
    url: "https://worker.test/v1/rooms/" + code + "/relay",
    method: "GET",
    headers: new Headers({ Upgrade: "websocket", Authorization: "Bearer " + guest.body.token })
  });
  assert.equal(guestUpgrade.status, 101);
  const guestPair = FakeWebSocketPair.pairs[1];

  const hostFrame = await makeClientFrame(host.body.relay.relay_id, host.body.relay.relay_key, 1, "host-state");
  await hostPair[1].emitMessage(hostFrame.buffer);
  assert.equal(guestPair[1].sent.length, 1);
  assert.deepEqual(await readServerFrame(guestPair[1].sent[0], guest.body.relay.relay_key), {
    slot: 1,
    sequence: 1,
    payload: "host-state"
  });

  await hostPair[1].emitMessage(hostFrame.buffer);
  assert.equal(guestPair[1].sent.length, 1, "duplicate GHC2 sequence must be dropped");

  const guestFrame = await makeClientFrame(guest.body.relay.relay_id, guest.body.relay.relay_key, 1, "guest-state");
  await guestPair[1].emitMessage(guestFrame.buffer);
  assert.equal(hostPair[1].sent.length, 1);
  assert.deepEqual(await readServerFrame(hostPair[1].sent[0], host.body.relay.relay_key), {
    slot: 2,
    sequence: 1,
    payload: "guest-state"
  });

  const forgedFrame = await makeClientFrame(host.body.relay.relay_id, "00".repeat(32), 2, "forged");
  await hostPair[1].emitMessage(forgedFrame.buffer);
  assert.equal(hostPair[1].closeCode, 1008, "invalid HMAC must close the sender connection");
  assert.equal(guestPair[1].closeCode, 1012, "peer must be notified when the sender disconnects");
  assert.equal(lobby.relaySockets.size, 0);
} finally {
  globalThis.Response = originalResponse;
  if (originalWebSocketPair === undefined) delete globalThis.WebSocketPair;
  else globalThis.WebSocketPair = originalWebSocketPair;
}

const left = await call("/v1/rooms/" + code + "/leave", "POST", {}, guest.body.token);
assert.equal(left.status, 200);
const waiting = await call("/v1/rooms/" + code, "GET", undefined, host.body.token);
assert.equal(waiting.status, 200);
assert.equal(waiting.body.status, "waiting");
assert.equal(waiting.body.player_count, 1);

const unauthenticated = await call("/v1/rooms/" + code);
assert.equal(unauthenticated.status, 401);

console.log("Cloudflare lobby Durable Object smoke tests: PASS");
