/*
 * Ghosts Campaign local-session bridge for IW6 / PS3.
 *
 * Reuses the engine-created local split-screen players from the native
 * Multiplayer path. It does not create clients, clone actors, poll pads,
 * write game memory, or alter the EBOOT.
 *
 * Run only after the engine has entered a real two-player split-screen
 * session. On success, Campaign's shared aliases are bound by native
 * primary/secondary identity rather than getentarray order.
 */

main()
{
    /*
     * Idempotent only while the exact same native primary/secondary pair
     * is still alive. If a controller disconnects or the native pair is
     * replaced, fall through to cleanup and a bounded rebind.
     */
    if ( isdefined( level.ghc_campaign_session_ready )
        && level.ghc_campaign_session_ready
        && issplitscreen()
        && isdefined( level.ghc_campaign_player_0 )
        && level.ghc_campaign_player_0 issplitscreenplayer()
        && level.ghc_campaign_player_0 issplitscreenplayerprimary()
        && isdefined( level.ghc_campaign_player_1 )
        && level.ghc_campaign_player_1 issplitscreenplayer()
        && !level.ghc_campaign_player_1 issplitscreenplayerprimary()
        && ghc_campaign_native_pair_is_current()
        && level.player == level.ghc_campaign_player_0
        && level.playengineeffects == level.ghc_campaign_player_1 )
    {
        ghc_campaign_start_session_monitor();
        return 1;
    }

    if ( isdefined( level.ghc_campaign_event_lock_initialized )
        && level.ghc_campaign_event_lock_initialized )
        ghc_campaign_event_lock_clear_active();

    if ( isdefined( level.ghc_campaign_drill_state_initialized )
        && level.ghc_campaign_drill_state_initialized )
    {
        ghc_campaign_drill_abort_slot( 0 );
        ghc_campaign_drill_abort_slot( 1 );
    }

    if ( isdefined( level.ghc_campaign_session_ready )
        && level.ghc_campaign_session_ready )
        ghc_campaign_clear_session_aliases();

    level.ghc_campaign_session_ready = 0;
    level.ghc_campaign_session_status = 0;
    level.ghc_campaign_player_0 = undefined;
    level.ghc_campaign_player_1 = undefined;

    /*
     * The second native entity can appear before its split-screen identity
     * flags are ready. Retry the full identity bind, not just the entity
     * count, for a bounded period. Never return early on an unready pair.
     */
    for ( var_0 = 0; var_0 < 200; var_0++ )
    {
        var_1 = getentarray( "player", "classname" );

        if ( isdefined( var_1 ) )
            level.playerpiggyback = var_1;

        if ( issplitscreen() && isdefined( var_1 ) && var_1.size >= 2 )
        {
            if ( ghc_campaign_bind_native_local_pair() )
            {
                ghc_campaign_start_session_monitor();
                return 1;
            }
        }

        wait 0.05;
    }

    var_1 = getentarray( "player", "classname" );
    if ( !isdefined( var_1 ) )
    {
        /*
         * Keep the last engine-owned player array available to original
         * Campaign scripts, but never bind against it when the fresh query
         * says no live player array exists.
         */
        level.ghc_campaign_session_status = 2;
        ghc_campaign_start_session_monitor();
        return 0;
    }

    level.playerpiggyback = var_1;

    if ( ghc_campaign_bind_native_local_pair() )
    {
        ghc_campaign_start_session_monitor();
        return 1;
    }

    /* Keep waiting for a native pair that may appear after initial startup. */
    ghc_campaign_start_session_monitor();
    return 0;
}
ghc_campaign_start_session_monitor()
{
    if ( isdefined( level.ghc_campaign_session_monitor_active )
        && level.ghc_campaign_session_monitor_active )
        return 1;

    level.ghc_campaign_session_monitor_stop = 0;
    level.ghc_campaign_session_monitor_active = 1;
    thread ghc_campaign_watch_native_session();
    return 1;
}

