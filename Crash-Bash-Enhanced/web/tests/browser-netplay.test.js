import test from "node:test";
import assert from "node:assert/strict";
import { CBN1BrowserClient } from "../public/browser-netplay.js";

const HOST = "00112233445566778899aabbccddeeff";
const SESSION = 0x11223344;
const MAGIC = [0x43, 0x42, 0x4e, 0x31];

class FakeTransport {
  constructor() {
    this.listeners = new Map();
    this.sent = [];
    this.closed = false;
  }
  on(name, fn) {
    if (!this.listeners.has(name)) this.listeners.set(name, new Set());
    this.listeners.get(name).add(fn);
    return () => this.listeners.get(name)?.delete(fn);
  }
  emit(name, value) {
    for (const fn of this.listeners.get(name) || []) fn(value);
  }
  async connect() { return this; }
  sendPacket(payload, recipientId, key) {
    this.sent.push({ payload: payload.slice(), recipientId, key });
    return true;
  }
  close() { this.closed = true; this.emit("close", {}); }
  packet(senderId, payload) { this.emit("packet", { senderId, payload }); }
}

function welcome(nonce, session = SESSION, machine = 1, refused = 0) {
  const packet = new Uint8Array(15);
  packet.set([...MAGIC, 2]);
  const view = new DataView(packet.buffer);
  view.setUint32(5, nonce, true);
  packet[9] = refused;
  packet[10] = machine;
  view.setUint32(11, session, true);
  return packet;
}

function ping(session = SESSION) {
  const packet = new Uint8Array(5 + 13);
  packet.set([...MAGIC, 3]);
  const view = new DataView(packet.buffer);
  view.setUint32(5, session, true);
  view.setUint32(9, 1234, true);
  packet[13] = 0;
  packet[14] = 2;
  packet[15] = 0;
  packet.set([1, 1], 16);
  return packet;
}

function save(session, slot, data = new Uint8Array()) {
  const packet = new Uint8Array(12 + data.length);
  packet.set([...MAGIC, 5]);
  const view = new DataView(packet.buffer);
  view.setUint32(5, session, true);
  packet[9] = slot;
  view.setUint16(10, data.length, true);
  packet.set(data, 12);
  return packet;
}

function start(session = SESSION, delay = 3) {
  const packet = new Uint8Array(5 + 12);
  packet.set([...MAGIC, 6]);
  const view = new DataView(packet.buffer);
  view.setUint32(5, session, true);
  packet[9] = delay;
  packet[10] = 2;
  packet[11] = 1;
  packet[12] = 0;
  packet.set([0, 1, 1, 1], 13);
  return packet;
}

function record(session = SESSION, first = 0, values = []) {
  const packet = new Uint8Array(5 + 28 + values.length * 28);
  packet.set([...MAGIC, 9]);
  const view = new DataView(packet.buffer);
  view.setUint32(5, session, true);
  view.setInt32(9, 0, true); // host has not acknowledged browser inputs yet
  view.setInt32(13, first + values.length, true);
  view.setInt32(17, -1, true); // no cross-emulator RAM fingerprint
  view.setUint32(21, 0, true);
  packet[25] = 0;
  packet[26] = 0;
  packet[27] = 0;
  view.setInt32(28, first, true);
  packet[32] = values.length;
  let offset = 33;
  for (const value of values) {
    packet.set(value, offset);
    offset += 28;
  }
  return packet;
}

test("joins the PS3 host with a CBN1 JOIN and accepts the matching WELCOME", async () => {
  const transport = new FakeTransport();
  const client = new CBN1BrowserClient({
    transport,
    hostSessionId: HOST,
    pads: 1,
    nonce: 0x55667788
  });
  const joined = client.connect();
  await new Promise(resolve => setTimeout(resolve, 0));
  assert.equal(transport.sent[0].recipientId, HOST);
  assert.equal(transport.sent[0].payload[4], 1);
  assert.equal(new DataView(transport.sent[0].payload.buffer).getUint32(16, true), 0x55667788);

  transport.packet(HOST, welcome(0x55667788));
  await joined;
  assert.equal(client.state, "lobby");
  assert.equal(client.session, SESSION);
  assert.equal(client.me, 1);
  client.close();
});

test("answers host PING, acknowledges all four SAVE slots, and accepts START", async () => {
  const transport = new FakeTransport();
  const client = new CBN1BrowserClient({ transport, hostSessionId: HOST, nonce: 77 });
  const joined = client.connect();
  await new Promise(resolve => setTimeout(resolve, 0));
  transport.packet(HOST, welcome(77));
  await joined;

  transport.packet(HOST, ping());
  assert.equal(transport.sent.at(-1).payload[4], 4); // PONG
  for (let slot = 0; slot < 4; slot++) transport.packet(HOST, save(SESSION, slot));
  assert.equal(client.ready & 0x0f, 0x0f);
  assert.equal(transport.sent.at(-1).payload[4], 7); // READY
  assert.equal(transport.sent.at(-1).payload[10], 0x0f);

  transport.packet(HOST, start());
  assert.equal(client.state, "playing");
  assert.equal(client.delay, 3);
  assert.equal(client.firstPad, 1);
  assert.equal(client.localHave, 3);
  assert.equal(transport.sent.at(-1).payload[10], 0x1f);
  client.close();
});

test("sends CBN1 input and releases the synchronized four-pad record to the browser", async () => {
  const transport = new FakeTransport();
  const localPad = new Uint8Array([3, 0, 0x40, 128, 128, 128, 128]);
  const client = new CBN1BrowserClient({
    transport,
    hostSessionId: HOST,
    nonce: 88,
    getLocalPads: () => [localPad]
  });
  const joined = client.connect();
  await new Promise(resolve => setTimeout(resolve, 0));
  transport.packet(HOST, welcome(88));
  await joined;
  for (let slot = 0; slot < 4; slot++) transport.packet(HOST, save(SESSION, slot));
  transport.packet(HOST, start());

  const stepPromise = client.step();
  await new Promise(resolve => setTimeout(resolve, 0));
  const input = transport.sent.findLast(item => item.payload[4] === 8);
  assert.ok(input, "browser should send a CBN1 INPUT packet");
  assert.equal(input.recipientId, HOST);
  assert.equal(input.payload[26], 1);
  assert.equal(input.payload[27], 3);
  assert.equal(input.payload[28], 0);
  assert.equal(input.payload[29], 0x40);

  const authoritative = new Uint8Array(28);
  authoritative[0] = 3;
  authoritative[1] = 0x00;
  authoritative[2] = 0x40;
  authoritative.fill(128, 3);
  transport.packet(HOST, record(SESSION, 0, [authoritative]));
  const result = await stepPromise;
  assert.equal(result.step, 0);
  assert.equal(result.record.length, 28);
  assert.deepEqual(result.record, authoritative);
  client.close();
});

test("rejects a PS3 host that reports a different game build", async () => {
  const transport = new FakeTransport();
  const client = new CBN1BrowserClient({ transport, hostSessionId: HOST, nonce: 99 });
  const joined = client.connect();
  await new Promise(resolve => setTimeout(resolve, 0));
  transport.packet(HOST, welcome(99, SESSION, 1, 4));
  await assert.rejects(joined, /builds do not match/);
  assert.equal(client.state, "failed");
});
