const RELAY_PROTOCOL = "cbash-v1";
const MAX_PACKET_BYTES = 1200;
const MAX_BATCH_PACKETS = 32;
const MAX_MESSAGE_BYTES = 28672;
const FLUSH_INTERVAL_MS = 220;
const HEARTBEAT_INTERVAL_MS = 20000;

function bytesFromHex(hex) {
  if (typeof hex !== "string" || !/^[0-9a-f]{32}$/i.test(hex)) {
    throw new TypeError("A 32-character session ID is required.");
  }
  return Uint8Array.from({ length: 16 }, (_, index) =>
    Number.parseInt(hex.slice(index * 2, index * 2 + 2), 16)
  );
}

function hexFromBytes(bytes) {
  return Array.from(bytes, byte => byte.toString(16).padStart(2, "0")).join("");
}

export function encodeRelayBatch(entries) {
  if (!Array.isArray(entries) || entries.length < 1 || entries.length > MAX_BATCH_PACKETS) {
    throw new RangeError("A relay batch must contain between 1 and 32 packets.");
  }

  let total = 5;
  const normalized = entries.map(entry => {
    const recipient = entry.recipientId ? bytesFromHex(entry.recipientId) : new Uint8Array(16);
    const payload = entry.payload instanceof Uint8Array
      ? entry.payload
      : new Uint8Array(entry.payload || []);
    if (payload.length < 1 || payload.length > MAX_PACKET_BYTES) {
      throw new RangeError("Each relay datagram must contain 1 to 1200 bytes.");
    }
    total += 18 + payload.length;
    return { recipient, payload };
  });

  if (total > MAX_MESSAGE_BYTES) {
    throw new RangeError("The encoded relay batch exceeds the 28,672-byte message cap.");
  }

  const output = new Uint8Array(total);
  output.set([0x43, 0x42, 0x55, 0x31, normalized.length], 0); // CBU1
  let offset = 5;
  for (const entry of normalized) {
    output.set(entry.recipient, offset);
    offset += 16;
    output[offset++] = (entry.payload.length >>> 8) & 0xff;
    output[offset++] = entry.payload.length & 0xff;
    output.set(entry.payload, offset);
    offset += entry.payload.length;
  }
  return output;
}

export function decodeRelayEnvelope(value) {
  const bytes = value instanceof Uint8Array ? value : new Uint8Array(value);
  if (bytes.length < 23 ||
      bytes[0] !== 0x43 || bytes[1] !== 0x42 ||
      bytes[2] !== 0x52 || bytes[3] !== 0x31) {
    throw new TypeError("Invalid CBR1 relay envelope.");
  }
  const length = (bytes[20] << 8) | bytes[21];
  if (length < 1 || length > MAX_PACKET_BYTES || bytes.length !== 22 + length) {
    throw new RangeError("Invalid CBR1 payload length.");
  }
  return {
    senderId: hexFromBytes(bytes.subarray(4, 20)),
    payload: bytes.slice(22)
  };
}

export class CrashBashRelayTransport {
  constructor({ apiOrigin = window.location.origin, roomCode, token, WebSocketImpl = WebSocket, now = () => Date.now() }) {
    if (!/^[A-Z0-9]{6}$/.test(String(roomCode || "").toUpperCase())) {
      throw new TypeError("A six-character room code is required.");
    }
    if (!token || typeof token !== "string") throw new TypeError("A guest session token is required.");
    this.apiOrigin = apiOrigin;
    this.roomCode = String(roomCode).toUpperCase();
    this.token = token;
    this.WebSocketImpl = WebSocketImpl;
    this.now = now;
    this.socket = null;
    this.queue = [];
    this.lastFlushAt = Number.NEGATIVE_INFINITY;
    this.flushTimer = null;
    this.heartbeatTimer = null;
    this.listeners = new Map();
  }

  on(event, handler) {
    if (!this.listeners.has(event)) this.listeners.set(event, new Set());
    this.listeners.get(event).add(handler);
    return () => this.listeners.get(event)?.delete(handler);
  }

  emit(event, value) {
    for (const handler of this.listeners.get(event) || []) {
      try { handler(value); } catch (error) { queueMicrotask(() => { throw error; }); }
    }
  }

