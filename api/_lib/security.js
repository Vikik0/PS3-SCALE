"use strict";

const crypto = require("node:crypto");

function getSecret() {
  const secret = process.env.CB_TOKEN_SECRET_V2 || process.env.CB_TOKEN_SECRET;
  if (!secret || secret.length < 40) {
    throw new Error("CB_TOKEN_SECRET is not configured");
  }
  return secret;
}

function base64url(value) {
  return Buffer.from(value).toString("base64url");
}

function signGuestToken(claims) {
  const now = Math.floor(Date.now() / 1000);
  const payload = {
    sub: claims.sub,
    room: claims.room,
    iat: now,
    exp: now + 60 * 60 * 12
  };
  const encoded = base64url(JSON.stringify(payload));
  const signature = crypto
    .createHmac("sha256", getSecret())
    .update(encoded)
    .digest("base64url");
  return encoded + "." + signature;
}

function verifyGuestToken(token) {
  if (typeof token !== "string" || token.length > 2048) return null;
  const parts = token.split(".");
  if (parts.length !== 2) return null;

  let expected;
  try {
    expected = crypto
      .createHmac("sha256", getSecret())
      .update(parts[0])
      .digest();
  } catch {
    return null;
  }

  let supplied;
  try {
    supplied = Buffer.from(parts[1], "base64url");
  } catch {
    return null;
  }
  if (expected.length !== supplied.length ||
      !crypto.timingSafeEqual(expected, supplied)) return null;

  try {
    const claims = JSON.parse(Buffer.from(parts[0], "base64url").toString("utf8"));
    const now = Math.floor(Date.now() / 1000);
    if (!claims.sub || !claims.room || !Number.isInteger(claims.exp) ||
        claims.exp <= now || claims.exp > now + 60 * 60 * 13) return null;
    return claims;
  } catch {
    return null;
  }
}

function cleanDisplayName(value) {
  if (typeof value !== "string") return "Guest";
  const name = value.replace(/[<>\u0000-\u001f]/g, "").trim().slice(0, 20);
  return name.length >= 2 ? name : "Guest";
}

function bearerToken(req) {
  const value = req.headers?.authorization || "";
  const match = /^Bearer\s+(.+)$/i.exec(value);
  return match ? match[1].trim() : "";
}

module.exports = {
  signGuestToken,
  verifyGuestToken,
  cleanDisplayName,
  bearerToken
};