ghc_campaign_stop_session_monitor()
{
    if ( !isdefined( level.ghc_campaign_session_monitor_active )
        || !level.ghc_campaign_session_monitor_active )
        return 0;

    level.ghc_campaign_session_monitor_stop = 1;
    return 1;
}

/*
 * Watch the exact engine-owned primary/secondary pair after binding.
 * Eight consecutive failed checks (2 seconds at 250 ms) are required
 * before invalidating the session, avoiding a one-frame entity-array
 * transition from clearing valid Campaign aliases.
 *
 * This monitor never creates or rebinds a player. After status 8, the
 * verified bridge main() must be called again to attempt a bounded rebind.
 */
ghc_campaign_watch_native_session()
{
    var_0 = 0;

    while ( !isdefined( level.ghc_campaign_session_monitor_stop )
        || !level.ghc_campaign_session_monitor_stop )
    {
        if ( isdefined( level.ghc_campaign_session_ready )
            && level.ghc_campaign_session_ready )
        {
            if ( ghc_campaign_native_pair_is_current()
                && level.player == level.ghc_campaign_player_0
                && level.playengineeffects == level.ghc_campaign_player_1 )
                var_0 = 0;
            else
            {
                var_0++;

                if ( var_0 >= 8 )
                {
                    ghc_campaign_invalidate_local_session();
                    var_0 = 0;
                }
            }
        }
        else
        {
            /*
             * Keep the watcher alive after invalidation. Rebind only when
             * the engine exposes a fresh player array and split-screen is
             * active; the bind helper still validates both native identities.
             */
            var_1 = getentarray( "player", "classname" );

            if ( isdefined( var_1 ) )
                level.playerpiggyback = var_1;

            if ( issplitscreen() && isdefined( var_1 ) && var_1.size >= 2 )
            {
                if ( ghc_campaign_bind_native_local_pair() )
                    var_0 = 0;
            }
        }

        wait 0.25;
    }

    level.ghc_campaign_session_monitor_active = undefined;
    level.ghc_campaign_session_monitor_stop = undefined;
}

ghc_campaign_bind_native_local_pair()
{
    level.ghc_campaign_session_ready = 0;
    level.ghc_campaign_session_status = 0;
    level.ghc_campaign_player_0 = undefined;
    level.ghc_campaign_player_1 = undefined;

    /*
     * Status:
     * 0 = not checked, 1 = ready, 2 = player array missing,
     * 3 = native split-screen inactive, 4 = duplicate primary,
     * 5 = duplicate secondary, 6 = not exactly two native players,
     * 7 = one of the two native identities is missing,
     * 8 = a previously ready native pair is no longer current.
     */
    if ( !isdefined( level.playerpiggyback ) )
    {
        level.ghc_campaign_session_status = 2;
        return 0;
    }

    if ( !issplitscreen() )
    {
        level.ghc_campaign_session_status = 3;
        return 0;
    }

    var_0 = 0;
    for ( var_1 = 0; var_1 < level.playerpiggyback.size; var_1++ )
    {
        var_2 = level.playerpiggyback[var_1];

        if ( !isdefined( var_2 ) || !var_2 issplitscreenplayer() )
            continue;

        var_0++;

        if ( var_2 issplitscreenplayerprimary() )
        {
            if ( isdefined( level.ghc_campaign_player_0 ) )
            {
                level.ghc_campaign_session_status = 4;
                return 0;
            }

            level.ghc_campaign_player_0 = var_2;
        }
        else
        {
            if ( isdefined( level.ghc_campaign_player_1 ) )
            {
                level.ghc_campaign_session_status = 5;
                return 0;
            }

            level.ghc_campaign_player_1 = var_2;
        }
    }

    if ( var_0 != 2 )
    {
        level.ghc_campaign_session_status = 6;
        return 0;
    }

    if ( !isdefined( level.ghc_campaign_player_0 )
        || !isdefined( level.ghc_campaign_player_1 ) )
    {
        level.ghc_campaign_session_status = 7;
        return 0;
    }

    /*
     * This is the actual Campaign-side reuse point:
     * preserve the native primary/secondary players and bind the shared
     * Campaign aliases to those exact engine-owned entities.
     */
    level.player = level.ghc_campaign_player_0;
    level.playengineeffects = level.ghc_campaign_player_1;
    level.ghc_campaign_player_0.unique_id = "player0";
    level.ghc_campaign_player_1.unique_id = "player1";

    ghc_campaign_event_lock_init();
    ghc_campaign_drill_state_init();

    /*
     * A new native secondary entity needs its own Clockwork loadout.
     * Do not carry the previous pair's one-time guard across a rebind.
     */
    level.ghc_campaign_clockwork_p2_loadout_applied = 0;

    level.ghc_campaign_session_ready = 1;
    level.ghc_campaign_session_status = 1;
    level notify( "ghc_campaign_players_ready" );
    return 1;
}

