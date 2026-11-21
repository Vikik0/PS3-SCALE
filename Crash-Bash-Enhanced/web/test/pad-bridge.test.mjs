import test from "node:test";
import assert from "node:assert/strict";
import { applyPadStateToEmulator, mapPadState } from "../public/pad-bridge.js";

test("maps PS1 face buttons and menu buttons to libretro indices", () => {
  const inputs = new Map(mapPadState(0x4000 | 0x2000 | 0x0001));
  assert.equal(inputs.get(0), 1);  // Cross
  assert.equal(inputs.get(8), 1);  // Circle
  assert.equal(inputs.get(2), 1);  // Select
  assert.equal(inputs.get(1), 0);  // Square remains released
  assert.equal(inputs.get(3), 0);  // Start remains released
});

test("maps right and left analog sticks with full signed range", () => {
  const inputs = new Map(mapPadState(0, [255, 0, 0, 255]));
  assert.equal(inputs.get(16), -32767); // Left X
  assert.equal(inputs.get(17), 32767);  // Left Y
  assert.equal(inputs.get(18), 32767);  // Right X
  assert.equal(inputs.get(19), -32767); // Right Y
});

test("neutral analog sticks map to zero", () => {
  const inputs = new Map(mapPadState(0));
  for (const index of [16, 17, 18, 19]) assert.equal(inputs.get(index), 0);
});

test("applies all digital and analog controls to the selected emulator port", () => {
  const calls = [];
  const gameManager = {
    functions: {
      simulateInput: (...args) => calls.push(args)
    }
  };
  assert.equal(applyPadStateToEmulator(gameManager, 2, 0x4000, [128, 128, 128, 128]), true);
  assert.equal(calls.length, 18);
  assert.deepEqual(calls[0], [2, 0, 1]);
  assert.deepEqual(calls.at(-4), [2, 16, 0]);
  assert.deepEqual(calls.at(-1), [2, 19, 0]);
});

test("rejects invalid controller slots and missing emulator APIs", () => {
  const gameManager = { functions: { simulateInput() {} } };
  assert.equal(applyPadStateToEmulator(gameManager, -1, 0), false);
  assert.equal(applyPadStateToEmulator(gameManager, 4, 0), false);
  assert.equal(applyPadStateToEmulator({}, 0, 0), false);
});
