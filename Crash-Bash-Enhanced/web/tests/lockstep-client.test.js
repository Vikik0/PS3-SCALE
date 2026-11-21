import test from "node:test";
import assert from "node:assert/strict";
import { BrowserLockstepClient } from "../public/lockstep-client.js";
import { decodeCBN1, CBN1_TYPES } from "../public/cbn1.js";
import { createLocalPadCapture, decodePadRecord } from "../public/pad-bridge.js";

const HOST = "00112233445566778899aabbccddeeff";
const NONCE = 0x11223344;
const SESSION = 0xaabbccdd;

class FakeTransport {
  constructor() { this.listeners = new Map(); this.sent = []; this.closed = false; }
  on(name, callback) {
    if (!this.listeners.has(name)) this.listeners.set(name, new Set());
    this.listeners.get(name).add(callback);
    return () => this.listeners.get(name)?.delete(callback);
  }
  async connect() { return this; }
  sendPacket(payload, recipientId, coalesceKey) {
    this.sent.push({ payload: payload.slice(), recipientId, coalesceKey });
    return true;
  }
  close() { this.closed = true; }
  emit(name, value) { for (const callback of this.listeners.get(name) || []) callback(value); }
  fromHost(payload) { this.emit("packet", { senderId: HOST, payload }); }
}

function packet(type, bodyLength) {
  const bytes = new Uint8Array(5 + bodyLength);
  bytes.set([0x43, 0x42, 0x4e, 0x31, type]);
  return bytes;
}

function welcome() {
  const bytes = packet(CBN1_TYPES.WELCOME, 10);
  const view = new DataView(bytes.buffer);
  view.setUint32(5, NONCE, true);
  bytes[9] = 0; bytes[10] = 1;
  view.setUint32(11, SESSION, true);
  return bytes;
}

function ping() {
  const bytes = packet(CBN1_TYPES.PING, 13);
  const view = new DataView(bytes.buffer);
  view.setUint32(5, SESSION, true);
  view.setUint32(9, 0x55667788, true);
  bytes[13] = 1; bytes[14] = 2; bytes[15] = 1; bytes[16] = 1; bytes[17] = 1;
  return bytes;
}

function save(slot) {
  const bytes = packet(CBN1_TYPES.SAVE, 7);
  const view = new DataView(bytes.buffer);
  view.setUint32(5, SESSION, true);
  bytes[9] = slot;
  view.setUint16(10, 0, true);
  return bytes;
}

function start() {
  const bytes = packet(CBN1_TYPES.START, 12);
  const view = new DataView(bytes.buffer);
  view.setUint32(5, SESSION, true);
  bytes[9] = 2; bytes[10] = 2; bytes[11] = 1; bytes[12] = 0;
  bytes.set([0, 1, 1, 1], 13);
  return bytes;
}

function record(step, hostInput = 0x4000) {
  const bytes = packet(CBN1_TYPES.RECORD, 56);
  const view = new DataView(bytes.buffer);
  view.setUint32(5, SESSION, true);
  view.setInt32(9, 0, true);
  view.setInt32(13, step + 1, true);
  view.setInt32(17, -1, true);
  view.setUint32(21, 0, true);
  bytes[25] = 0; bytes[26] = 0; bytes[27] = 0;
  view.setInt32(28, step, true);
  bytes[32] = 1;
  bytes[33] = 0x03;
  bytes[34] = hostInput & 0xff;
  bytes[35] = (hostInput >>> 8) & 0xff;
  bytes.set([0x80, 0x80, 0x80, 0x80], 36);
  for (let pad = 1; pad < 4; pad++) bytes.set([0x03, 0, 0, 0x80, 0x80, 0x80, 0x80], 33 + pad * 7);
  return bytes;
}

function setupClient() {
  const transport = new FakeTransport();
  const capture = { snapshot: () => new Uint8Array([0x03, 0x01, 0, 0x80, 0x80, 0x80, 0x80]) };
  const applied = [];
  const advanced = [];
  const client = new BrowserLockstepClient({
    transport, hostSessionId: HOST, capture,
    applyPadState: (slot, buttons, sticks) => applied.push({ slot, buttons, sticks }),
    advanceFrames: async count => { advanced.push(count); },
    nonce: () => NONCE
  });
  return { transport, client, applied, advanced };
}

