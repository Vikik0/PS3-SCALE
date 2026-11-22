clockwork_pick_enemy_target( ai )
{
    if ( !isdefined( ai ) || !isalive( ai ) )
        return undefined;

    target_player = undefined;

    if ( isdefined( level.player ) && isplayer( level.player ) && isalive( level.player ) )
        target_player = level.player;

    if ( isdefined( level.playengineeffects ) )
    {
        secondary_player = level.playengineeffects;

        if ( isdefined( secondary_player ) && isplayer( secondary_player ) && isalive( secondary_player ) && secondary_player != target_player )
        {
            if ( !isdefined( target_player ) )
                target_player = secondary_player;
            else if ( distance( ai.origin, secondary_player.origin ) < distance( ai.origin, target_player.origin ) )
                target_player = secondary_player;
        }
    }

    return target_player;
}

discovery_guys_wakeup()
{
    foreach ( var_1 in level.discovery_guys )
    {
        var_2 = clockwork_pick_enemy_target( var_1 );
        if ( isdefined( var_2 ) && isplayer( var_2 ) && isalive( var_2 ) )
            var_1 getenemyinfo( var_2 );
    }
}