ghc_campaign_clear_session_aliases()
{
    /*
     * playerpiggyback is an original Campaign player array, not a private
     * co-op alias. Rebuild it from live player entities instead of clearing
     * it, so unrelated mission scripts can continue to enumerate players
     * after a secondary-client/session invalidation.
     */
    var_0 = getentarray( "player", "classname" );
    if ( !isdefined( var_0 ) )
        var_0 = [];

    level.playerpiggyback = var_0;

    var_1 = undefined;
    var_2 = undefined;

    if ( isdefined( level.ghc_campaign_player_0 ) )
    {
        for ( var_3 = 0; var_3 < var_0.size; var_3++ )
        {
            if ( isdefined( var_0[var_3] )
                && var_0[var_3] == level.ghc_campaign_player_0 )
            {
                var_1 = var_0[var_3];
                break;
            }
        }
    }

    if ( !isdefined( var_1 ) )
    {
        for ( var_3 = 0; var_3 < var_0.size; var_3++ )
        {
            if ( isdefined( var_0[var_3] ) )
            {
                var_2 = var_0[var_3];
                break;
            }
        }
    }

    if ( isdefined( var_1 ) )
        level.player = var_1;
    else if ( isdefined( var_2 ) )
        level.player = var_2;
    else
        level.player = undefined;

    level.playengineeffects = undefined;
    level.ghc_campaign_player_0 = undefined;
    level.ghc_campaign_player_1 = undefined;
}

ghc_campaign_invalidate_local_session()
{
    if ( isdefined( level.ghc_campaign_event_lock_initialized )
        && level.ghc_campaign_event_lock_initialized )
        ghc_campaign_event_lock_clear_active();

    ghc_campaign_drill_abort_slot( 0 );
    ghc_campaign_drill_abort_slot( 1 );
    ghc_campaign_clear_session_aliases();
    level.ghc_campaign_session_ready = 0;
    level.ghc_campaign_session_status = 8;
    return 0;
}

ghc_campaign_native_pair_is_current()
{
    if ( !issplitscreen()
        || !isdefined( level.ghc_campaign_player_0 )
        || !isdefined( level.ghc_campaign_player_1 )
        || !level.ghc_campaign_player_0 issplitscreenplayer()
        || !level.ghc_campaign_player_0 issplitscreenplayerprimary()
        || !level.ghc_campaign_player_1 issplitscreenplayer()
        || level.ghc_campaign_player_1 issplitscreenplayerprimary() )
        return 0;

    var_0 = getentarray( "player", "classname" );
    if ( !isdefined( var_0 ) )
        return 0;

    level.playerpiggyback = var_0;
    var_1 = 0;
    var_2 = 0;
    var_3 = 0;

    for ( var_4 = 0; var_4 < var_0.size; var_4++ )
    {
        var_5 = var_0[var_4];
        if ( !isdefined( var_5 ) || !var_5 issplitscreenplayer() )
            continue;

        var_1++;

        if ( var_5 == level.ghc_campaign_player_0 )
            var_2 = 1;
        else if ( var_5 == level.ghc_campaign_player_1 )
            var_3 = 1;
        else
            return 0;
    }

    if ( var_1 != 2 || !var_2 || !var_3 )
        return 0;

    return 1;
}

