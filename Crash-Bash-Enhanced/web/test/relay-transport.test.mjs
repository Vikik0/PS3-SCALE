import test from "node:test";
import assert from "node:assert/strict";
import {
  CrashBashRelayTransport,
  decodeRelayEnvelope,
  encodeRelayBatch,
  RELAY_LIMITS
} from "../public/relay-transport.js";

const sessionA = "00112233445566778899aabbccddeeff";
const sessionB = "ffeeddccbbaa99887766554433221100";

test("encodes targeted and broadcast packets into CBU1", () => {
  const bytes = encodeRelayBatch([
    { recipientId: sessionA, payload: new Uint8Array([1, 2, 3]) },
    { payload: new Uint8Array([4, 5]) }
  ]);
  assert.deepEqual(Array.from(bytes.subarray(0, 5)), [0x43, 0x42, 0x55, 0x31, 2]);
  assert.equal(bytes.length, 5 + 18 + 3 + 18 + 2);
  assert.equal(Buffer.from(bytes.subarray(5, 21)).toString("hex"), sessionA);
  assert.equal((bytes[21] << 8) | bytes[22], 3);
  assert.deepEqual(Array.from(bytes.subarray(23, 26)), [1, 2, 3]);
  assert.deepEqual(Array.from(bytes.subarray(26, 42)), new Array(16).fill(0));
});

test("decodes authenticated CBR1 sender envelopes", () => {
  const payload = new Uint8Array([0x43, 0x42, 0x4e, 0x31, 7]);
  const envelope = new Uint8Array(22 + payload.length);
  envelope.set([0x43, 0x42, 0x52, 0x31], 0);
  envelope.set(Buffer.from(sessionB, "hex"), 4);
  envelope[20] = 0;
  envelope[21] = payload.length;
  envelope.set(payload, 22);
  const decoded = decodeRelayEnvelope(envelope);
  assert.equal(decoded.senderId, sessionB);
  assert.deepEqual(decoded.payload, payload);
});

test("rejects invalid CBU1 and CBR1 frames", () => {
  assert.throws(() => encodeRelayBatch([]), /between 1 and 32/);
  assert.throws(() => encodeRelayBatch([{ payload: new Uint8Array(1201) }]), /1 to 1200/);
  assert.throws(() => encodeRelayBatch([{ recipientId: "bad", payload: new Uint8Array([1]) }]), /session ID/);
  assert.throws(() => decodeRelayEnvelope(new Uint8Array([1, 2, 3])), /CBR1/);
});

class FakeWebSocket {
  static instances = [];
  constructor(url, protocols) {
    this.url = url;
    this.protocols = protocols;
    this.readyState = 0;
    this.sent = [];
    this.failSend = false;
    FakeWebSocket.instances.push(this);
  }
  open() {
    this.readyState = 1;
    this.onopen?.();
  }
  send(data) {
    if (this.failSend) throw new Error("simulated send failure");
    this.sent.push(data);
  }
  close(code = 1000, reason = "") {
    this.readyState = 3;
    this.onclose?.({ code, reason });
  }
  message(data) {
    this.onmessage?.({ data });
  }
}

test("connects with authenticated room subprotocols and receives CBR1 packets", async () => {
  FakeWebSocket.instances = [];
  const transport = new CrashBashRelayTransport({
    apiOrigin: "https://crash-bash.test",
    roomCode: "abc123",
    token: "a".repeat(32),
    WebSocketImpl: FakeWebSocket
  });
  let received = null;
  transport.on("packet", packet => { received = packet; });
  const connected = transport.connect();
  const socket = FakeWebSocket.instances[0];
  assert.equal(socket.url, "wss://crash-bash.test/v1/rooms/ABC123/transport");
  assert.deepEqual(socket.protocols, ["cbash-v1", `cbash-auth.${"a".repeat(32)}`]);
  socket.open();
  await connected;

  const payload = new Uint8Array([9, 8, 7]);
  const envelope = new Uint8Array(22 + payload.length);
  envelope.set([0x43, 0x42, 0x52, 0x31], 0);
  envelope.set(Buffer.from(sessionB, "hex"), 4);
  envelope[21] = payload.length;
  envelope.set(payload, 22);
  socket.message(envelope.buffer);
  assert.equal(received.senderId, sessionB);
  assert.deepEqual(received.payload, payload);
  transport.close();
});

test("batches at the rate cap, coalesces retransmissions, and retains failed sends", async () => {
  FakeWebSocket.instances = [];
  let clock = 1000;
  const transport = new CrashBashRelayTransport({
    apiOrigin: "https://crash-bash.test",
    roomCode: "ABC123",
    token: "b".repeat(32),
    WebSocketImpl: FakeWebSocket,
    now: () => clock
  });
  const connected = transport.connect();
  const socket = FakeWebSocket.instances[0];
  socket.open();
  await connected;

  assert.equal(transport.sendPacket(new Uint8Array([1]), sessionA, "input"), true);
  assert.equal(transport.sendPacket(new Uint8Array([2]), sessionA, "input"), true);
  assert.equal(transport.queue.length, 1);
  assert.equal(transport.flush(), true);
  assert.equal(socket.sent.length, 1);
  assert.equal(new Uint8Array(socket.sent[0])[4], 1);
  assert.equal(new Uint8Array(socket.sent[0])[23], 2);

  transport.sendPacket(new Uint8Array([3]), sessionA);
  clock += 100;
  assert.equal(transport.flush(), false);
  assert.equal(transport.queue.length, 1);

  socket.failSend = true;
  clock += 120;
  assert.equal(transport.flush(), false);
  assert.equal(transport.queue.length, 1, "failed send must keep the queued packet");
  socket.failSend = false;
  assert.equal(transport.flush(), true, "the queued packet should retry once the socket recovers");
  assert.equal(transport.queue.length, 0);
  transport.close();
});
