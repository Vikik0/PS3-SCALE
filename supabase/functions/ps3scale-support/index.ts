import { createClient } from "https://esm.sh/@supabase/supabase-js@2";

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

Deno.serve(async (request) => {
  if (request.method === "OPTIONS") {
    return new Response("ok", { status: 200, headers: corsHeaders });
  }
  if (request.method !== "POST") {
    return reply(405, { ok: false, error: "method_not_allowed" });
  }

  let body: Record<string, unknown>;
  try {
    body = await request.json();
  } catch {
    return reply(400, { ok: false, error: "invalid_json" });
  }

  const kind = body.kind === "feedback" ? "feedback" : "issue";
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
  const serviceRoleKey = Deno.env.get("SUPABASE_SERVICE_ROLE_KEY");
  const rateLimitSalt = Deno.env.get("PS3SCALE_SUPPORT_RATE_LIMIT_SALT");
  if (!supabaseUrl || !serviceRoleKey || !rateLimitSalt) {
    return reply(503, { ok: false, error: "support_service_not_configured" });
  }

  const forwarded = request.headers.get("cf-connecting-ip") ??
    request.headers.get("x-forwarded-for")?.split(",")[0]?.trim() ??
    "unknown";
  const ipHash = await sha256Hex(`${rateLimitSalt}:${forwarded}`);
  const admin = createClient(supabaseUrl, serviceRoleKey, {
    auth: { persistSession: false, autoRefreshToken: false },
  });

  const since = new Date(Date.now() - 24 * 60 * 60 * 1000).toISOString();
  const { count, error: rateError } = await admin
    .from("ps3scale_support_reports")
    .select("id", { count: "exact", head: true })
    .eq("ip_hash", ipHash)
    .gte("created_at", since);

  if (rateError) {
    return reply(503, { ok: false, error: "rate_limit_check_failed" });
  }
  if ((count ?? 0) >= 8) {
    return reply(429, { ok: false, error: "daily_report_limit_reached" });
  }

  const { data: inserted, error: insertError } = await admin
    .from("ps3scale_support_reports")
    .insert({
      kind,
      title,
      details,
      model,
      firmware,
      platform,
      app_version: appVersion,
      ip_hash: ipHash,
    })
    .select("id")
    .single();

  if (insertError || !inserted) {
    return reply(500, { ok: false, error: "report_store_failed" });
  }

  let issueUrl: string | null = null;
  const githubToken = Deno.env.get("PS3SCALE_GITHUB_TOKEN");
  const repository = Deno.env.get("PS3SCALE_GITHUB_REPOSITORY") ??
    "Vikik0/PS3-SCALE";

  if (githubToken) {
    const issueBody = [
      "**Source:** PS3 SCALE app",
      "**Type:** " + kind,
      "**Model:** " + model,
      "**Firmware:** " + firmware,
      "**Platform:** " + platform,
      "**App version:** " + appVersion,
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
        issueUrl = typeof issue.html_url === "string" ? issue.html_url : null;
        if (issueUrl) {
          await admin
            .from("ps3scale_support_reports")
            .update({ github_issue_url: issueUrl })
            .eq("id", inserted.id);
        }
      }
    } catch {
      // The database record remains saved even if GitHub issue creation fails.
    }
  }

  return reply(201, {
    ok: true,
    stored: true,
    report_id: inserted.id,
    github_issue_url: issueUrl,
  });
});
