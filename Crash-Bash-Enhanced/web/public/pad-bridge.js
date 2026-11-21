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
  [13, 0x0200]  // R2
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
