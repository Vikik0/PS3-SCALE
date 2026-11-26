# PS3 SCALE support service

This backend is isolated from every other Supabase project. It must be deployed only to a newly created project dedicated to PS3 SCALE support.

## Architecture

- The PS3 app writes each report as an atomic JSON file under its own UUID in the local outbox before attempting any network request.
- The outbox uses a stable UUID request ID. Pending reports survive app restarts and can be retried from Help & Support; retrying the same report cannot create a duplicate database row or GitHub issue.
- The app sends only report kind, title, details, model, firmware, platform, app version and request ID.
- The public Edge Function validates the payload, enforces a 12 KiB request limit and a maximum of 100 title characters / 2,000 detail characters.
- An HMAC-SHA-256 hash of the Cloudflare-provided client IP, keyed with the server-side Supabase secret, is used for an atomic limit of 8 reports per UTC day. Raw IP addresses are never stored.
- Reports and rate-limit state have RLS enabled. Public and authenticated clients have no table access; only the Edge Function's server-side secret key can access them.
- A GitHub Issue integration is optional. It is disabled unless a fine-grained GitHub token is deliberately configured as a server-side secret. By default, reports stay in the dedicated Supabase database.

## Free-plan deployment

The deployment must remain on Supabase's Free Plan. Do not create this project in the existing `VIKIK` organization: that organization is on the Pro Plan, and Supabase quoted approximately $10/month for an additional project there. A Pro organization cannot mix free and paid projects.

1. Create a **separate Supabase organization on the Free Plan** from the Supabase Dashboard. Do not upgrade it and do not add paid project add-ons.
2. In that Free Plan organization, create a new project named `ps3scale-support`. Do not reuse `Vikik-Winter-Fit`, `BRKNDA-Workspace`, or any other existing project.
3. Select Frankfurt (`eu-central-1`) for Jordan/Europe access. Keep the default Free/Nano compute.
4. Confirm the new organization is still on the Free Plan and the project is using Nano compute before deployment. If the Dashboard shows any nonzero project cost, stop; do not confirm or create it.
5. Install the current Supabase CLI (v3) and authenticate:
   ```sh
   supabase login
   ```
6. From the repository root, link only the new Free Plan project:
   ```sh
   supabase link <NEW_PROJECT_REF>
   ```
7. Apply the isolated database migration:
   ```sh
   supabase db push --linked --yes
   ```
8. Deploy the public, rate-limited Edge Function (it validates its own payload and uses only server-side credentials):
   ```sh
   supabase functions deploy ps3scale-support --no-verify-jwt --yes
   ```
9. The function URL is:
   ```
   https://<NEW_PROJECT_REF>.supabase.co/functions/v1/ps3scale-support
   ```
10. Only after the migration and function are verified, replace the `YOUR_PROJECT` placeholder in `PS3SCALE_SUPPORT_URL` in `PS3-SCALE-Alpha-0.2/source/ps3/main.c`, replace `YOUR_PUBLISHABLE_KEY` with that project's publishable key, bump the app version, and build a new PKG.

### Free-plan limits and trade-offs

- The Free Plan includes up to two active free projects per eligible organization and uses Nano compute at $0/month.
- The support backend must stay within the Free Plan quotas (including database size, storage, Edge Function invocations and egress). Do not enable paid add-ons or upgrade the organization.
- Free projects can be paused after a period of low activity. If Supabase pauses this support project, restore it from the Dashboard; do not upgrade to Pro just to avoid pausing.
- If the new Free organization cannot be created or its free-project quota is exhausted, stop and resolve that separately. Never fall back to creating the project in `VIKIK`.

## Optional GitHub Issues

Do not configure this by default. If the user later wants reports mirrored into GitHub Issues, create a fine-grained token restricted to issue creation in `Vikik0/PS3-SCALE`, then set:

```sh
supabase secrets set PS3SCALE_GITHUB_TOKEN=<FINE_GRAINED_TOKEN> PS3SCALE_GITHUB_REPOSITORY=Vikik0/PS3-SCALE --yes
```

The token stays in Supabase Edge Function secrets and is never embedded in the PS3 app. If no token is configured, reports are still stored in Supabase and no GitHub Issue is created.

## Privacy and safety

- No PSN ID, console ID, account token, raw IP address, or unique console identifier is sent by the app.
- The app does not send any report until the local atomic outbox write succeeds.
- Pending JSON files are stored separately under `/dev_hdd0/game/PS3SCALE1/USRDIR/support/outbox/`; the Help & Support screen can retry them after a restart using the same request ID.
- Public callers cannot read reports or rate-limit rows. The Edge Function uses a server-side secret key supplied by the Supabase runtime.
- The database function that increments the daily counter is atomic and executable only by the server-side role.
- The endpoint returns only a receipt ID and optional GitHub issue URL, never the stored report body.
- GitHub mirroring is opt-in because it sends report text to a second service.
