import crypto from "node:crypto";
import fs from "node:fs";
import http from "node:http";
import https from "node:https";

const GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const server = http.createServer();
const secureServer = https.createServer({
  key: fs.readFileSync(process.env.CB_TEST_TLS_KEY),
  cert: fs.readFileSync(process.env.CB_TEST_TLS_CERT)
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

function handleUpgrade(request, socket) {
  const key = request.headers["sec-websocket-key"];
  const protocols = String(request.headers["sec-websocket-protocol"] || "")
    .split(",").map(value => value.trim());
  if (!key || !protocols.includes("cbash-v1")) {
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
        if (longLength > 2048n) {
          socket.destroy();
          return;
        }
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
        sendFrame(socket, 2, payload);
      } else if (opcode === 1 && payload.toString() === "PING") {
        sendFrame(socket, 1, Buffer.from("PONG"));
      } else if (opcode === 8) {
        sendFrame(socket, 8, payload);
        socket.end();
        return;
      }
    }
  });
}

server.on("upgrade", handleUpgrade);
secureServer.on("upgrade", handleUpgrade);
let readyCount = 0;
function markReady() {
  readyCount++;
  if (readyCount === 2) process.stdout.write("ready\n");
}
server.listen(8765, "127.0.0.1", markReady);
secureServer.listen(8767, "127.0.0.1", markReady);
