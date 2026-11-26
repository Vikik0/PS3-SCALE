"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const handler = require("../api/health");

test("health endpoint reports lobby readiness without claiming gameplay transport", () => {
  const response = {
    statusCode: 0,
    headers: {},
    body: null,
    setHeader(key, value) { this.headers[key] = value; return this; },
    status(code) { this.statusCode = code; return this; },
    json(value) { this.body = value; return this; }
  };

  handler({}, response);

  assert.equal(response.statusCode, 200);
  assert.equal(response.headers["Cache-Control"], "no-store");
  assert.equal(response.body.version, "0.8.7");
  assert.equal(response.body.discConfirmationRequiresReady, true);
  assert.equal(response.body.startedMatchDiscRetry, "same-confirmed-fingerprint-only");
  assert.equal(response.body.membershipJoinReset, "all-active-ready-and-disc-confirmations");
  assert.equal(response.body.netplayHostValidation, "retroarch-only-relay-first-no-direct-downgrade");
  assert.equal(response.body.passwordProtectedHosts, "blocked-from-automatic-join");
  assert.equal(response.body.discFingerprintVerification, "fnv1a-64-content-and-track-layout");
  assert.equal(response.body.discConfirmationReset, "on-selection-change-or-unready-or-membership-change");
  assert.equal(response.body.readyLeaseSeconds, 90);
  assert.equal(response.body.heartbeatSemantics, "presence-only-explicit-ready-action");
  assert.equal(response.body.readyChangesAfterStart, "locked");
  assert.equal(response.body.roomTtlMinutes, 30);
  assert.equal(response.body.playerTtlMinutes, 10);
  assert.equal(response.body.minimumReadyPlayers, 2);
  assert.equal(response.body.roomHostAssignment, "creator-with-active-seat-promotion");
  assert.equal(response.body.roomListing, "cursor-paginated");
  assert.equal(response.body.expiredRoomCleanup, "conditional");
  assert.equal(response.body.nativeDiscPicker, "psxiso-hdd-usb");
  assert.deepEqual(response.body.nativeLaunchModes, ["retroarch-host", "retroarch-auto-connect"]);
  assert.equal(response.body.automaticHostDiscovery, "retroarch-public-lobby-relay");
  assert.equal(response.body.roomLaunchLock, "host-started-netplay-session");
  assert.equal(response.body.roomLaunchCancelWindowSeconds, 30);
  assert.equal(response.body.startedRoomSeatTtlMinutes, 30);
  assert.equal(response.body.psnRequired, false);
  assert.equal(response.body.maxPlayersPerRoom, 4);
  assert.equal(response.body.gameplayTransport, "retroarch-netplay");
});
