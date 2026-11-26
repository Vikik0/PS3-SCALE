"use strict";

const { isIP } = require("node:net");

const RETROARCH_LOBBY_URL = "https://lobby.libretro.com/list/";
const MAX_LOBBY_BYTES = 4 * 1024 * 1024;
const MAX_LOBBY_ROOMS = 1000;

function cleanText(value, limit = 160) {
  return String(value ?? "").replace(/[\u0000-\u001f\u007f]/g, "").trim().slice(0, limit);
}
function isTruthy(value) {
  return value === true || value === 1 || value === "1" || String(value).toLowerCase() === "true";
}
function isExplicitFalse(value) {
  return value === false || value === 0 || value === "0" || String(value).toLowerCase() === "false";
}
function safeHost(value) {
  const host = cleanText(value, 253);
  if (!host) return false;
  if (isIP(host)) return true;
  if (host.length > 253 || host.startsWith(".") || host.startsWith("-")) return false;
  return host.split(".").every((label) => /^[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$/.test(label));
}
function safeSession(value) {
  const session = cleanText(value, 64);
  return session.length >= 8 && /^[A-Za-z0-9+/=_-]+$/.test(session);
}
function validPort(value) {
  const port = Number(value);
  return Number.isInteger(port) && port > 0 && port <= 65535 ? port : 0;
}
function unwrapEntry(item) {
  if (!item || typeof item !== "object" || Array.isArray(item)) return null;
  if (item.fields && typeof item.fields === "object" && !Array.isArray(item.fields)) return item.fields;
  return item;
}
function parseRetroArchLobby(payload) {
  const data = typeof payload === "string" || Buffer.isBuffer(payload) ? JSON.parse(payload.toString()) : payload;
  if (!Array.isArray(data)) throw new Error("RetroArch lobby returned an invalid room list.");
  return data.slice(0, MAX_LOBBY_ROOMS).map(unwrapEntry).filter(Boolean);
}
function endpointFromEntry(entry) {
  if (isTruthy(entry.has_password)) return { ready: false, state: "host_password_required" };
  const gameName = cleanText(entry.game_name, 128);
  const coreName = cleanText(entry.core_name, 64);
  const coreVersion = cleanText(entry.core_version, 32);
  const relayAddress = cleanText(entry.mitm_ip, 253);
  const relayPort = validPort(entry.mitm_port);
  const relaySession = cleanText(entry.mitm_session, 64);
  if (safeHost(relayAddress) && relayPort && safeSession(relaySession)) {
    return { ready: true, state: "ready", address: relayAddress, port: relayPort, mode: "relay", mitmSession: relaySession, gameName, coreName, coreVersion };
  }
  if (isExplicitFalse(entry.connectable)) return { ready: false, state: "host_not_connectable" };
  const address = cleanText(entry.ip, 253);
  const port = validPort(entry.port);
  if (!safeHost(address) || !port) return { ready: false, state: "host_not_connectable" };
  return { ready: true, state: "ready", address, port, mode: "direct", mitmSession: "", gameName, coreName, coreVersion };
}
function findRetroArchHost(payload, expectedUsername) {
  const expected = cleanText(expectedUsername, 80).toUpperCase();
  if (!expected) return null;
  const entries = Array.isArray(payload) ? payload.map(unwrapEntry).filter(Boolean) : parseRetroArchLobby(payload);
  const entry = entries.find((item) => cleanText(item.username, 80).toUpperCase() === expected);
  return entry ? endpointFromEntry(entry) : null;
}
async function readBoundedBody(response) {
  const declared = Number(response.headers?.get?.("content-length"));
  if (Number.isFinite(declared) && declared > MAX_LOBBY_BYTES) throw new Error("RetroArch lobby response exceeded the size limit.");
  if (response.body && typeof response.body.getReader === "function") {
    const reader = response.body.getReader();
    const chunks = [];
    let total = 0;
    try {
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        total += value.byteLength;
        if (total > MAX_LOBBY_BYTES) {
          await reader.cancel();
          throw new Error("RetroArch lobby response exceeded the size limit.");
        }
        chunks.push(Buffer.from(value));
      }
    } finally {
      try { reader.releaseLock(); } catch {}
    }
    return Buffer.concat(chunks, total).toString("utf8");
  }
  const body = await response.text();
  if (Buffer.byteLength(body, "utf8") > MAX_LOBBY_BYTES) throw new Error("RetroArch lobby response exceeded the size limit.");
  return body;
}
async function fetchRetroArchLobby({ fetchImpl = globalThis.fetch, timeoutMs = 5000 } = {}) {
  if (typeof fetchImpl !== "function") throw new Error("HTTP fetch is unavailable.");
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const response = await fetchImpl(RETROARCH_LOBBY_URL, {
      method: "GET",
      headers: { Accept: "application/json" },
      cache: "no-store",
      signal: controller.signal
    });
    if (!response || !response.ok) throw new Error("RetroArch public lobby is temporarily unavailable.");
    return parseRetroArchLobby(await readBoundedBody(response));
  } finally {
    clearTimeout(timer);
  }
}
async function lookupRetroArchHost(username, options = {}) {
  const entries = await fetchRetroArchLobby(options);
  return findRetroArchHost(entries, username) || { ready: false, state: "waiting_for_host_start" };
}
module.exports = {
  RETROARCH_LOBBY_URL, MAX_LOBBY_BYTES, parseRetroArchLobby,
  endpointFromEntry, findRetroArchHost, fetchRetroArchLobby,
  lookupRetroArchHost, safeHost, safeSession
};
