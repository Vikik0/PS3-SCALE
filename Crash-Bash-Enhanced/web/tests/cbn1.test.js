import test from "node:test";
import assert from "node:assert/strict";
import { CBN1_BUILD_HASH, CBN1_DATA_HASH, decodeCBN1, decodePing, decodeRecord, decodeStart, decodeWelcome, encodeBye, encodeInput, encodeJoin, encodePong, encodeReady } from "../public/cbn1.js";

test("encodes the pinned PS3-compatible CBN1 JOIN header", () => {
  const packet = encodeJoin(0x11223344, 1);
  assert.equal(Buffer.from(packet).toString("hex"), "43424e31010200f8bd6dc3000000000144332211");
  assert.equal(CBN1_BUILD_HASH, 0xc36dbdf8);
  assert.equal(CBN1_DATA_HASH, 0);
});

test("decodes WELCOME and rejects a mismatched nonce", () => {
  const packet = new Uint8Array(15);
  packet.set([0x43, 0x42, 0x4e, 0x31, 2]);
  const v = new DataView(packet.buffer);
  v.setUint32(5, 0x11223344, true); packet[9] = 0; packet[10] = 1; v.setUint32(11, 0xaabbccdd, true);
  assert.deepEqual(decodeWelcome(packet, 0x11223344), { nonce: 0x11223344, refused: 0, machine: 1, session: 0xaabbccdd });
  assert.throws(() => decodeWelcome(packet, 1), /nonce/);
});

test("encodes PONG, READY, and BYE with little-endian fields", () => {
  assert.equal(Buffer.from(encodePong(0x11223344, 1, 0x55667788)).toString("hex"), "43424e3104443322110188776655");
  assert.equal(Buffer.from(encodeReady(0x11223344, 1, 0x1f)).toString("hex"), "43424e310744332211011f");
  assert.equal(Buffer.from(encodeBye(0x11223344, 1, 2, 0x55667788)).toString("hex"), "43424e310a44332211010288776655");
});

test("decodes host PING roster and START controller assignments", () => {
  const ping = new Uint8Array(5 + 13); ping.set([0x43,0x42,0x4e,0x31,3]);
  const p = new DataView(ping.buffer); p.setUint32(5, 0x11223344, true); p.setUint32(9, 0x55667788, true); ping[13]=1; ping[14]=2; ping[15]=1; ping[16]=1; ping[17]=1;
  assert.deepEqual(decodePing(ping), { session: 0x11223344, sent: 0x55667788, timed: true, machines: 2, me: 1, pads: new Uint8Array([1,1]) });
  const start = new Uint8Array(5 + 12); start.set([0x43,0x42,0x4e,0x31,6]);
  const s = new DataView(start.buffer); s.setUint32(5, 0xaabbccdd, true); start[9]=12; start[10]=2; start[11]=1; start[12]=0; start.set([0,1,1,1],13);
  assert.deepEqual(decodeStart(start), { session: 0xaabbccdd, delay: 12, machines: 2, me: 1, flags: 0, assignments: [{first:0,pads:1},{first:1,pads:1}] });
});

test("encodes browser pad input and decodes authoritative four-pad records", () => {
  const local = new Uint8Array([1, 0x00, 0x40, 0x80, 0x80, 0x80, 0x80]);
  const input = encodeInput({ session: 0x11223344, me: 1, hostHave: 4, hostAcked: 2, records: [local] });
  assert.equal(input[4], 8); assert.equal(input[26], 1); assert.equal(input.byteLength, 34);
  const packet = new Uint8Array(5 + 28 + 28); packet.set([0x43,0x42,0x4e,0x31,9]);
  const v = new DataView(packet.buffer); v.setUint32(5,0x11223344,true); v.setInt32(9,2,true); v.setInt32(13,5,true); v.setInt32(17,-1,true); v.setUint32(21,0,true); packet[25]=0; packet[26]=0; packet[27]=0; v.setInt32(28,2,true); packet[32]=1; packet.set(new Uint8Array(28).fill(7),33);
  const record = decodeRecord(packet);
  assert.equal(record.session,0x11223344); assert.equal(record.acked,2); assert.equal(record.hostNext,5); assert.equal(record.first,2); assert.equal(record.records.length,1); assert.equal(record.records[0][0],7);
});

test("rejects malformed CBN1 frames", () => {
  assert.throws(() => decodeCBN1(new Uint8Array([1,2,3,4,5])), /header/);
  const bad = new Uint8Array([0x43,0x42,0x4e,0x31,9]);
  assert.throws(() => decodeRecord(bad), /RECORD/);
});