test("captures EmulatorJS player-one controls as the seven-byte PS1 pad layout", () => {
  const forwarded = [];
  const gameManager = {
    simulateInput: (...args) => forwarded.push(args),
    functions: { simulateInput() {} }
  };
  const capture = createLocalPadCapture(gameManager, () => true);
  gameManager.simulateInput(0, 0, 1);
  gameManager.simulateInput(0, 16, 32767);
  gameManager.simulateInput(0, 18, -32767);
  gameManager.simulateInput(1, 0, 1);
  const pad = capture.snapshot();
  assert.deepEqual(Array.from(pad), [0x03, 0, 0x40, 0, 128, 255, 128]);
  assert.equal(forwarded.length, 1, "non-local controller input is not captured");
  assert.deepEqual(decodePadRecord(pad), { connected: true, buttons: 0x4000, sticks: [0, 128, 255, 128], flags: 0x03 });
});

test("joins the PS3 CBN1 lobby, answers pings, acknowledges empty saves, and accepts START", async () => {
  const { transport, client } = setupClient();
  await client.connect();
  assert.equal(client.phase, "joining");
  const join = decodeCBN1(transport.sent[0].payload);
  assert.equal(join.type, CBN1_TYPES.JOIN);
  assert.equal(transport.sent[0].recipientId, HOST);
  assert.equal(new DataView(join.body.buffer, join.body.byteOffset).getUint32(11, true), NONCE);

  transport.fromHost(welcome());
  assert.equal(client.phase, "lobby");
  assert.equal(client.session, SESSION);
  assert.equal(client.me, 1);

  transport.fromHost(ping());
  const pong = transport.sent.find(item => decodeCBN1(item.payload).type === CBN1_TYPES.PONG);
  assert.ok(pong);
  assert.equal(pong.coalesceKey, "cbn1-pong");

  for (let slot = 0; slot < 4; slot++) transport.fromHost(save(slot));
  assert.equal(client.readyMask & 0x0f, 0x0f);
  transport.fromHost(start());
  assert.equal(client.phase, "playing");
  assert.equal(client.delay, 2);
  assert.equal(client.meHave, 2);
  assert.equal(client.inputHistory.length, 2);
  assert.equal(client.readyMask, 0x1f);
  const initialInput = transport.sent.find(item => decodeCBN1(item.payload).type === CBN1_TYPES.INPUT);
  assert.ok(initialInput);
  assert.equal(initialInput.coalesceKey, "cbn1-input");
  assert.equal(initialInput.payload[26], 2);
  client.stop();
});

test("plays authoritative host records two emulator frames at a time and sends delayed browser input", async () => {
  const { transport, client, applied, advanced } = setupClient();
  await client.connect();
  transport.fromHost(welcome());
  for (let slot = 0; slot < 4; slot++) transport.fromHost(save(slot));
  transport.fromHost(start());
  transport.fromHost(record(0));
  await new Promise(resolve => setTimeout(resolve, 0));
  assert.equal(client.next, 1);
  assert.equal(client.meHave, 3);
  assert.equal(advanced.length, 1);
  assert.deepEqual(advanced, [2]);
  assert.equal(applied.length, 4);
  assert.deepEqual(applied.map(item => item.slot), [0, 1, 2, 3]);
  assert.equal(applied[0].buttons, 0x4000);
  const inputPackets = transport.sent.filter(item => decodeCBN1(item.payload).type === CBN1_TYPES.INPUT);
  const lastInput = inputPackets.at(-1);
  assert.equal(lastInput.payload[26], 3);
  client.stop();
});

test("ignores packets from non-host room members", async () => {
  const { transport, client } = setupClient();
  await client.connect();
  transport.emit("packet", { senderId: "ffeeddccbbaa99887766554433221100", payload: welcome() });
  assert.equal(client.phase, "joining");
  client.stop();
});
test("pauses the browser match when the PS3 host leaves the room", async () => {
  const { transport, client } = setupClient();
  await client.connect();
  transport.fromHost(welcome());
  for (let slot = 0; slot < 4; slot++) transport.fromHost(save(slot));
  transport.fromHost(start());
  transport.emit("control", JSON.stringify({ type: "room_update", room: { status: "waiting", host_session_id: HOST } }));
  assert.equal(client.phase, "failed");
  assert.equal(transport.closed, true);
});
test("sends CBN1 BYE when leaving before the host starts the match", async () => {
  const { transport, client } = setupClient();
  await client.connect();
  transport.fromHost(welcome());
  client.stop();
  assert.equal(decodeCBN1(transport.sent.at(-1).payload).type, CBN1_TYPES.BYE);
  assert.equal(transport.closed, true);
});
