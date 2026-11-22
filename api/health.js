"use strict";

module.exports = function handler(_req, res) {
  res.setHeader("Cache-Control", "no-store");
  return res.status(200).json({
    ok: true,
    service: "Crash Bash Enhanced lobby",
    version: "0.3.0",
    roomTtlMinutes: 30,
    playerTtlMinutes: 10,
    psnRequired: false,
    maxPlayersPerRoom: 4,
    storageConfigured: Boolean(process.env.BLOB_READ_WRITE_TOKEN),
    tokenConfigured: Boolean(process.env.CB_TOKEN_SECRET_V2 || process.env.CB_TOKEN_SECRET),
    gameplayTransport: "not connected yet",
    note: "This endpoint verifies the four-seat room lobby only. It does not connect to crashbash.app or claim that Crash Bash gameplay is online."
  });
};
