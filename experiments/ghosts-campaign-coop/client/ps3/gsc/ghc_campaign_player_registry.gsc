/*
 * Read-only Campaign local-player registry shim for IW6 / PS3.
 *
 * It never creates, clones, respawns, or moves a player. It only validates
 * native local-player objects already registered by the engine loader.
 * Runtime loading/injection is not yet implemented.
 */

main()
{
    if ( !isdefined( level.playerpiggyback ) )
        level waittill( "level.players initialized" );

    return ghc_campaign_player_registry_init();
}

ghc_campaign_player_registry_init()
{
    level.ghc_campaign_coop_ready = 0;
    level.ghc_campaign_registry_status = 0;
    level.ghc_campaign_player_count = 0;
    level.ghc_campaign_player_0 = undefined;
    level.ghc_campaign_player_1 = undefined;

    /*
     * Status:
     * 0 = not checked, 1 = ready, 2 = player array missing,
     * 3 = split-screen inactive, 4 = duplicate primary,
     * 5 = duplicate secondary, 6 = player count is not exactly two.
     */
    if ( !isdefined( level.playerpiggyback ) )
    {
        level.ghc_campaign_registry_status = 2;
        return 0;
    }

    if ( !issplitscreen() )
    {
        level.ghc_campaign_registry_status = 3;
        return 0;
    }

    for ( var_0 = 0; var_0 < level.playerpiggyback.size; var_0++ )
    {
        var_1 = level.playerpiggyback[var_0];

        if ( !isdefined( var_1 ) || !var_1 issplitscreenplayer() )
            continue;

        level.ghc_campaign_player_count++;

        if ( var_1 issplitscreenplayerprimary() )
        {
            if ( isdefined( level.ghc_campaign_player_0 ) )
            {
                level.ghc_campaign_registry_status = 4;
                return 0;
            }

            level.ghc_campaign_player_0 = var_1;
        }
        else
        {
            if ( isdefined( level.ghc_campaign_player_1 ) )
            {
                level.ghc_campaign_registry_status = 5;
                return 0;
            }

            level.ghc_campaign_player_1 = var_1;
        }
    }

    if ( level.ghc_campaign_player_count != 2 )
    {
        level.ghc_campaign_registry_status = 6;
        return 0;
    }

    if ( !isdefined( level.ghc_campaign_player_0 )
        || !isdefined( level.ghc_campaign_player_1 ) )
    {
        level.ghc_campaign_registry_status = 6;
        return 0;
    }

    level.ghc_campaign_coop_ready = 1;
    level.ghc_campaign_registry_status = 1;
    level notify( "ghc_campaign_players_ready" );
    return 1;
}

ghc_campaign_player_for_slot( var_0 )
{
    if ( !isdefined( level.ghc_campaign_coop_ready ) || !level.ghc_campaign_coop_ready )
        return undefined;

    if ( var_0 == 0 )
        return level.ghc_campaign_player_0;

    if ( var_0 == 1 )
        return level.ghc_campaign_player_1;

    return undefined;
}
