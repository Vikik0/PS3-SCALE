# PS3 SCALE support service

This backend is isolated from every other Supabase project. It must be deployed only to a newly created project dedicated to PS3 SCALE support.

## Architecture

- The PS3 app writes each report to its own local outbox file before attempting any network request.
- The outbox uses a stable UUID request ID. Retrying the same report cannot create a duplicate database row or GitHub issue.
- The app sends only report kind, title, details, model, firmware, platform, app version and request ID.
- The public Edge Function validates the payload, enforces a 12 KiB request limit and a maximum of 100 title characters / 2,000 detail characters.
- A salted SHA-256 hash of the Cloudflare-provided client IP is used for an atomic limit of 8 reports per UTC day. Raw IP addresses are never stored.
- Reports and rate-limit state have RLS enabled. Public and authenticated clients have no table access; only the Edge Function's server-side service role can access them.
- A GitHub Issue integration is optional. It is disabled unless a fine-grained GitHub token is deliberately configured as a server-side secret. By default, reports stay in the dedicated Supabase database.

## Create and deploy

1. Create a **new Supabase project** named `ps3scale-support` in the VIKIK organization. Do not reuse `Vikik-Winter-Fit`, `BRKNDA-Workspace`, or any other existing project.
2. Prefer the Frankfurt region (`eu-central-1`) for Jordan/Europe access.
3. Install the Supabase CLI and authenticate:
   ```sh
   supabase login
   ```
4. From the repository root, link only the newly created project:
   ```sh
   supabase link --project-ref <NEW_PROJECT_REF>
   ```
5. Apply the isolated database migration:
   ```sh
   supabase db push
   ```
6. Generate a strong random salt of at least 32 characters and configure it as a server-side secret:
   ```sh
   supabase secrets set PS3SCALE_SUPPORT_RATE_LIMIT_SALT=<LONG_RANDOM_VALUE>
   ```
7. Deploy the public, rate-limited Edge Function (it validates its own payload and uses only server-side credentials):
   ```sh
   supabase functions deploy ps3scale-support --no-verify-jwt
   ```
8. The function URL is:
   ```
   https://<NEW_PROJECT_REF>.supabase.co/functions/v1/ps3scale-support
   ```
9. Only after the migration and function are verified, replace the `YOUR_PROJECT` placeholder in `PS3SCALE_SUPPORT_URL` in `PS3-SCALE-Alpha-0.2/source/ps3/main.c`, bump the app version, and build a new PKG.

## Optional GitHub Issues

Do not configure this by default. If the user later wants reports mirrored into GitHub Issues, create a fine-grained token restricted to issue creation in `Vikik0/PS3-SCALE`, then set:

```sh
supabase secrets set PS3SCALE_GITHUB_TOKEN=<FINE_GRAINED_TOKEN> PS3SCALE_GITHUB_REPOSITORY=Vikik0/PS3-SCALE
```

The token stays in Supabase Edge Function secrets and is never embedded in the PS3 app. If no token is configured, reports are still stored in Supabase and no GitHub Issue is created.

## Privacy and safety

- No PSN ID, console ID, account token, raw IP address, or unique console identifier is sent by the app.
- The app does not send any report until the local atomic outbox write succeeds.
- Outbox files are stored separately under `/dev_hdd0/game/PS3SCALE1/USRDIR/support/outbox/`; a retry reuses the same request ID.
- Public callers cannot read reports or rate-limit rows. The Edge Function uses a server-side service-role key supplied by the Supabase runtime.
- The database function that increments the daily counter is atomic and executable only by `service_role`.
- The endpoint returns only a receipt ID and optional GitHub issue URL, never the stored report body.
- GitHub mirroring is opt-in because it sends report text to a second service.
