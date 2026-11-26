"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");

process.env.CB_TOKEN_SECRET = "test-only-secret-with-more-than-forty-characters-123456";

const {
  signGuestToken,
  verifyGuestToken,
  cleanDisplayName
} = require("../api/_lib/security");

test("guest token signs and verifies room-scoped identity", () => {
  const token = signGuestToken({ sub: "player-123", room: "ABC234" });
  const claims = verifyGuestToken(token);
  assert.equal(claims.sub, "player-123");
  assert.equal(claims.room, "ABC234");
});

test("guest token rejects tampering", () => {
  const token = signGuestToken({ sub: "player-123", room: "ABC234" });
  const [payload, signature] = token.split(".");
  const index = Math.floor(signature.length / 2);
  const replacement = signature[index] === "A" ? "B" : "A";
  const alteredSignature =
    signature.slice(0, index) + replacement + signature.slice(index + 1);
  assert.equal(verifyGuestToken(payload + "." + alteredSignature), null);
});

test("display names are bounded and strip markup/control characters", () => {
  assert.equal(cleanDisplayName("  <Guest>\n "), "Guest");
  assert.equal(cleanDisplayName("Crash Player"), "Crash Player");
  assert.equal(cleanDisplayName("A"), "Guest");
});
