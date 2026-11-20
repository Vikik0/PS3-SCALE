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

const health = await call("/health");
assert.equal(health.status, 200);
assert.equal(health.body.transport, "peer-to-peer-udp");

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

const guestPublish = await call("/v1/rooms/" + code + "/candidate", "POST", guestCandidate, guest.body.token);
assert.equal(guestPublish.status, 200);
assert.deepEqual(guestPublish.body.peer_candidate, hostCandidate);
assert.equal(guestPublish.body.peer.relay_id, host.body.relay.relay_id);
assert.equal(guestPublish.body.peer.relay_key, host.body.relay.relay_key);

const peer = await call("/v1/rooms/" + code + "/peer", "GET", undefined, host.body.token);
assert.equal(peer.status, 200);
assert.deepEqual(peer.body.peer_candidate, guestCandidate);
assert.equal(peer.body.peer.relay_id, guest.body.relay.relay_id);
assert.equal(peer.body.peer.relay_key, guest.body.relay.relay_key);

const left = await call("/v1/rooms/" + code + "/leave", "POST", {}, guest.body.token);
assert.equal(left.status, 200);
const waiting = await call("/v1/rooms/" + code, "GET", undefined, host.body.token);
assert.equal(waiting.status, 200);
assert.equal(waiting.body.status, "waiting");
assert.equal(waiting.body.player_count, 1);

const unauthenticated = await call("/v1/rooms/" + code);
assert.equal(unauthenticated.status, 401);

console.log("Cloudflare lobby Durable Object smoke tests: PASS");
