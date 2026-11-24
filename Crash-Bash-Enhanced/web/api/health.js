"use strict";

function configuredSecret() {
  const secret = process.env.CB_TOKEN_SECRET_V2 || process.env.CB_TOKEN_SECRET || "";
  return secret.length >= 40;
}

module.exports = function handler(_req, res) {
  const storageConfigured = Boolean(process.env.BLOB_READ_WRITE_TOKEN);
  const tokenConfigured = configuredSecret();

  res.setHeader("Cache-Control", "no-store");
  res.setHeader("X-Content-Type-Options", "nosniff");
  return res.status(200).json({
    ok: true,
    service: "Crash Bash Enhanced lobby",
    version: "0.7.0",
    protocolVersion: "0.7.0",
    psnRequired: false,
    maxPlayersPerRoom: 4,
    storageConfigured,
    tokenConfigured,
    roomCoordination: true,
    readyLeaseSeconds: 90,
    matchLockSeconds: 1800,
    gameplayTransport: "RetroArch Netplay",
    gameplayFramesRelayedByApi: false,
    crashbashAppCrossPlay: false,
    note: "The API coordinates rooms, READY state, disc matching and host discovery. RetroArch Netplay carries gameplay; physical PS3 synchronization still requires hardware validation."
  });
};
