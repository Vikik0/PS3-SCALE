"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const html = fs.readFileSync(path.join(__dirname, "..", "index.html"), "utf8");
const scripts = [...html.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script>/gi)];
const script = scripts.map((match) => match[1]).join("\n");

test("browser lobby has one syntactically valid inline application script", () => {
  assert.equal(scripts.length, 1);
  assert.doesNotThrow(() => new Function(script));
});

test("keyboard focus is routed to room controls while a room is active", () => {
  assert.match(script, /if\(state\.screen==="room"\)/);
  assert.match(script, /function showRoomScreen\(\)/);
  assert.match(script, /state\.screen="room"/);
});

test("entering a room hides the game-type stage and public-room list", () => {
  assert.match(script, /\$\("gameStage"\)\.hidden=true/);
  assert.match(script, /\$\("publicRoomsPanel"\)\.hidden=true/);
  assert.match(script, /showRoomScreen\(\);\$\("roomTitle"\)\.textContent=state\.code/);
});

test("session restoration re-enters the room screen before refreshing its seat", () => {
  assert.match(script, /if\(token&&code\)\{[\s\S]*?showRoomScreen\(\);[\s\S]*?const restored=await refreshRoom\(\)/);
});

test("browser room status distinguishes lobby presence from PS3 gameplay", () => {
  assert.match(script, /this browser page only tracks the room/i);
  assert.match(script, /this browser lobby does not run gameplay/);
  assert.match(html, /id="roomNotice"[^>]*role="status"[^>]*aria-live="polite"/);
});

test("create-room failures are shown as errors rather than unavailable game modes", () => {
  assert.match(script, /function showNotice\(message,warning=false\)/);
  assert.match(script, /catch\(e\)\{showNotice\("Could not create battle: "\+e\.message,true\);\}/);
  assert.match(html, /id="modeNotice"[^>]*role="alert"[^>]*aria-live="polite"/);
});


test("room session generation prevents an old refresh from restoring a room after leave", async () => {
  const vm = require("node:vm");
  const elements = new Map();
  const makeElement = (id = "") => ({
    id, hidden: false, className: "", textContent: "", value: "", checked: false,
    disabled: false, innerHTML: "", style: {},
    classList: { add() {}, remove() {}, toggle() {} },
    querySelectorAll() { return []; },
    querySelector() { return { textContent: "" }; },
    appendChild() {}, replaceChildren() {}, scrollIntoView() {}, focus() {}, click() {}
  });
  const document = {
    getElementById(id) {
      if (!elements.has(id)) elements.set(id, makeElement(id));
      return elements.get(id);
    },
    createElement(tag) { return makeElement(tag); },
    addEventListener() {}
  };
  const storage = new Map();
  const sessionStorage = {
    getItem(key) { return storage.has(key) ? storage.get(key) : null; },
    setItem(key, value) { storage.set(key, String(value)); },
    removeItem(key) { storage.delete(key); }
  };
  let resolveRoomRead;
  let roomReadCount = 0;
  const jsonResponse = (body, status = 200) => ({
    ok: status >= 200 && status < 300,
    status,
    async json() { return body; }
  });
  const fetch = async (url, options = {}) => {
    if (url === "/api/health") {
      return jsonResponse({ ok: true, storageConfigured: true, tokenConfigured: true });
    }
    if (url.startsWith("/api/rooms?code=")) {
      roomReadCount++;
      return new Promise(resolve => { resolveRoomRead = () => resolve(jsonResponse({
        room: {
          code: "ABC234",
          selfPlayerId: "player-1",
          players: [{ id: "player-1", name: "Host", ready: true }],
          maxPlayers: 4,
          gameStarted: false
        }
      })); });
    }
    if (url === "/api/rooms" && options.method === "POST") {
      return jsonResponse({ ok: true });
    }
    if (url === "/api/rooms" && options.method === "GET") {
      return jsonResponse({ rooms: [] });
    }
    throw new Error("Unexpected fetch: " + url);
  };
  const context = {
    document, sessionStorage, fetch, console,
    setTimeout(fn) { fn(); return 1; },
    setInterval() { return 1; },
    clearInterval() {},
    Promise
  };
  vm.runInNewContext(script, context);
  const getState = () => vm.runInContext("state", context);
  await new Promise(resolve => setImmediate(resolve));

  const entering = context.enterRoom({
    token: "guest-token",
    room: { code: "ABC234" },
    player: { id: "player-1", name: "Host" }
  });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(roomReadCount, 1);

  await context.leaveRoom();
  assert.equal(getState().token, "");
  assert.equal(getState().room, null);
  assert.equal(getState().screen, "battle");

  resolveRoomRead();
  await entering;
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(getState().token, "");
  assert.equal(getState().room, null);
  assert.equal(getState().player, null);
  assert.equal(getState().screen, "battle");
  assert.equal(roomReadCount, 1);
});
