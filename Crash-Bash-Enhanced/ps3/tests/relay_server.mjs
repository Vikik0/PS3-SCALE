import crypto from "node:crypto";
import http from "node:http";

const TICKET = "a".repeat(64);
const SESSION_ID = "00112233445566778899aabbccddeeff";
const GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const server = http.createServer((request, response) => {
  if (request.method !== "POST" || request.url !== "/v1/rooms/ABC123/join-guest") {
    response.writeHead(404, { "Content-Type": "application/json" });
    response.end(JSON.stringify({ error: "NOT_FOUND" }));
    return;
  }

  let body = "";
  request.setEncoding("utf8");
  request.on("data", chunk => { body += chunk; });
  request.on("end", () => {
    const parsed = JSON.parse(body);
    if (parsed.display_name !== "PS3 Player") {
      response.writeHead(400, { "Content-Type": "application/json" });
      response.end(JSON.stringify({ error: "BAD_NAME" }));
      return;
    }
    const payload = JSON.stringify({ ticket: TICKET, session_id: SESSION_ID, host_session_id: "ffeeddccbbaa99887766554433221100", psn_required: false });
    response.writeHead(201, {
      "Content-Type": "application/json",
      "Content-Length": Buffer.byteLength(payload),
      "Connection": "close"
    });
    response.end(payload);
  });
});

function sendFrame(socket, opcode, payload) {
  const data = Buffer.from(payload);
  let header;
  if (data.length <= 125) {
    header = Buffer.from([0x80 | opcode, data.length]);
  } else {
    header = Buffer.alloc(4);
    header[0] = 0x80 | opcode;
    header[1] = 126;
    header.writeUInt16BE(data.length, 2);
  }
  socket.write(Buffer.concat([header, data]));
}

server.on("upgrade", (request, socket) => {
  const key = request.headers["sec-websocket-key"];
  const protocols = String(request.headers["sec-websocket-protocol"] || "")
    .split(",").map(value => value.trim());
  if (request.url !== "/v1/rooms/ABC123/transport" ||
      !protocols.includes("cbash-v1") ||
      !protocols.includes("cbash-ticket." + TICKET)) {
    socket.destroy();
    return;
  }

  const accept = crypto.createHash("sha1").update(key + GUID).digest("base64");
  socket.write(
    "HTTP/1.1 101 Switching Protocols\r\n" +
    "Upgrade: websocket\r\n" +
    "Connection: Upgrade\r\n" +
    "Sec-WebSocket-Accept: " + accept + "\r\n" +
    "Sec-WebSocket-Protocol: cbash-v1\r\n\r\n"
  );

  let buffered = Buffer.alloc(0);
  socket.on("data", chunk => {
    buffered = Buffer.concat([buffered, chunk]);
    while (buffered.length >= 2) {
      const first = buffered[0];
      const second = buffered[1];
      const opcode = first & 0x0f;
      const masked = (second & 0x80) !== 0;
      let length = second & 0x7f;
      let offset = 2;

      if (length === 126) {
        if (buffered.length < 4) return;
        length = buffered.readUInt16BE(2);
        offset = 4;
      } else if (length === 127) {
        if (buffered.length < 10) return;
        const longLength = buffered.readBigUInt64BE(2);
        if (longLength > 2048n) return socket.destroy();
        length = Number(longLength);
        offset = 10;
      }

      let mask;
      if (masked) {
        if (buffered.length < offset + 4) return;
        mask = buffered.subarray(offset, offset + 4);
        offset += 4;
      }
      if (buffered.length < offset + length) return;

      const payload = Buffer.from(buffered.subarray(offset, offset + length));
      buffered = buffered.subarray(offset + length);
      if (masked) {
        for (let i = 0; i < payload.length; i++) payload[i] ^= mask[i & 3];
      }

      if (opcode === 2) {
        if (payload.length < 5 || payload.toString("ascii", 0, 4) !== "CBU1") {
          socket.destroy();
          return;
        }
        const count = payload[4];
        let packetOffset = 5;
        for (let packetIndex = 0; packetIndex < count; packetIndex++) {
          if (packetOffset + 18 > payload.length) return socket.destroy();
          const recipient = payload.subarray(packetOffset, packetOffset + 16);
          packetOffset += 16;
          const packetLength = payload.readUInt16BE(packetOffset);
          packetOffset += 2;
          if (packetLength < 1 || packetLength > 1200 || packetOffset + packetLength > payload.length) {
            return socket.destroy();
          }
          const packet = payload.subarray(packetOffset, packetOffset + packetLength);
          packetOffset += packetLength;
          const envelope = Buffer.alloc(22 + packet.length);
          envelope.write("CBR1", 0, "ascii");
          Buffer.from(SESSION_ID, "hex").copy(envelope, 4);
          envelope.writeUInt16BE(packet.length, 20);
          packet.copy(envelope, 22);
          sendFrame(socket, 2, envelope);
          void recipient;
        }
        if (packetOffset !== payload.length) socket.destroy();
      } else if (opcode === 1 && payload.toString() === "PING") {
        sendFrame(socket, 1, Buffer.from("PONG"));
      } else if (opcode === 8) {
        sendFrame(socket, 8, payload);
        socket.end();
        return;
      }
    }
  });
});

server.listen(8766, "127.0.0.1", () => process.stdout.write("ready\n"));
