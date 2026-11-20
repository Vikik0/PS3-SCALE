"use strict";

module.exports = function handler(_req, res) {
  res.setHeader("Cache-Control", "no-store");
  return res.status(200).json({
    ok: true,
    service: "Crash Bash Enhanced lobby",
    version: "0.2.0",
    psnRequired: false,
    maxPlayersPerRoom: 2,
    storageConfigured: Boolean(process.env.BLOB_READ_WRITE_TOKEN),
    tokenConfigured: Boolean(process.env.CB_TOKEN_SECRET),
    gameplayTransport: "not connected yet",
    note: "This endpoint verifies the lobby service only; it does not claim that Crash Bash gameplay is online."
  });
};