ghc_campaign_player_for_slot( var_0 )
{
    if ( !isdefined( level.ghc_campaign_session_ready )
        || !level.ghc_campaign_session_ready )
        return undefined;

    if ( var_0 == 0 )
        return level.ghc_campaign_player_0;

    if ( var_0 == 1 )
        return level.ghc_campaign_player_1;

    return undefined;
}

/*
 * Clockwork's original setup_combat() equips Logan with this exact kit.
 * Call this helper immediately after the original P1 setup_combat() hook.
 * P1's original function is left untouched; only the validated P2 entity
 * receives the matching campaign loadout.
 */
ghc_campaign_apply_clockwork_player_two_loadout()
{
    if ( !isdefined( level.ghc_campaign_session_ready )
        || !level.ghc_campaign_session_ready )
        return 0;

    if ( !ghc_campaign_native_pair_is_current() )
        return ghc_campaign_invalidate_local_session();

    if ( isdefined( level.ghc_campaign_clockwork_p2_loadout_applied )
        && level.ghc_campaign_clockwork_p2_loadout_applied )
        return 1;

    var_0 = ghc_campaign_player_for_slot( 1 );
    if ( !isdefined( var_0 )
        || !var_0 issplitscreenplayer()
        || var_0 issplitscreenplayerprimary() )
        return 0;

    var_0 setviewmodel( "viewhands_fed_army_arctic" );
    var_0 takeallweapons();
    var_0 giveweapon( "gm6+scopegm6_sp+silencer03_sp" );
    var_0 giveweapon( "cz805bren+reflex_sp+silencer_sp" );
    var_0 switchtoweaponimmediate( "cz805bren+reflex_sp+silencer_sp" );
    var_0 setoffhandprimaryclass( "frag" );
    var_0 setoffhandsecondaryclass( "flash" );

    level.ghc_campaign_clockwork_p2_loadout_applied = 1;
    return 1;
}

/*
 * Clockwork's first vertical slice has two drill spots. This ownership
 * state is shared by the mission, while weapon/input/animation/control
 * operations must always use the player argument passed by the caller.
 *
 * GSC script execution is serialized between waits. begin/finish contain
 * no wait/yield between checking and assigning ownership.
 */
ghc_campaign_drill_state_init()
{
    if ( isdefined( level.ghc_campaign_drill_state_initialized )
        && level.ghc_campaign_drill_state_initialized )
        return;

    level.ghc_campaign_drill_owner_0 = -1;
    level.ghc_campaign_drill_owner_1 = -1;
    level.ghc_campaign_drill_active_player_0 = -1;
    level.ghc_campaign_drill_active_player_1 = -1;
    level.ghc_campaign_drill_complete_0 = 0;
    level.ghc_campaign_drill_complete_1 = 0;
    level.ghc_campaign_drill_state_initialized = 1;
}

ghc_campaign_drill_player_slot( var_0 )
{
    if ( !isdefined( var_0 )
        || !isdefined( level.ghc_campaign_player_0 )
        || !isdefined( level.ghc_campaign_player_1 ) )
        return -1;

    if ( var_0 == level.ghc_campaign_player_0 )
        return 0;

    if ( var_0 == level.ghc_campaign_player_1 )
        return 1;

    return -1;
}

ghc_campaign_drill_owner_for_spot( var_0 )
{
    if ( !isdefined( level.ghc_campaign_drill_state_initialized )
        || !level.ghc_campaign_drill_state_initialized )
        return -1;

    if ( var_0 == 0 )
        return level.ghc_campaign_drill_owner_0;

    if ( var_0 == 1 )
        return level.ghc_campaign_drill_owner_1;

    return -1;
}

