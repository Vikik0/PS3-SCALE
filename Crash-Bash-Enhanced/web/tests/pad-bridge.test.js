import test from "node:test";
import assert from "node:assert/strict";
import {
  applyPadStateToEmulator,
  createLocalPadCapture,
  mapPadState
} from "../public/pad-bridge.js";

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
  assert.equal(calls.length, 20);
  assert.deepEqual(calls[0], [2, 0, 1]);
  assert.deepEqual(calls.at(-4), [2, 16, 0]);
  assert.deepEqual(calls.at(-1), [2, 19, 0]);
});

test("captures local inputs into the seven-byte CBN1 PS1 pad record", () => {
  const forwarded = [];
  const gameManager = {
    simulateInput: (...args) => forwarded.push(args),
    functions: { simulateInput() {} }
  };
  const capture = createLocalPadCapture(gameManager, () => true);
  gameManager.simulateInput(0, 0, 1);   // Cross
  gameManager.simulateInput(0, 8, 1);   // Circle
  gameManager.simulateInput(0, 16, -32767); // Left X
  gameManager.simulateInput(0, 19, 32767);  // Right Y

  const record = capture.getRecord();
  assert.equal(record.length, 7);
  assert.equal(record[0], 3); // connected + analog sticks
  assert.equal(record[1], 0x00);
  assert.equal(record[2], 0x60); // Cross + Circle
  assert.equal(record[5], 0); // Left X minimum
  assert.equal(record[4], 255); // Right Y maximum
  assert.equal(forwarded.length, 0, "netplay must withhold unsynchronized local input");
});

test("keeps local input working when netplay is inactive", () => {
  const forwarded = [];
  const gameManager = {
    simulateInput: (...args) => forwarded.push(args),
    functions: { simulateInput() {} }
  };
  const capture = createLocalPadCapture(gameManager, () => false);
  gameManager.simulateInput(0, 0, 1);
  assert.deepEqual(forwarded, [[0, 0, 1]]);
  assert.equal(capture.getRecord()[2], 0x40);
});

test("rejects invalid controller slots and missing emulator APIs", () => {
  const gameManager = { functions: { simulateInput() {} } };
  assert.equal(applyPadStateToEmulator(gameManager, -1, 0), false);
  assert.equal(applyPadStateToEmulator(gameManager, 4, 0), false);
  assert.equal(applyPadStateToEmulator({}, 0, 0), false);
});
