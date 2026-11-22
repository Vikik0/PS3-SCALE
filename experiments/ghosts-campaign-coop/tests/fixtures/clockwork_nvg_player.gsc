player_lever_input()
{
    if ( level.x_turretcheck || level.ps3 )
    {
        if ( isdefined( level.player ) && isalive( level.player ) )
            thread player_nvg_light_fx( level.player );

        if ( isdefined( level.playengineeffects ) && isplayer( level.playengineeffects ) && isalive( level.playengineeffects ) && level.playengineeffects != level.player )
            thread player_nvg_light_fx( level.playengineeffects );

        if ( !isdefined( level.ghc_nvg_secondary_light_watcher_active ) || !level.ghc_nvg_secondary_light_watcher_active )
        {
            level.ghc_nvg_secondary_light_watcher_active = 1;
            thread watch_secondary_nvg_player();
        }
    }
}

watch_secondary_nvg_player()
{
    while ( !common_scripts\utility::flag( "lights_on" ) )
    {
        if ( isdefined( level.playengineeffects ) )
        {
            var_0 = level.playengineeffects;

            if ( isdefined( var_0 ) && isplayer( var_0 ) && isalive( var_0 ) && var_0 != level.player )
            {
                if ( !isdefined( var_0.ghc_nvg_light_active ) || !var_0.ghc_nvg_light_active )
                    thread player_nvg_light_fx( var_0 );
            }
        }

        wait 0.1;
    }

    level.ghc_nvg_secondary_light_watcher_active = undefined;
}

player_nvg_light_fx( player )
{
    if ( !isdefined( player ) || !isalive( player ) )
        return;

    if ( isdefined( player.ghc_nvg_light_active ) && player.ghc_nvg_light_active )
        return;

    player.ghc_nvg_light_active = 1;
    player.ghc_nvg_light_on = !isdefined( player.nightvision_effectsoff );
    var_0 = common_scripts\utility::spawn_tag_fx();
    playfxontag( level._effect["player_nvg_light"], var_0, "tag_origin" );
    thread keep_player_below_surface( player );

    while ( !common_scripts\utility::flag( "lights_on" ) && isdefined( player ) && isalive( player ) )
    {
        if ( isdefined( player.ghc_nvg_light_on ) && player.ghc_nvg_light_on )
        {
            var_0.angles = player getplayerangles();
            var_0.origin = player.origin + ( 0, 0, 64 );
        }
        else
            var_0.origin = ( 0, 0, 0 );

        wait 0.01;
    }

    if ( isdefined( var_0 ) )
        var_0 delete();

    if ( isdefined( player ) )
    {
        player.ghc_nvg_light_active = undefined;
        player.ghc_nvg_light_on = undefined;
    }
}

keep_player_below_surface( player )
{
    if ( !isdefined( player ) || !isalive( player ) )
        return;

    player notifyonplayercommand( "nvg_toggle", "+actionslot 1" );

    while ( !common_scripts\utility::flag( "lights_on" ) && isdefined( player ) && isalive( player ) )
    {
        player.ghc_nvg_light_on = !isdefined( player.nightvision_effectsoff );
        wait 0.05;
    }
}
