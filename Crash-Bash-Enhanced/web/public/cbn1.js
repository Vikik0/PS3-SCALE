// CBN1 packet codec shared with the pinned Crash Bash netcode (protocol 2).
// The submodule is pinned at 1.4.0; PS3's platform_data_path() is NULL, so data_hash() is 0.
export const CBN1_PROTOCOL = 2;
export const CBN1_BUILD_HASH = 0xc36dbdf8;
export const CBN1_DATA_HASH = 0;
export const CBN1_PAD_BYTES = 7;
export const CBN1_RECORD_BYTES = 28;
export const CBN1_MAX_RECORDS = 32;

const MAGIC = [0x43, 0x42, 0x4e, 0x31];
const TYPE = Object.freeze({ JOIN: 1, WELCOME: 2, PING: 3, PONG: 4, SAVE: 5, START: 6, READY: 7, INPUT: 8, RECORD: 9, BYE: 10 });

function view(bytes) {
  if (bytes instanceof Uint8Array) return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (bytes instanceof ArrayBuffer) return new DataView(bytes);
  if (ArrayBuffer.isView(bytes)) return new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  throw new TypeError("Expected an ArrayBuffer or byte view.");
}

function begin(type, bodyLength) {
  const bytes = new Uint8Array(5 + bodyLength);
  bytes.set(MAGIC, 0);
  bytes[4] = type;
  return bytes;
}

function writeU32(bytes, offset, value) {
  view(bytes).setUint32(offset, Number(value) >>> 0, true);
}

function writeI32(bytes, offset, value) {
  view(bytes).setInt32(offset, Number(value) | 0, true);
}

function readU32(data, offset) { return view(data).getUint32(offset, true); }
function readI32(data, offset) { return view(data).getInt32(offset, true); }

export function decodeCBN1(bytes) {
  const data = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes.buffer || bytes, bytes.byteOffset || 0, bytes.byteLength);
  if (data.byteLength < 5 || !MAGIC.every((value, index) => data[index] === value)) {
    throw new Error("Invalid CBN1 packet header.");
  }
  return { type: data[4], body: data.subarray(5) };
}

export function encodeJoin(nonce, pads = 1) {
  if (!Number.isInteger(pads) || pads < 1 || pads > 4) throw new RangeError("pads must be between 1 and 4");
  const packet = begin(TYPE.JOIN, 15);
  const data = view(packet);
  data.setUint16(5, CBN1_PROTOCOL, true);
  data.setUint32(7, CBN1_BUILD_HASH, true);
  data.setUint32(11, CBN1_DATA_HASH, true);
  packet[15] = pads;
  writeU32(packet, 16, nonce);
  return packet;
}

export function decodeWelcome(packet, expectedNonce) {
  const { type, body } = decodeCBN1(packet);
  if (type !== TYPE.WELCOME || body.byteLength !== 10) throw new Error("Invalid CBN1 WELCOME packet.");
  const nonce = readU32(body, 0);
  if (expectedNonce !== undefined && nonce !== (Number(expectedNonce) >>> 0)) throw new Error("WELCOME nonce does not match this browser session.");
  return { nonce, refused: body[4], machine: body[5], session: readU32(body, 6) };
}

export function decodePing(packet) {
  const { type, body } = decodeCBN1(packet);
  if (type !== TYPE.PING || body.byteLength < 11) throw new Error("Invalid CBN1 PING packet.");
  const machines = body[9];
  if (machines < 2 || machines > 4 || body.byteLength !== 11 + machines) throw new Error("Invalid CBN1 PING roster.");
  return { session: readU32(body, 0), sent: readU32(body, 4), timed: body[8] !== 0, machines, me: body[10], pads: body.slice(11) };
}

export function encodePong(session, me, sent) {
  const packet = begin(TYPE.PONG, 9);
  writeU32(packet, 5, session);
  packet[9] = me & 0xff;
  writeU32(packet, 10, sent);
  return packet;
}

export function decodeSave(packet) {
  const { type, body } = decodeCBN1(packet);
  if (type !== TYPE.SAVE || body.byteLength < 7) throw new Error("Invalid CBN1 SAVE packet.");
  const size = view(body).getUint16(5, true);
  if (body.byteLength !== 7 + size) throw new Error("Invalid CBN1 SAVE payload length.");
  return { session: readU32(body, 0), slot: body[4], data: body.slice(7) };
}

export function decodeStart(packet) {
  const { type, body } = decodeCBN1(packet);
  if (type !== TYPE.START || body.byteLength < 8) throw new Error("Invalid CBN1 START packet.");
  const machines = body[5];
  if (machines < 2 || machines > 4 || body.byteLength !== 8 + machines * 2) throw new Error("Invalid CBN1 START roster.");
  const assignments = [];
  for (let i = 0; i < machines; i++) assignments.push({ first: body[8 + i * 2], pads: body[9 + i * 2] });
  return { session: readU32(body, 0), delay: body[4], machines, me: body[6], flags: body[7], assignments };
}

export function encodeReady(session, me, ready) {
  const packet = begin(TYPE.READY, 6);
  writeU32(packet, 5, session);
  packet[9] = me & 0xff;
  packet[10] = ready & 0xff;
  return packet;
}

export function encodeInput({ session, me, hostHave, hostAcked, records, printStep = -1, printValue = 0 }) {
  if (!Array.isArray(records) || records.length > CBN1_MAX_RECORDS) throw new RangeError("CBN1 INPUT accepts at most 32 records.");
  const recordBytes = records.reduce((total, record) => {
    if (!(record instanceof Uint8Array) || record.byteLength < CBN1_PAD_BYTES || record.byteLength > CBN1_PAD_BYTES * 4 || record.byteLength % CBN1_PAD_BYTES !== 0) {
      throw new TypeError("Each input record must contain one to four 7-byte pad states.");
    }
    return total + record.byteLength;
  }, 0);
  const packet = begin(TYPE.INPUT, 22 + recordBytes);
  writeU32(packet, 5, session);
  packet[9] = me & 0xff;
  writeU32(packet, 10, hostHave);
  writeI32(packet, 14, printStep);
  writeU32(packet, 18, printValue);
  writeU32(packet, 22, hostAcked);
  packet[26] = records.length;
  let offset = 27;
  for (const record of records) { packet.set(record, offset); offset += record.byteLength; }
  return packet;
}

export function decodeRecord(packet) {
  const { type, body } = decodeCBN1(packet);
  if (type !== TYPE.RECORD || body.byteLength < 28) throw new Error("Invalid CBN1 RECORD packet.");
  const count = body[27];
  if (count > CBN1_MAX_RECORDS || body.byteLength !== 28 + count * CBN1_RECORD_BYTES) throw new Error("Invalid CBN1 RECORD payload length.");
  const records = [];
  for (let i = 0; i < count; i++) {
    const start = 28 + i * CBN1_RECORD_BYTES;
    records.push(body.slice(start, start + CBN1_RECORD_BYTES));
  }
  return {
    session: readU32(body, 0), acked: readI32(body, 4), hostNext: readI32(body, 8),
    printStep: readI32(body, 12), printValue: readU32(body, 16),
    quiet: body[20] !== 0, gone: body[21], slow: body[22], first: readI32(body, 23), records
  };
}

export function encodeBye(session, me, reason, step = 0) {
  const packet = begin(TYPE.BYE, 10);
  writeU32(packet, 5, session);
  packet[9] = me & 0xff;
  packet[10] = reason & 0xff;
  writeI32(packet, 11, step);
  return packet;
}

export const CBN1_TYPES = TYPE;