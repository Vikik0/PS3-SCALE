create extension if not exists pgcrypto;

create table if not exists public.cb_profiles (
  id uuid primary key references auth.users(id) on delete cascade,
  display_name text not null default 'Guest',
  created_at timestamptz not null default now(),
  constraint cb_profiles_name_length check (char_length(display_name) between 2 and 24)
);
create table if not exists public.cb_rooms (
  id uuid primary key default gen_random_uuid(),
  room_code text not null unique,
  host_id uuid not null references public.cb_profiles(id) on delete cascade,
  is_private boolean not null default false,
  status text not null default 'waiting' check (status in ('waiting','playing','finished')),
  max_players smallint not null default 2 check (max_players = 2),
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);
create table if not exists public.cb_room_players (
  room_id uuid not null references public.cb_rooms(id) on delete cascade,
  player_id uuid not null references public.cb_profiles(id) on delete cascade,
  seat smallint not null check (seat in (1,2)),
  is_ready boolean not null default false,
  last_seen timestamptz not null default now(),
  primary key (room_id, player_id),
  unique (room_id, seat)
);
create table if not exists public.cb_friend_requests (
  id uuid primary key default gen_random_uuid(),
  sender_id uuid not null references public.cb_profiles(id) on delete cascade,
  receiver_id uuid not null references public.cb_profiles(id) on delete cascade,
  status text not null default 'pending' check (status in ('pending','accepted','declined')),
  created_at timestamptz not null default now(),
  unique (sender_id, receiver_id),
  check (sender_id <> receiver_id)
);
create table if not exists public.cb_match_results (
  id uuid primary key default gen_random_uuid(),
  room_id uuid references public.cb_rooms(id) on delete set null,
  player_id uuid not null references public.cb_profiles(id) on delete cascade,
  result text not null check (result in ('win','loss','draw')),
  score integer not null default 0,
  duration_ms integer not null default 0,
  created_at timestamptz not null default now()
);
create index if not exists cb_rooms_status_created_idx on public.cb_rooms(status, created_at desc);
create index if not exists cb_room_players_player_idx on public.cb_room_players(player_id);
create index if not exists cb_friend_requests_receiver_idx on public.cb_friend_requests(receiver_id, status);
create index if not exists cb_match_results_player_idx on public.cb_match_results(player_id, created_at desc);

create or replace function public.cb_handle_new_user()
returns trigger language plpgsql security definer set search_path = public
as $$
begin
  insert into public.cb_profiles(id, display_name)
  values (new.id, coalesce(nullif(new.raw_user_meta_data ->> 'display_name',''), 'Guest'))
  on conflict (id) do nothing;
  return new;
end;
$$;
drop trigger if exists cb_auth_user_created on auth.users;
create trigger cb_auth_user_created after insert on auth.users
for each row execute procedure public.cb_handle_new_user();

alter table public.cb_profiles enable row level security;
alter table public.cb_rooms enable row level security;
alter table public.cb_room_players enable row level security;
alter table public.cb_friend_requests enable row level security;
alter table public.cb_match_results enable row level security;

drop policy if exists "profiles readable by authenticated players" on public.cb_profiles;
create policy "profiles readable by authenticated players" on public.cb_profiles for select to authenticated using (true);
drop policy if exists "players update own profile" on public.cb_profiles;
create policy "players update own profile" on public.cb_profiles for update to authenticated using (id = auth.uid()) with check (id = auth.uid());

drop policy if exists "authenticated players read rooms" on public.cb_rooms;
create policy "authenticated players read rooms" on public.cb_rooms for select to authenticated using (true);
drop policy if exists "host creates room" on public.cb_rooms;
create policy "host creates room" on public.cb_rooms for insert to authenticated with check (host_id = auth.uid());
drop policy if exists "host updates room" on public.cb_rooms;
create policy "host updates room" on public.cb_rooms for update to authenticated using (host_id = auth.uid()) with check (host_id = auth.uid());

drop policy if exists "players read room members" on public.cb_room_players;
create policy "players read room members" on public.cb_room_players for select to authenticated using (true);
drop policy if exists "players join as themselves" on public.cb_room_players;
create policy "players join as themselves" on public.cb_room_players for insert to authenticated with check (player_id = auth.uid());
drop policy if exists "players update own room presence" on public.cb_room_players;
create policy "players update own room presence" on public.cb_room_players for update to authenticated using (player_id = auth.uid()) with check (player_id = auth.uid());
drop policy if exists "players leave own room" on public.cb_room_players;
create policy "players leave own room" on public.cb_room_players for delete to authenticated using (player_id = auth.uid());

drop policy if exists "friend request participants read" on public.cb_friend_requests;
create policy "friend request participants read" on public.cb_friend_requests for select to authenticated using (sender_id = auth.uid() or receiver_id = auth.uid());
drop policy if exists "players send own friend requests" on public.cb_friend_requests;
create policy "players send own friend requests" on public.cb_friend_requests for insert to authenticated with check (sender_id = auth.uid());
drop policy if exists "receiver responds to request" on public.cb_friend_requests;
create policy "receiver responds to request" on public.cb_friend_requests for update to authenticated using (receiver_id = auth.uid()) with check (receiver_id = auth.uid());

drop policy if exists "leaderboard visible to authenticated players" on public.cb_match_results;
create policy "leaderboard visible to authenticated players" on public.cb_match_results for select to authenticated using (true);
drop policy if exists "players submit own match results" on public.cb_match_results;
create policy "players submit own match results" on public.cb_match_results for insert to authenticated with check (player_id = auth.uid());

do $$
begin
  if exists (select 1 from pg_publication where pubname = 'supabase_realtime') then
    begin alter publication supabase_realtime add table public.cb_rooms;
    exception when duplicate_object then null; end;
    begin alter publication supabase_realtime add table public.cb_room_players;
    exception when duplicate_object then null; end;
  end if;
end;
$$;
