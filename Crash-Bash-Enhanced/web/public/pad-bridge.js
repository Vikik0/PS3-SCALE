const PS1_TO_LIBRETRO_BUTTONS = Object.freeze([
  [0, 0x4000],  // Cross -> B
  [1, 0x8000],  // Square -> Y
  [2, 0x0001],  // Select
  [3, 0x0008],  // Start
  [4, 0x0010],  // Up
  [5, 0x0040],  // Down
  [6, 0x0080],  // Left
  [7, 0x0020],  // Right
  [8, 0x2000],  // Circle -> A
  [9, 0x1000],  // Triangle -> X
  [10, 0x0400], // L1
  [11, 0x0800], // R1
  [12, 0x0100], // L2
  [13, 0x0200], // R2
  [14, 0x0002], // L3
  [15, 0x0004]  // R3
]);

function axisToLibretro(value) {
  const clamped = Math.max(0, Math.min(255, Number(value) || 0));
  return clamped < 128
    ? Math.round((clamped - 128) * 32767 / 128)
    : Math.round((clamped - 128) * 32767 / 127);
}

/**
 * Convert the PS1 pad layout used by the PS3 bridge and CBN1 netcode
 * into libretro joypad button/axis inputs for EmulatorJS.
 * Stick order is rightX, rightY, leftX, leftY.
 */
export function mapPadState(buttons, sticks = [128, 128, 128, 128]) {
  const mask = Number(buttons) & 0xffff;
  const inputs = PS1_TO_LIBRETRO_BUTTONS.map(([index, bit]) => [
    index,
    (mask & bit) ? 1 : 0
  ]);
  inputs.push(
    [16, axisToLibretro(sticks[2] ?? 128)],
    [17, axisToLibretro(sticks[3] ?? 128)],
    [18, axisToLibretro(sticks[0] ?? 128)],
    [19, axisToLibretro(sticks[1] ?? 128)]
  );
  return inputs;
}

export function applyPadStateToEmulator(gameManager, slot, buttons, sticks) {
  const simulate = gameManager?.functions?.simulateInput;
  if (typeof simulate !== "function" || !Number.isInteger(slot) || slot < 0 || slot > 3) {
    return false;
  }
  for (const [index, value] of mapPadState(buttons, sticks)) {
    simulate(slot, index, value);
  }
  return true;
}

const LIBRETRO_TO_PS1_BUTTONS = new Map(PS1_TO_LIBRETRO_BUTTONS.map(([index, bit]) => [index, bit]));
const CAPTURE_KEY = Symbol.for("crash-bash-online.local-pad-capture");

function axisFromLibretro(value) {
  const clamped = Math.max(-32767, Math.min(32767, Number(value) || 0));
  return clamped < 0
    ? Math.round(128 + clamped * 128 / 32767)
    : Math.round(128 + clamped * 127 / 32767);
}

/**
 * Capture the browser's local keyboard/gamepad inputs as the same 7-byte
 * PS1 pad record used by CBN1. While netplay is active, local input is
 * withheld from port 1 and only applied after the host's frame record arrives.
 */
export function createLocalPadCapture(gameManager, isNetplayActive = () => false) {
  if (!gameManager || typeof gameManager.simulateInput !== "function") {
    throw new TypeError("A ready EmulatorJS GameManager is required.");
  }
  if (gameManager[CAPTURE_KEY]) return gameManager[CAPTURE_KEY];

  const state = {
    flags: 0x03, // PAD_THERE | PAD_STICKS
    buttons: 0,
    sticks: [128, 128, 128, 128] // rightX, rightY, leftX, leftY
  };
  const original = gameManager.simulateInput.bind(gameManager);

  const capture = {
    update(index, value) {
      if (LIBRETRO_TO_PS1_BUTTONS.has(index)) {
        const bit = LIBRETRO_TO_PS1_BUTTONS.get(index);
        state.buttons = value ? (state.buttons | bit) : (state.buttons & ~bit);
      } else if (index >= 16 && index <= 19) {
        const stickIndex = ({ 16: 2, 17: 3, 18: 0, 19: 1 })[index];
        state.sticks[stickIndex] = axisFromLibretro(value);
      }
    },
    getRecord() {
      return new Uint8Array([
        state.flags,
        state.buttons & 0xff,
        (state.buttons >>> 8) & 0xff,
        ...state.sticks
      ]);
    },
    reset() {
      state.buttons = 0;
      state.sticks = [128, 128, 128, 128];
    }
  };

  gameManager.simulateInput = (player, index, value) => {
    if (player === 0 && index >= 0 && index <= 19) {
      capture.update(index, value);
      if (isNetplayActive()) return;
    }
    return original(player, index, value);
  };
  gameManager[CAPTURE_KEY] = capture;
  return capture;
}
const LIBRETRO_TO_PS1_BUTTONS = new Map(PS1_TO_LIBRETRO_BUTTONS.map(([index, bit]) => [index, bit]));

function axisToPad(value) {
  const clamped = Math.max(-32767, Math.min(32767, Number(value) || 0));
  return clamped < 0
    ? Math.max(0, Math.min(255, Math.round(128 + clamped * 128 / 32767)))
    : Math.max(0, Math.min(255, Math.round(128 + clamped * 127 / 32767)));
}

/** Capture the browser's default player-1 controls as a seven-byte PS1 pad record. */
export function createLocalPadCapture() {
  let buttons = 0;
  const sticks = [128, 128, 128, 128]; // rightX, rightY, leftX, leftY

  return {
    handle(player, index, value) {
      if (player !== 0) return false;
      const bit = LIBRETRO_TO_PS1_BUTTONS.get(index);
      if (bit !== undefined) {
        buttons = value ? (buttons | bit) : (buttons & ~bit);
        return true;
      }
      if (index >= 16 && index <= 19) {
        const stickIndex = ({ 16: 2, 17: 3, 18: 0, 19: 1 })[index];
        sticks[stickIndex] = axisToPad(value);
        return true;
      }
      return false;
    },
    snapshot() {
      return new Uint8Array([
        0x03,
        buttons & 0xff,
        (buttons >>> 8) & 0xff,
        sticks[0], sticks[1], sticks[2], sticks[3]
      ]);
    },
    reset() {
      buttons = 0;
      sticks.fill(128);
    }
  };
}

export function decodePadRecord(record, offset = 0) {
  if (!(record instanceof Uint8Array) || offset < 0 || offset + 7 > record.byteLength) {
    throw new RangeError("A complete seven-byte PS1 pad record is required.");
  }
  const flags = record[offset];
  const buttons = record[offset + 1] | (record[offset + 2] << 8);
  const sticks = (flags & 0x02)
    ? Array.from(record.subarray(offset + 3, offset + 7))
    : [128, 128, 128, 128];
  return { connected: (flags & 0x01) !== 0, buttons, sticks, flags };
}