ghc_campaign_drill_is_complete( var_0 )
{
    if ( !isdefined( level.ghc_campaign_drill_state_initialized )
        || !level.ghc_campaign_drill_state_initialized )
        return 0;

    if ( var_0 == 0 )
        return level.ghc_campaign_drill_complete_0;

    if ( var_0 == 1 )
        return level.ghc_campaign_drill_complete_1;

    return 0;
}

ghc_campaign_drill_active_spot_for_player( var_0 )
{
    if ( var_0 == 0 )
        return level.ghc_campaign_drill_active_player_0;

    if ( var_0 == 1 )
        return level.ghc_campaign_drill_active_player_1;

    return -1;
}

ghc_campaign_drill_set_active_spot_for_player( var_0, var_1 )
{
    if ( var_0 == 0 )
    {
        level.ghc_campaign_drill_active_player_0 = var_1;
        return 1;
    }

    if ( var_0 == 1 )
    {
        level.ghc_campaign_drill_active_player_1 = var_1;
        return 1;
    }

    return 0;
}

ghc_campaign_drill_set_owner_for_spot( var_0, var_1 )
{
    if ( var_0 == 0 )
    {
        level.ghc_campaign_drill_owner_0 = var_1;
        return 1;
    }

    if ( var_0 == 1 )
    {
        level.ghc_campaign_drill_owner_1 = var_1;
        return 1;
    }

    return 0;
}

ghc_campaign_drill_set_complete_for_spot( var_0, var_1 )
{
    if ( var_0 == 0 )
    {
        level.ghc_campaign_drill_complete_0 = var_1;
        return 1;
    }

    if ( var_0 == 1 )
    {
        level.ghc_campaign_drill_complete_1 = var_1;
        return 1;
    }

    return 0;
}

ghc_campaign_drill_begin( var_0, var_1 )
{
    if ( !isdefined( level.ghc_campaign_session_ready )
        || !level.ghc_campaign_session_ready )
        return 0;

    if ( !ghc_campaign_native_pair_is_current() )
        return ghc_campaign_invalidate_local_session();

    var_2 = ghc_campaign_drill_player_slot( var_0 );
    if ( var_2 < 0 || var_1 < 0 || var_1 > 1 )
        return 0;

    if ( ghc_campaign_drill_active_spot_for_player( var_2 ) != -1 )
        return 0;

    /* The retail Clockwork flow reuses shared drill flags and one vault rig.
     * Its two drill spots are sequential stages, never concurrent sessions. */
    if ( ghc_campaign_drill_active_spot_for_player( 0 ) != -1
        || ghc_campaign_drill_active_spot_for_player( 1 ) != -1 )
        return 0;

    if ( var_1 == 1 && !ghc_campaign_drill_is_complete( 0 ) )
        return 0;

    if ( ghc_campaign_drill_is_complete( var_1 ) )
        return 0;

    if ( ghc_campaign_drill_owner_for_spot( var_1 ) != -1 )
        return 0;

    ghc_campaign_drill_set_owner_for_spot( var_1, var_2 );
    ghc_campaign_drill_set_active_spot_for_player( var_2, var_1 );
    return 1;
}

ghc_campaign_drill_finish( var_0, var_1, var_2 )
{
    if ( !isdefined( level.ghc_campaign_session_ready )
        || !level.ghc_campaign_session_ready )
        return 0;

    if ( !ghc_campaign_native_pair_is_current() )
        return ghc_campaign_invalidate_local_session();

    var_3 = ghc_campaign_drill_player_slot( var_0 );
    if ( var_3 < 0 || var_1 < 0 || var_1 > 1 )
        return 0;

    if ( ghc_campaign_drill_owner_for_spot( var_1 ) != var_3 )
        return 0;

    if ( ghc_campaign_drill_active_spot_for_player( var_3 ) != var_1 )
        return 0;

    if ( var_2 )
        ghc_campaign_drill_set_complete_for_spot( var_1, 1 );

    ghc_campaign_drill_set_owner_for_spot( var_1, -1 );
    ghc_campaign_drill_set_active_spot_for_player( var_3, -1 );
    return 1;
}

