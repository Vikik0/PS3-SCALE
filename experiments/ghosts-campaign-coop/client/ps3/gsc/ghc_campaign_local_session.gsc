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
        return 1;

    if ( isdefined( level.ghc_campaign_drill_state_initialized )
        && level.ghc_campaign_drill_state_initialized )
    {
        ghc_campaign_drill_abort_slot( 0 );
        ghc_campaign_drill_abort_slot( 1 );
    }

    level.ghc_campaign_session_ready = 0;
    level.ghc_campaign_session_status = 0;
    level.ghc_campaign_player_0 = undefined;
    level.ghc_campaign_player_1 = undefined;
    level.playerpiggyback = undefined;

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
                return 1;
        }

        wait 0.05;
    }

    var_1 = getentarray( "player", "classname" );
    if ( isdefined( var_1 ) )
        level.playerpiggyback = var_1;

    return ghc_campaign_bind_native_local_pair();
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
     * 7 = one of the two native identities is missing.
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

    ghc_campaign_drill_state_init();

    level.ghc_campaign_session_ready = 1;
    level.ghc_campaign_session_status = 1;
    level notify( "ghc_campaign_players_ready" );
    return 1;
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
        || !level.ghc_campaign_session_ready
        || !ghc_campaign_native_pair_is_current() )
        return 0;

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
    {
        level.ghc_campaign_session_ready = 0;
        ghc_campaign_drill_abort_slot( 0 );
        ghc_campaign_drill_abort_slot( 1 );
        return 0;
    }

    var_2 = ghc_campaign_drill_player_slot( var_0 );
    if ( var_2 < 0 || var_1 < 0 || var_1 > 1 )
        return 0;

    if ( ghc_campaign_drill_active_spot_for_player( var_2 ) != -1 )
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
        || !level.ghc_campaign_session_ready
        || !ghc_campaign_native_pair_is_current() )
    {
        level.ghc_campaign_session_ready = 0;
        ghc_campaign_drill_abort_slot( 0 );
        ghc_campaign_drill_abort_slot( 1 );
        return 0;
    }

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

