import { createClient } from "@supabase/supabase-js";

const corsHeaders = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Headers": "content-type",
  "Access-Control-Allow-Methods": "POST, OPTIONS",
};

function reply(status: number, body: Record<string, unknown>) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { ...corsHeaders, "Content-Type": "application/json" },
  });
}

function cleanText(value: unknown, maxLength: number): string | null {
  if (typeof value !== "string") return null;
  const text = value.trim();
  if (!text || text.length > maxLength) return null;
  return text;
}

async function sha256Hex(value: string): Promise<string> {
  const digest = await crypto.subtle.digest(
    "SHA-256",
    new TextEncoder().encode(value),
  );
  return Array.from(new Uint8Array(digest))
    .map((byte) => byte.toString(16).padStart(2, "0"))
    .join("");
}

function isRequestId(value: string | null): value is string {
  return value !== null &&
    /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i
      .test(value);
}

Deno.serve(async (request) => {
  if (request.method === "OPTIONS") {
    return new Response("ok", { status: 200, headers: corsHeaders });
  }
  if (request.method !== "POST") {
    return reply(405, { ok: false, error: "method_not_allowed" });
  }

  let body: Record<string, unknown>;
  try {
    const rawBody = await request.text();
    if (new TextEncoder().encode(rawBody).byteLength > 12 * 1024) {
      return reply(413, { ok: false, error: "request_too_large" });
    }
    body = JSON.parse(rawBody);
    if (!body || typeof body !== "object" || Array.isArray(body)) {
      return reply(400, { ok: false, error: "invalid_json_object" });
    }
  } catch {
    return reply(400, { ok: false, error: "invalid_json" });
  }

  if (body.kind !== "issue" && body.kind !== "feedback") {
    return reply(400, { ok: false, error: "invalid_report_kind" });
  }
  const kind = body.kind;
  const requestId = cleanText(body.request_id, 36);
  if (!isRequestId(requestId)) {
    return reply(400, { ok: false, error: "valid_request_id_required" });
  }

  const title = cleanText(body.title, 100);
  const details = cleanText(body.details, 2000);
  const model = cleanText(body.model, 80) ?? "UNKNOWN";
  const firmware = cleanText(body.firmware, 40) ?? "UNKNOWN";
  const platform = cleanText(body.platform, 80) ?? "UNKNOWN";
  const appVersion = cleanText(body.app_version, 40) ?? "UNKNOWN";

  if (!title || !details) {
    return reply(400, { ok: false, error: "title_and_details_required" });
  }

  const supabaseUrl = Deno.env.get("SUPABASE_URL");
  const secretKeysJson = Deno.env.get("SUPABASE_SECRET_KEYS");
  const rateLimitSalt = Deno.env.get("PS3SCALE_SUPPORT_RATE_LIMIT_SALT");
  let serviceRoleKey: string | undefined;
  try {
    const secretKeys = JSON.parse(secretKeysJson ?? "{}") as
      Record<string, unknown>;
    const defaultSecret = secretKeys.default;
    if (typeof defaultSecret === "string" && defaultSecret.length > 0) {
      serviceRoleKey = defaultSecret;
    }
  } catch {
    serviceRoleKey = undefined;
  }
  if (!supabaseUrl || !serviceRoleKey || !rateLimitSalt ||
      rateLimitSalt.length < 32) {
    return reply(503, { ok: false, error: "support_service_not_configured" });
  }

  // Supabase's Cloudflare edge supplies this header. Do not trust a
  // caller-controlled X-Forwarded-For value for the rate-limit identity.
  const clientIp = request.headers.get("cf-connecting-ip") ?? "unknown";
  const ipHash = await sha256Hex(`${rateLimitSalt}:${clientIp}`);
  const admin = createClient(supabaseUrl, serviceRoleKey, {
    auth: { persistSession: false, autoRefreshToken: false },
  });

  // A retry with the same UUID returns the existing receipt and never
  // creates a second database row or duplicate GitHub issue.
  const { data: existing, error: existingError } = await admin
    .from("ps3scale_support_reports")
    .select("id, github_issue_url")
    .eq("request_id", requestId)
    .maybeSingle();

  if (existingError) {
    return reply(503, { ok: false, error: "idempotency_check_failed" });
  }
  if (existing) {
    return reply(200, {
      ok: true,
      stored: true,
      duplicate: true,
      report_id: existing.id,
      github_issue_url: existing.github_issue_url,
    });
  }

  const { data: allowed, error: rateError } = await admin.rpc(
    "consume_ps3scale_support_rate_limit",
    { p_ip_hash: ipHash, p_limit: 8 },
  );
  if (rateError) {
    return reply(503, { ok: false, error: "rate_limit_check_failed" });
  }
  if (allowed !== true) {
    return reply(429, { ok: false, error: "daily_report_limit_reached" });
  }

  const { data: inserted, error: insertError } = await admin
    .from("ps3scale_support_reports")
    .upsert({
      request_id: requestId,
      kind,
      title,
      details,
      model,
      firmware,
      platform,
      app_version: appVersion,
      ip_hash: ipHash,
    }, { onConflict: "request_id", ignoreDuplicates: true })
    .select("id")
    .maybeSingle();

  if (insertError) {
    return reply(500, { ok: false, error: "report_store_failed" });
  }
  if (!inserted) {
    // A simultaneous retry may have won the unique-key race.
    const { data: duplicate, error: duplicateError } = await admin
      .from("ps3scale_support_reports")
      .select("id, github_issue_url")
      .eq("request_id", requestId)
      .maybeSingle();
    if (duplicateError || !duplicate) {
      return reply(500, { ok: false, error: "report_receipt_unavailable" });
    }
    return reply(200, {
      ok: true,
      stored: true,
      duplicate: true,
      report_id: duplicate.id,
      github_issue_url: duplicate.github_issue_url,
    });
  }

  let issueUrl: string | null = null;
  const githubToken = Deno.env.get("PS3SCALE_GITHUB_TOKEN");
  const configuredRepository = Deno.env.get("PS3SCALE_GITHUB_REPOSITORY") ??
    "Vikik0/PS3-SCALE";
  const repository = /^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$/.test(
      configuredRepository,
    )
    ? configuredRepository
    : "Vikik0/PS3-SCALE";

  if (githubToken) {
    const issueBody = [
      "**Source:** PS3 SCALE app",
      "**Type:** " + kind,
      "**Model:** " + model,
      "**Firmware:** " + firmware,
      "**Platform:** " + platform,
      "**App version:** " + appVersion,
      "**Request ID:** " + requestId,
      "",
      "## Details",
      details,
      "",
      "_A copy of this report is stored in the separate PS3 SCALE support database._",
    ].join("\n");

    try {
      const issueResponse = await fetch(
        `https://api.github.com/repos/${repository}/issues`,
        {
          method: "POST",
          headers: {
            "Accept": "application/vnd.github+json",
            "Authorization": `Bearer ${githubToken}`,
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "PS3-SCALE-Support",
            "Content-Type": "application/json",
          },
          body: JSON.stringify({
            title: `[PS3 SCALE ${kind.toUpperCase()}] ${title}`,
            body: issueBody,
          }),
        },
      );
      if (issueResponse.ok) {
        const issue = await issueResponse.json();
        issueUrl = typeof issue.html_url === "string" &&
            issue.html_url.startsWith("https://github.com/")
          ? issue.html_url
          : null;
        if (issueUrl) {
          await admin
            .from("ps3scale_support_reports")
            .update({ github_issue_url: issueUrl })
            .eq("id", inserted.id);
        }
      }
    } catch {
      // The report remains stored even if optional GitHub issue creation fails.
    }
  }

  return reply(201, {
    ok: true,
    stored: true,
    duplicate: false,
    report_id: inserted.id,
    github_issue_url: issueUrl,
  });
});