ghc_campaign_drill_complete( var_0, var_1 )
{
    return ghc_campaign_drill_finish( var_0, var_1, 1 );
}

ghc_campaign_drill_cancel( var_0, var_1 )
{
    return ghc_campaign_drill_finish( var_0, var_1, 0 );
}

ghc_campaign_drill_abort_slot( var_0 )
{
    if ( !isdefined( level.ghc_campaign_drill_state_initialized )
        || !level.ghc_campaign_drill_state_initialized )
        return 0;

    if ( var_0 < 0 || var_0 > 1 )
        return 0;

    var_1 = ghc_campaign_drill_active_spot_for_player( var_0 );
    if ( var_1 < 0 || var_1 > 1 )
        return 0;

    if ( ghc_campaign_drill_owner_for_spot( var_1 ) != var_0 )
        return 0;

    ghc_campaign_drill_set_owner_for_spot( var_1, -1 );
    ghc_campaign_drill_set_active_spot_for_player( var_0, -1 );
    return 1;
}

ghc_campaign_drill_abort_player( var_0 )
{
    var_1 = ghc_campaign_drill_player_slot( var_0 );
    if ( var_1 < 0 )
        return 0;

    return ghc_campaign_drill_abort_slot( var_1 );
}

/* Reusable single-owner lock for shared Campaign mission interactions. */
ghc_campaign_event_lock_init()
{
    if ( isdefined( level.ghc_campaign_event_lock_initialized )
        && level.ghc_campaign_event_lock_initialized )
        return 1;

    level.ghc_campaign_event_lock_owner = [];
    level.ghc_campaign_event_lock_active = [];
    level.ghc_campaign_event_lock_complete = [];

    for ( var_0 = 0; var_0 < 16; var_0++ )
    {
        level.ghc_campaign_event_lock_owner[var_0] = -1;
        level.ghc_campaign_event_lock_complete[var_0] = 0;
    }

    for ( var_1 = 0; var_1 < 2; var_1++ )
        level.ghc_campaign_event_lock_active[var_1] = -1;

    level.ghc_campaign_event_lock_initialized = 1;
    return 1;
}

ghc_campaign_event_lock_player_slot( player )
{
    if ( !isdefined( player )
        || !isdefined( level.ghc_campaign_player_0 )
        || !isdefined( level.ghc_campaign_player_1 ) )
        return -1;

    if ( player == level.ghc_campaign_player_0 )
        return 0;

    if ( player == level.ghc_campaign_player_1 )
        return 1;

    return -1;
}

ghc_campaign_event_lock_acquire( player, event_id )
{
    if ( !isdefined( level.ghc_campaign_session_ready )
        || !level.ghc_campaign_session_ready )
        return 0;

    if ( !ghc_campaign_native_pair_is_current() )
        return ghc_campaign_invalidate_local_session();

    if ( !isdefined( level.ghc_campaign_event_lock_initialized )
        || !level.ghc_campaign_event_lock_initialized )
        ghc_campaign_event_lock_init();

    var_0 = ghc_campaign_event_lock_player_slot( player );
    if ( var_0 < 0 || event_id < 0 || event_id > 15 )
        return 0;

    if ( level.ghc_campaign_event_lock_owner[event_id] != -1
        || level.ghc_campaign_event_lock_active[var_0] != -1
        || level.ghc_campaign_event_lock_complete[event_id] )
        return 0;

    /* Critical section: no scheduler yield between validation and ownership assignment. */
    level.ghc_campaign_event_lock_owner[event_id] = var_0;
    level.ghc_campaign_event_lock_active[var_0] = event_id;
    return 1;
}

