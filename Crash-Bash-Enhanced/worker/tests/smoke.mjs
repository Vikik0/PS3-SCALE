import assert from "node:assert/strict";
import { webcrypto } from "node:crypto";
import { CrashBashLobby } from "../src/index.js";

globalThis.crypto ??= webcrypto;

const data = new Map();
const ctx = {
  storage: {
    async get(key) {
      return data.get(key);
    },
    async put(key, value) {
      data.set(key, structuredClone(value));
    }
  }
};

const lobby = new CrashBashLobby(ctx, {});
async function call(path, method = "GET", body, token) {
  const headers = new Headers();
  if (body !== undefined) headers.set("Content-Type", "application/json");
  if (token) headers.set("Authorization", "Bearer " + token);
  const request = new Request("https://crash-bash.test" + path, {
    method,
    headers,
    body: body === undefined ? undefined : JSON.stringify(body)
  });
  const response = await lobby.fetch(request);
  return { status: response.status, body: await response.json() };
}

const health = await call("/health");
assert.equal(health.status, 200);
assert.equal(health.body.psn_required, false);
assert.equal(health.body.max_players, 4);

const compatibility = await call("/v1/compatibility");
assert.equal(compatibility.body.game, "Crash Bash");
assert.equal(compatibility.body.local_player_capacity, 4);

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

const peer = await call("/v1/rooms/" + code + "/peer", "GET", undefined, players[0].token);
assert.equal(peer.status, 200);
assert.equal(peer.body.players.length, 3);

const left = await call("/v1/rooms/" + code + "/leave", "POST", {}, players[0].token);
assert.equal(left.status, 200);
const afterHostLeave = await call("/v1/rooms/" + code, "GET", undefined, players[1].token);
assert.equal(afterHostLeave.status, 200);
assert.equal(afterHostLeave.body.status, "waiting");
assert.equal(afterHostLeave.body.players.length, 3);
assert.equal(afterHostLeave.body.players.find(p => p.player_id === players[1].session_id).is_host, true);

const mismatch = await call("/v1/rooms/AAAAAA/ready", "POST", { ready: true }, players[1].token);
assert.equal(mismatch.status, 404);

const unauthorized = await call("/v1/rooms/" + code, "GET");
assert.equal(unauthorized.status, 401);

console.log("Crash Bash Online lobby tests: PASS");
