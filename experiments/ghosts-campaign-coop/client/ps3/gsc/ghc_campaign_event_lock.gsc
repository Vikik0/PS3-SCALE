/*
 * Reusable single-owner lock for shared Campaign mission interactions.
 *
 * Event IDs 0..15 are mission-local identifiers. The same event ID must be
 * used by every player/script that can trigger the same shared objective.
 * Different event IDs may be owned concurrently by different players.
 *
 * This helper never creates players, polls controllers, writes game memory,
 * or starts a mission event by itself. Callers own the actual interaction.
 */

main()
{
    return ghc_campaign_event_lock_init();
}

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

    /* No wait/yield between validation and ownership assignment. */
    level.ghc_campaign_event_lock_owner[event_id] = var_0;
    level.ghc_campaign_event_lock_active[var_0] = event_id;
    return 1;
}

ghc_campaign_event_lock_release( player, event_id, completed )
{
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
