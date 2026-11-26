create table if not exists public.ps3scale_support_reports (
    id uuid primary key default gen_random_uuid(),
    request_id uuid not null,
    kind text not null check (kind in ('issue', 'feedback')),
    title text not null check (char_length(btrim(title)) between 1 and 100),
    details text not null check (char_length(btrim(details)) between 1 and 2000),
    model text not null default 'UNKNOWN',
    firmware text not null default 'UNKNOWN',
    platform text not null default 'UNKNOWN',
    app_version text not null default 'UNKNOWN',
    ip_hash text not null check (ip_hash ~ '^[0-9a-f]{64}$'),
    github_issue_url text,
    created_at timestamptz not null default now()
);

alter table public.ps3scale_support_reports
    add column if not exists request_id uuid default gen_random_uuid();

update public.ps3scale_support_reports
set request_id = gen_random_uuid()
where request_id is null;

alter table public.ps3scale_support_reports
    alter column request_id set not null;

create unique index if not exists ps3scale_support_reports_request_id_uidx
    on public.ps3scale_support_reports (request_id);

create index if not exists ps3scale_support_reports_rate_limit_idx
    on public.ps3scale_support_reports (ip_hash, created_at desc);

alter table public.ps3scale_support_reports enable row level security;
revoke all on table public.ps3scale_support_reports from anon;
revoke all on table public.ps3scale_support_reports from authenticated;
grant all on table public.ps3scale_support_reports to service_role;

-- A compact UTC-day counter makes the public endpoint's limit atomic under
-- concurrent requests. Only the Edge Function's service_role can use it.
create table if not exists public.ps3scale_support_rate_limits (
    ip_hash text not null check (ip_hash ~ '^[0-9a-f]{64}$'),
    window_date date not null,
    report_count integer not null default 0
        check (report_count between 1 and 8),
    primary key (ip_hash, window_date)
);

create index if not exists ps3scale_support_rate_limits_window_idx
    on public.ps3scale_support_rate_limits (window_date);

alter table public.ps3scale_support_rate_limits enable row level security;
revoke all on table public.ps3scale_support_rate_limits from anon;
revoke all on table public.ps3scale_support_rate_limits from authenticated;
grant all on table public.ps3scale_support_rate_limits to service_role;

create or replace function public.consume_ps3scale_support_rate_limit(
    p_ip_hash text,
    p_limit integer default 8
)
returns boolean
language plpgsql
security definer
set search_path = ''
as $$
declare
    v_window_date date := (pg_catalog.now() at time zone 'utc')::date;
    v_report_count integer;
begin
    if p_ip_hash is null
       or p_ip_hash !~ '^[0-9a-f]{64}$'
       or p_limit is null
       or p_limit < 1
       or p_limit > 8 then
        return false;
    end if;

    delete from public.ps3scale_support_rate_limits
    where window_date < v_window_date - 7;

    insert into public.ps3scale_support_rate_limits (
        ip_hash, window_date, report_count
    ) values (
        p_ip_hash, v_window_date, 1
    )
    on conflict (ip_hash, window_date)
    do update set report_count =
        public.ps3scale_support_rate_limits.report_count + 1
    where public.ps3scale_support_rate_limits.report_count < p_limit
    returning report_count into v_report_count;

    return found;
end;
$$;

revoke all on function public.consume_ps3scale_support_rate_limit(text, integer)
    from public, anon, authenticated;
grant execute on function public.consume_ps3scale_support_rate_limit(text, integer)
    to service_role;