  connect() {
    if (this.socket) throw new Error("Relay transport is already connecting or connected.");
    const url = new URL(`/v1/rooms/${this.roomCode}/transport`, this.apiOrigin);
    url.protocol = url.protocol === "https:" ? "wss:" : "ws:";
    const socket = new this.WebSocketImpl(url.toString(), [
      RELAY_PROTOCOL,
      `cbash-auth.${this.token}`
    ]);
    socket.binaryType = "arraybuffer";
    this.socket = socket;

    return new Promise((resolve, reject) => {
      let settled = false;
      socket.onopen = () => {
        settled = true;
        this.flushTimer = setInterval(() => this.flush(), 30);
        this.heartbeatTimer = setInterval(() => {
          if (socket.readyState === 1) socket.send("PING");
        }, HEARTBEAT_INTERVAL_MS);
        this.emit("open");
        resolve(this);
      };
      socket.onmessage = event => this.handleMessage(event.data);
      socket.onerror = event => {
        this.emit("error", event);
        if (!settled) reject(new Error("Could not connect to the Crash Bash relay."));
      };
      socket.onclose = event => {
        this.stopTimers();
        this.socket = null;
        this.emit("close", event);
        if (!settled) reject(new Error("The Crash Bash relay closed before connecting."));
      };
    });
  }

  handleMessage(data) {
    if (typeof data === "string") {
      if (data !== "PONG") this.emit("control", data);
      return;
    }
    if (data instanceof Blob) {
      data.arrayBuffer().then(buffer => this.handleBinary(buffer)).catch(error => this.emit("error", error));
      return;
    }
    this.handleBinary(data);
  }

  handleBinary(data) {
    try {
      this.emit("packet", decodeRelayEnvelope(data));
    } catch (error) {
      this.emit("error", error);
    }
  }

  sendPacket(payload, recipientId = null, coalesceKey = "") {
    const bytes = payload instanceof Uint8Array ? payload.slice() : new Uint8Array(payload || []);
    if (bytes.length < 1 || bytes.length > MAX_PACKET_BYTES) return false;
    const recipientKey = recipientId ? String(recipientId).toUpperCase() : "";
    try {
      if (recipientKey) bytesFromHex(recipientKey);
    } catch {
      return false;
    }

    if (coalesceKey) {
      const existing = this.queue.findIndex(item =>
        item.coalesceKey === coalesceKey &&
        item.recipientId === recipientKey
      );
      if (existing >= 0) {
        this.queue[existing] = { recipientId: recipientKey, payload: bytes, coalesceKey };
        return true;
      }
    }

    if (this.queue.length >= MAX_BATCH_PACKETS) {
      this.flush();
      if (this.queue.length >= MAX_BATCH_PACKETS) return false;
    }
    this.queue.push({ recipientId: recipientKey, payload: bytes, coalesceKey });
    return true;
  }

  flush() {
    if (!this.socket || this.socket.readyState !== 1 || this.queue.length === 0) return false;
    if (this.now() - this.lastFlushAt < FLUSH_INTERVAL_MS) return false;

    const batch = [];
    let size = 5;
    let takeCount = 0;
    while (takeCount < this.queue.length && batch.length < MAX_BATCH_PACKETS) {
      const next = this.queue[takeCount];
      const nextSize = 18 + next.payload.length;
      if (size + nextSize > MAX_MESSAGE_BYTES) break;
      batch.push({
        recipientId: next.recipientId || null,
        payload: next.payload
      });
      takeCount += 1;
      size += nextSize;
    }
    if (!batch.length) return false;

    try {
      this.socket.send(encodeRelayBatch(batch));
      this.queue.splice(0, takeCount);
      this.lastFlushAt = this.now();
      this.emit("flush", { packetCount: batch.length, byteLength: size });
      return true;
    } catch (error) {
      this.emit("error", error);
      return false;
    }
  }

  stopTimers() {
    if (this.flushTimer) clearInterval(this.flushTimer);
    if (this.heartbeatTimer) clearInterval(this.heartbeatTimer);
    this.flushTimer = null;
    this.heartbeatTimer = null;
  }

  close(code = 1000, reason = "Client closed") {
    this.stopTimers();
    if (this.socket && this.socket.readyState < 2) this.socket.close(code, reason);
    this.socket = null;
  }
}

export const RELAY_LIMITS = Object.freeze({
  maxPacketBytes: MAX_PACKET_BYTES,
  maxBatchPackets: MAX_BATCH_PACKETS,
  maxMessageBytes: MAX_MESSAGE_BYTES,
  flushIntervalMs: FLUSH_INTERVAL_MS,
  heartbeatIntervalMs: HEARTBEAT_INTERVAL_MS
});
