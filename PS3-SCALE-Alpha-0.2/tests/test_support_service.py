#!/usr/bin/env python3
"""Static security and deployment contract checks for PS3 SCALE support."""
import pathlib
import sys

root = pathlib.Path(sys.argv[1]).resolve().parent
migration = (root / "supabase/migrations/202611100001_ps3scale_support.sql").read_text(
    encoding="utf-8"
)
edge = (root / "supabase/functions/ps3scale-support/index.ts").read_text(
    encoding="utf-8"
)
config = (root / "supabase/config.toml").read_text(encoding="utf-8")
main = (root / "PS3-SCALE-Alpha-0.2/source/ps3/main.c").read_text(
    encoding="utf-8"
)
readme = (root / "supabase/README.md").read_text(encoding="utf-8")

# Database privacy: reports and rate-limit state are not public API tables.
assert "create table if not exists public.ps3scale_support_reports" in migration
assert "create table if not exists public.ps3scale_support_rate_limits" in migration
assert "enable row level security" in migration
assert "revoke all on table public.ps3scale_support_reports from anon" in migration
assert "revoke all on table public.ps3scale_support_reports from authenticated" in migration
assert "revoke all on table public.ps3scale_support_rate_limits from anon" in migration
assert "revoke all on table public.ps3scale_support_rate_limits from authenticated" in migration
assert "grant all on table public.ps3scale_support_reports to service_role" in migration
assert "grant all on table public.ps3scale_support_rate_limits to service_role" in migration
assert "request_id uuid not null" in migration
assert "unique index if not exists ps3scale_support_reports_request_id_uidx" in migration

# Rate limiting must be atomic and callable only by the Edge Function role.
assert "create or replace function public.consume_ps3scale_support_rate_limit" in migration
assert "security definer" in migration
assert "set search_path = ''" in migration
assert "on conflict (ip_hash, window_date)" in migration
assert "where public.ps3scale_support_rate_limits.report_count < p_limit" in migration
assert "return found;" in migration
assert "revoke all on function public.consume_ps3scale_support_rate_limit(text, integer)" in migration
assert "from public, anon, authenticated" in migration
assert "grant execute on function public.consume_ps3scale_support_rate_limit(text, integer)" in migration
assert "to service_role" in migration

# The public function validates bounded input and stable request IDs.
assert 'request.method !== "POST"' in edge
assert "12 * 1024" in edge
assert "invalid_report_kind" in edge
assert "valid_request_id_required" in edge
assert "request_id" in edge
assert "title_and_details_required" in edge
assert "hmacSha256Hex(serviceRoleKey, clientIp)" in edge
assert '{ name: "HMAC", hash: "SHA-256" }' in edge
assert 'request.headers.get("cf-connecting-ip")' in edge
assert '.rpc(' in edge and '"consume_ps3scale_support_rate_limit"' in edge
assert "{ p_ip_hash: ipHash, p_limit: 8 }" in edge

# Retries are idempotent: a request UUID cannot create a second report/issue.
assert '.eq("request_id", requestId)' in edge
assert 'onConflict: "request_id"' in edge
assert "ignoreDuplicates: true" in edge
assert "duplicate: true" in edge
assert "duplicate: false" in edge

# No public report reads, raw IP storage, or service secrets in the PS3 binary.
assert '.get("x-forwarded-for")' not in edge.lower()
assert "ip_hash: ipHash" in edge
assert "clientIp" in edge and "ipHash" in edge
assert "SUPABASE_SERVICE_ROLE_KEY" not in main
assert "SUPABASE_SECRET_KEYS" in edge
assert "PS3SCALE_GITHUB_TOKEN" not in main
assert "PS3SCALE_SUPPORT_URL" in main
assert "PS3SCALE_SUPPORT_API_KEY" in main
assert "YOUR_PUBLISHABLE_KEY" in main
assert "npm:@supabase/supabase-js@2.119.2" in (root / "supabase/functions/ps3scale-support/deno.json").read_text(encoding="utf-8")
assert "verify_jwt = false" in config
gitignore = (root / ".gitignore").read_text(encoding="utf-8")
assert "supabase/functions/.env" in gitignore
assert "supabase/.temp/" in gitignore

# Deployment is explicitly isolated and the GitHub token remains optional.
assert "project dedicated to PS3 SCALE support" in readme
assert "isolated from every other Supabase project" in readme
assert "separate Supabase organization on the Free Plan" in readme
assert "Do not create this project in the existing `VIKIK` organization" in readme
assert "Nano compute at $0/month" in readme
assert "Never fall back to creating the project in `VIKIK`." in readme
assert "Do not reuse" in readme
assert "supabase db push --linked --yes" in readme
assert "supabase functions deploy ps3scale-support --no-verify-jwt --yes" in readme
assert "--remote=" not in readme
assert "HMAC-SHA-256" in readme
assert "PS3SCALE_GITHUB_TOKEN" in readme
assert "If no token is configured" in readme
print("PS3 SCALE support service security contract: PASS")
