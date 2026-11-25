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
  assert.match(script, /this browser page only tracks the room/);
  assert.match(script, /this browser lobby does not run gameplay/);
  assert.match(html, /id="roomNotice"[^>]*role="status"[^>]*aria-live="polite"/);
});

test("create-room failures are shown as errors rather than unavailable game modes", () => {
  assert.match(script, /function showNotice\(message,warning=false\)/);
  assert.match(script, /catch\(e\)\{showNotice\("Could not create battle: "\+e\.message,true\)\}/);
  assert.match(html, /id="modeNotice"[^>]*role="alert"[^>]*aria-live="polite"/);
});
