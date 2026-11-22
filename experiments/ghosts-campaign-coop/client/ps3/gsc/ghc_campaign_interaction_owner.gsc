/*
 * Campaign interaction ownership helper for Clockwork-style co-op.
 * It consumes the validated native local-player pair from
 * ghc_campaign_local_session.gsc. It does not spawn players or patch memory.
 *
 * Status codes:
 * 0 = success
 * 1 = session not ready
 * 2 = player is not one of the validated local players
 * 3 = invalid interaction spot
 * 4 = spot has no active owner / player has no active interaction
 * 5 = caller is not the owner
 * 6 = spot is already owned
 * 7 = player already owns another interaction
 * 8 = spot is already complete
 */

main()
{
    level.ghc_campaign_interaction_ready = 0;

    for ( var_0 = 0; var_0 < 200; var_0++ )
    {
        if ( isdefined( level.ghc_campaign_session_ready )
            && level.ghc_campaign_session_ready )
            break;

        wait 0.05;
    }

    if ( !isdefined( level.ghc_campaign_session_ready )
        || !level.ghc_campaign_session_ready )
    {
        level.ghc_campaign_interaction_status = 1;
        return 0;
    }

    ghc_campaign_interaction_reset();
    level.ghc_campaign_interaction_ready = 1;
    level notify( "ghc_campaign_interactions_ready" );
    return 1;
}

ghc_campaign_interaction_reset()
{
    level.ghc_campaign_interaction_owner_by_spot = [];
    level.ghc_campaign_interaction_active_spot_by_player = [];
    level.ghc_campaign_interaction_completed_by_spot = [];

    for ( var_0 = 0; var_0 < 8; var_0++ )
    {
        level.ghc_campaign_interaction_owner_by_spot[var_0] = undefined;
        level.ghc_campaign_interaction_completed_by_spot[var_0] = undefined;
    }

    for ( var_1 = 0; var_1 < 2; var_1++ )
        level.ghc_campaign_interaction_active_spot_by_player[var_1] = undefined;

    level.ghc_campaign_interaction_status = 0;
}

ghc_campaign_interaction_slot_for_player( var_0 )
{
    if ( !isdefined( level.ghc_campaign_session_ready )
        || !level.ghc_campaign_session_ready )
        return 2;

    if ( !isdefined( var_0 ) )
        return 2;

    if ( var_0 == level.ghc_campaign_player_0 )
        return 0;

    if ( var_0 == level.ghc_campaign_player_1 )
        return 1;

    return 2;
}

ghc_campaign_interaction_begin( var_0, var_1 )
{
    if ( !isdefined( level.ghc_campaign_interaction_ready )
        || !level.ghc_campaign_interaction_ready )
    {
        level.ghc_campaign_interaction_status = 1;
        return 1;
    }

    var_2 = ghc_campaign_interaction_slot_for_player( var_0 );
    if ( var_2 > 1 )
    {
        level.ghc_campaign_interaction_status = 2;
        return 2;
    }

    if ( var_1 < 0 || var_1 >= 8 )
    {
        level.ghc_campaign_interaction_status = 3;
        return 3;
    }

    if ( isdefined( level.ghc_campaign_interaction_completed_by_spot[var_1] )
        && level.ghc_campaign_interaction_completed_by_spot[var_1] )
    {
        level.ghc_campaign_interaction_status = 8;
        return 8;
    }

    if ( isdefined( level.ghc_campaign_interaction_active_spot_by_player[var_2] ) )
    {
        level.ghc_campaign_interaction_status = 7;
        return 7;
    }

    if ( isdefined( level.ghc_campaign_interaction_owner_by_spot[var_1] ) )
    {
        level.ghc_campaign_interaction_status = 6;
        return 6;
    }

    level.ghc_campaign_interaction_owner_by_spot[var_1] = var_0;
    level.ghc_campaign_interaction_active_spot_by_player[var_2] = var_1;
    level.ghc_campaign_interaction_status = 0;
    return 0;
}

ghc_campaign_interaction_finish( var_0, var_1, var_2 )
{
    if ( !isdefined( level.ghc_campaign_interaction_ready )
        || !level.ghc_campaign_interaction_ready )
    {
        level.ghc_campaign_interaction_status = 1;
        return 1;
    }

    var_3 = ghc_campaign_interaction_slot_for_player( var_0 );
    if ( var_3 > 1 )
    {
        level.ghc_campaign_interaction_status = 2;
        return 2;
    }

    if ( var_1 < 0 || var_1 >= 8 )
    {
        level.ghc_campaign_interaction_status = 3;
        return 3;
    }

    if ( !isdefined( level.ghc_campaign_interaction_owner_by_spot[var_1] ) )
    {
        level.ghc_campaign_interaction_status = 4;
        return 4;
    }

    if ( level.ghc_campaign_interaction_owner_by_spot[var_1] != var_0
        || level.ghc_campaign_interaction_active_spot_by_player[var_3] != var_1 )
    {
        level.ghc_campaign_interaction_status = 5;
        return 5;
    }

    if ( var_2 )
        level.ghc_campaign_interaction_completed_by_spot[var_1] = 1;

    level.ghc_campaign_interaction_owner_by_spot[var_1] = undefined;
    level.ghc_campaign_interaction_active_spot_by_player[var_3] = undefined;
    level.ghc_campaign_interaction_status = 0;
    return 0;
}

ghc_campaign_interaction_complete( var_0, var_1 )
{
    return ghc_campaign_interaction_finish( var_0, var_1, 1 );
}

ghc_campaign_interaction_cancel( var_0, var_1 )
{
    return ghc_campaign_interaction_finish( var_0, var_1, 0 );
}

ghc_campaign_interaction_abort_player( var_0 )
{
    var_1 = ghc_campaign_interaction_slot_for_player( var_0 );
    if ( var_1 > 1 )
    {
        level.ghc_campaign_interaction_status = 2;
        return 2;
    }

    if ( !isdefined( level.ghc_campaign_interaction_active_spot_by_player[var_1] ) )
    {
        level.ghc_campaign_interaction_status = 4;
        return 4;
    }

    return ghc_campaign_interaction_cancel(
        var_0,
        level.ghc_campaign_interaction_active_spot_by_player[var_1]
    );
}

ghc_campaign_interaction_is_complete( var_0 )
{
    if ( var_0 < 0 || var_0 >= 8 )
        return 0;

    if ( !isdefined( level.ghc_campaign_interaction_completed_by_spot[var_0] ) )
        return 0;

    return level.ghc_campaign_interaction_completed_by_spot[var_0];
}

ghc_campaign_interaction_owner_for_spot( var_0 )
{
    if ( var_0 < 0 || var_0 >= 8 )
        return undefined;

    return level.ghc_campaign_interaction_owner_by_spot[var_0];
}
