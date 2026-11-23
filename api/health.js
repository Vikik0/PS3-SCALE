"use strict";

module.exports = function handler(_req, res) {
  res.setHeader("Cache-Control", "no-store");
  return res.status(200).json({
    ok: true,
    service: "Crash Bash Enhanced lobby",
    version: "0.8.0",
    roomTtlMinutes: 30,
    playerTtlMinutes: 10,
    minimumReadyPlayers: 2,
    roomHostAssignment: "creator-with-active-seat-promotion",
    roomListing: "cursor-paginated",
    expiredRoomCleanup: "conditional",
    nativeDiscPicker: "psxiso-hdd-usb",
    nativeLaunchModes: ["retroarch-host", "retroarch-auto-connect"],
    discFingerprintVerification: "fnv1a-64-content-and-track-layout",
    automaticHostDiscovery: "retroarch-public-lobby-relay",
    roomLaunchLock: "host-started-netplay-session",
    psnRequired: false,
    maxPlayersPerRoom: 4,
    storageConfigured: Boolean(process.env.BLOB_READ_WRITE_TOKEN),
    tokenConfigured: Boolean(process.env.CB_TOKEN_SECRET_V2 || process.env.CB_TOKEN_SECRET),
    gameplayTransport: "retroarch-netplay",
    startedRoomSeatTtlMinutes: 30,
    note: "The Vercel API performs room matchmaking and authenticated host-relay discovery. Actual synchronized play still requires PS3 hardware validation."
  });
};
