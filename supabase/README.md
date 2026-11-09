# PS3 SCALE support service (separate Supabase project)

This backend is deliberately separate from the user's existing Supabase projects and databases. Create a **new Supabase project dedicated to PS3 SCALE support** before deploying it.

## What it does
- Accepts small JSON issue / feedback reports from the PS3 SCALE app over HTTPS.
- Stores reports in the public table public.ps3scale_support_reports.
- Adds model, firmware, platform and app version metadata supplied by the app.
- Optionally creates a GitHub Issue in Vikik0/PS3-SCALE.
- Keeps the GitHub token and Supabase service-role key inside Supabase Edge Function secrets. No secret or bearer token is embedded in the PS3 app.
- Uses a salted SHA-256 hash of the forwarded client IP for a limit of 8 reports per 24 hours. Raw IP addresses are not stored.

## Deploy to a NEW project
1. Create a new Supabase project for PS3 SCALE support. Do not link this folder to any existing voting or business database.
2. Install the Supabase CLI and authenticate: supabase login
3. From the repository root, link only the new project: supabase link --project-ref <NEW_PROJECT_REF>
4. Apply the isolated migration: supabase db push
5. Configure server-side secrets:
   supabase secrets set PS3SCALE_SUPPORT_RATE_LIMIT_SALT=<LONG_RANDOM_VALUE> PS3SCALE_GITHUB_TOKEN=<FINE_GRAINED_TOKEN> PS3SCALE_GITHUB_REPOSITORY=Vikik0/PS3-SCALE
   The GitHub token needs permission to create issues in this repository. If it is omitted, reports are still stored in Supabase but no GitHub Issue is created.
6. Deploy the public, rate-limited function: supabase functions deploy ps3scale-support --no-verify-jwt
7. Copy the function URL:
   https://<NEW_PROJECT_REF>.supabase.co/functions/v1/ps3scale-support
8. Replace the YOUR_PROJECT placeholder in PS3SCALE_SUPPORT_URL in PS3-SCALE-Alpha-0.2/source/ps3/main.c with the new project URL, then build and test the PKG.

## Privacy and safety
- The PS3 app sends only the report title, details, model, firmware, platform and app version.
- No PSN ID, console ID, IP address or account token is sent by the app.
- The Edge Function does not return report contents or permit public reads of the database table.
- Reports are first written to the console's local outbox; the app only attempts the online POST after local saving succeeds.
- The function's public endpoint is protected by request validation and a per-IP-hash rate limit. Review usage and adjust the limit if many users share a public IP.