ghc_campaign_event_lock_release( player, event_id, completed )
{
    if ( !isdefined( completed )
        || ( completed != 0 && completed != 1 ) )
        return 0;

    if ( !isdefined( level.ghc_campaign_event_lock_initialized )
        || !level.ghc_campaign_event_lock_initialized )
        return 0;

    if ( !isdefined( level.ghc_campaign_session_ready )
        || !level.ghc_campaign_session_ready )
        return 0;

    if ( !ghc_campaign_native_pair_is_current() )
        return ghc_campaign_invalidate_local_session();

    var_0 = ghc_campaign_event_lock_player_slot( player );
    if ( var_0 < 0 || event_id < 0 || event_id > 15 )
        return 0;

    if ( level.ghc_campaign_event_lock_owner[event_id] != var_0
        || level.ghc_campaign_event_lock_active[var_0] != event_id )
        return 0;

    if ( completed )
        level.ghc_campaign_event_lock_complete[event_id] = 1;

    level.ghc_campaign_event_lock_owner[event_id] = -1;
    level.ghc_campaign_event_lock_active[var_0] = -1;
    return 1;
}

ghc_campaign_event_lock_abort_slot( slot )
{
    if ( !isdefined( level.ghc_campaign_event_lock_initialized )
        || !level.ghc_campaign_event_lock_initialized )
        return 0;

    if ( slot < 0 || slot > 1 )
        return 0;

    var_0 = level.ghc_campaign_event_lock_active[slot];
    if ( var_0 < 0 || var_0 > 15 )
        return 0;

    if ( level.ghc_campaign_event_lock_owner[var_0] != slot )
        return 0;

    /* Abort releases ownership but never marks a shared event complete. */
    level.ghc_campaign_event_lock_owner[var_0] = -1;
    level.ghc_campaign_event_lock_active[slot] = -1;
    return 1;
}

ghc_campaign_event_lock_abort_player( player )
{
    var_0 = ghc_campaign_event_lock_player_slot( player );
    if ( var_0 < 0 )
        return 0;

    return ghc_campaign_event_lock_abort_slot( var_0 );
}

ghc_campaign_event_lock_clear_active()
{
    ghc_campaign_event_lock_abort_slot( 0 );
    ghc_campaign_event_lock_abort_slot( 1 );
    return 1;
}

ghc_campaign_event_lock_owner( event_id )
{
    if ( !isdefined( level.ghc_campaign_event_lock_initialized )
        || !level.ghc_campaign_event_lock_initialized
        || event_id < 0 || event_id > 15 )
        return -1;

    return level.ghc_campaign_event_lock_owner[event_id];
}

ghc_campaign_event_lock_active_for_player( slot )
{
    if ( !isdefined( level.ghc_campaign_event_lock_initialized )
        || !level.ghc_campaign_event_lock_initialized
        || slot < 0 || slot > 1 )
        return -1;

    return level.ghc_campaign_event_lock_active[slot];
}

ghc_campaign_event_lock_is_complete( event_id )
{
    if ( !isdefined( level.ghc_campaign_event_lock_initialized )
        || !level.ghc_campaign_event_lock_initialized
        || event_id < 0 || event_id > 15 )
        return 0;

    return level.ghc_campaign_event_lock_complete[event_id];
}

ghc_campaign_event_lock_is_owner( player, event_id )
{
    if ( !isdefined( level.ghc_campaign_event_lock_initialized )
        || !level.ghc_campaign_event_lock_initialized )
        return 0;

    var_0 = ghc_campaign_event_lock_player_slot( player );
    if ( var_0 < 0 || event_id < 0 || event_id > 15 )
        return 0;

    return level.ghc_campaign_event_lock_owner[event_id] == var_0
        && level.ghc_campaign_event_lock_active[var_0] == event_id;
}

ghc_campaign_event_lock_reset( event_id )
{
    if ( !isdefined( level.ghc_campaign_event_lock_initialized )
        || !level.ghc_campaign_event_lock_initialized
        || event_id < 0 || event_id > 15 )
        return 0;

    if ( level.ghc_campaign_event_lock_owner[event_id] != -1 )
        return 0;

    level.ghc_campaign_event_lock_complete[event_id] = 0;
    return 1;
}
