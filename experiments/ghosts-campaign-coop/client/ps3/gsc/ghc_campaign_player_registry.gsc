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
    level.ghc_campaign_player_count = 0;
    level.ghc_campaign_player_0 = undefined;
    level.ghc_campaign_player_1 = undefined;

    if ( !isdefined( level.playerpiggyback ) )
        return 0;

    level.ghc_campaign_player_count = level.playerpiggyback.size;

    if ( !issplitscreen() || level.ghc_campaign_player_count < 2 )
        return 0;

    level.ghc_campaign_player_0 = level.playerpiggyback[0];
    level.ghc_campaign_player_1 = level.playerpiggyback[1];

    if ( !isdefined( level.ghc_campaign_player_0 ) || !isdefined( level.ghc_campaign_player_1 ) )
        return 0;

    if ( !level.ghc_campaign_player_0 issplitscreenplayer() )
        return 0;

    if ( !level.ghc_campaign_player_1 issplitscreenplayer() )
        return 0;

    if ( !level.ghc_campaign_player_0 issplitscreenplayerprimary() )
        return 0;

    if ( level.ghc_campaign_player_1 issplitscreenplayerprimary() )
        return 0;

    level.ghc_campaign_coop_ready = 1;
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
