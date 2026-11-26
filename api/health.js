"use strict";

module.exports = function handler(_req, res) {
  res.setHeader("Cache-Control", "no-store");
  return res.status(200).json({
    ok: true,
    service: "Crash Bash Enhanced lobby",
    version: "0.8.7",
    roomTtlMinutes: 30,
    playerTtlMinutes: 10,
    minimumReadyPlayers: 2,
    roomHostAssignment: "creator-with-active-seat-promotion",
    roomListing: "cursor-paginated",
    expiredRoomCleanup: "conditional",
    nativeDiscPicker: "psxiso-hdd-usb",
    nativeLaunchModes: ["retroarch-host", "retroarch-auto-connect"],
    discFingerprintVerification: "fnv1a-64-content-and-track-layout",
    discConfirmationReset: "on-selection-change-or-unready-or-membership-change",
    discConfirmationRequiresReady: true,
    startedMatchDiscRetry: "same-confirmed-fingerprint-only",
    membershipJoinReset: "all-active-ready-and-disc-confirmations",
    netplayHostValidation: "retroarch-only-relay-first-no-direct-downgrade",
    passwordProtectedHosts: "blocked-from-automatic-join",
    readyLeaseSeconds: 90,
    heartbeatSemantics: "presence-only-explicit-ready-action",
    readyChangesAfterStart: "locked",
    automaticHostDiscovery: "retroarch-public-lobby-relay",
    roomLaunchLock: "host-started-netplay-session",
    roomLaunchCancelWindowSeconds: 30,
    psnRequired: false,
    maxPlayersPerRoom: 4,
    storageConfigured: Boolean(process.env.BLOB_READ_WRITE_TOKEN),
    tokenConfigured: Boolean(process.env.CB_TOKEN_SECRET_V2 || process.env.CB_TOKEN_SECRET),
    gameplayTransport: "retroarch-netplay",
    startedRoomSeatTtlMinutes: 30,
    note: "The Vercel API performs room matchmaking and authenticated host-relay discovery. Actual synchronized play still requires PS3 hardware validation."
  });
};
