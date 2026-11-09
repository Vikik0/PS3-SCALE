create table if not exists public.ps3scale_support_reports (
    id uuid primary key default gen_random_uuid(),
    kind text not null check (kind in ('issue', 'feedback')),
    title text not null check (char_length(btrim(title)) between 1 and 100),
    details text not null check (char_length(btrim(details)) between 1 and 2000),
    model text not null default 'UNKNOWN',
    firmware text not null default 'UNKNOWN',
    platform text not null default 'UNKNOWN',
    app_version text not null default 'UNKNOWN',
    ip_hash text not null,
    github_issue_url text,
    created_at timestamptz not null default now()
);

alter table public.ps3scale_support_reports enable row level security;

revoke all on table public.ps3scale_support_reports from anon;
revoke all on table public.ps3scale_support_reports from authenticated;
grant all on table public.ps3scale_support_reports to service_role;

create index if not exists ps3scale_support_reports_rate_limit_idx
    on public.ps3scale_support_reports (ip_hash, created_at desc);
