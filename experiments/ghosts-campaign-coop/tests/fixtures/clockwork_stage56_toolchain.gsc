/*
 * Synthetic Stage 5/6 GSC Tool fixture.
 * This is not extracted retail source and is not a FastFile replacement.
 */

clockwork_play_vault_cinematics()
{
    cinematicingameloopresident( "clockwork_vault_scan" );
    cinematicingame( "clockwork_vault_drill" );
}

clockwork_get_secondary_player()
{
    if ( !issplitscreen() || !isdefined( level.playengineeffects ) )
        return undefined;

    secondary_player = level.playengineeffects;

    if ( !isdefined( secondary_player ) || !isplayer( secondary_player ) || !isalive( secondary_player ) || secondary_player == level.player || !secondary_player issplitscreenplayer() || secondary_player issplitscreenplayerprimary() )
        return undefined;

    return secondary_player;
}

clockwork_apply_vault_blast( player, blast_origin )
{
    if ( !isdefined( player ) || !isplayer( player ) || !isalive( player ) || !isdefined( blast_origin ) )
        return;

    blast_damage = player.health - 5;

    if ( blast_damage < 0 )
        blast_damage = 0;

    player dodamage( blast_damage, blast_origin );
    player shellshock( "default", 5 );
    player setstance( "prone" );
    player pushplayervector( ( 100, 0, 0 ), 1 );
    wait 0.2;

    if ( isdefined( player ) && isalive( player ) )
        player pushplayervector( ( 0, 0, 0 ), 1 );
}

new_trench_setup()
{
    if ( common_scripts\utility::flag( "vault_blast_area" ) )
    {
        var_3 = getent( "smoke_spot", "targetname" );
        secondary_player = clockwork_get_secondary_player();

        if ( isdefined( secondary_player ) )
            thread clockwork_apply_vault_blast( secondary_player, var_3.origin );

        clockwork_apply_vault_blast( level.player, var_3.origin );
    }

    level.player playrumbleonentity( "artillery_rumble" );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
        secondary_player playrumbleonentity( "artillery_rumble" );
}

setup_interior_cqb()
{
    level.player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
    {
        secondary_player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" );
        secondary_player setactionslot( 1, "nightvision" );
        secondary_player thread maps\_nightvision::nightvision_off();
        secondary_player nightvisiongogglesforceon();
    }
}

setup_interior()
{
    level.player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
        secondary_player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" );
}

vault_nag()
{
    common_scripts\utility::flag_wait( "at_vault_door" );
    level.player thread maps\_utility::display_hint_timeout( "disable_nvg", 5 );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
        secondary_player thread maps\_utility::display_hint_timeout( "disable_nvg", 5 );

    common_scripts\utility::flag_wait( "approaching_vault_door" );

    while ( level.player isswitchingweapon() || ( isdefined( secondary_player ) && secondary_player isswitchingweapon() ) )
    {
        wait 0.01;
        secondary_player = clockwork_get_secondary_player();
    }

    thread maps\clockwork_code::nvg_blackout_anims();
}

animate_vault_door()
{
    common_scripts\utility::flag_wait( "glow_start" );
    level.player thread maps\_utility::display_hint( "disable_nvg" );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
        secondary_player thread maps\_utility::display_hint( "disable_nvg" );

    common_scripts\utility::flag_wait( "thermite_start" );
    level.player setactionslot( 1, "" );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
        secondary_player setactionslot( 1, "" );

    thread maps\clockwork_code::nvg_blackout_anims();

    level.player setactionslot( 1, "" );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
        secondary_player setactionslot( 1, "" );

    level.player giveweapon( "fraggrenade" );
    level.player setweaponammoclip( "fraggrenade", level.grenades );
    level.player setweaponammostock( "fraggrenade", level.grenades );
    level.player setweaponammoclip( "flash_grenade", level.flashbangs );

    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( level.ghc_vault_player_two ) && isdefined( secondary_player ) && secondary_player == level.ghc_vault_player_two && isalive( secondary_player ) )
    {
        secondary_player giveweapon( "fraggrenade" );
        secondary_player setweaponammoclip( "fraggrenade", level.ghc_vault_player_two_grenades );
        secondary_player setweaponammostock( "fraggrenade", level.ghc_vault_player_two_grenades );
        secondary_player giveweapon( "flash_grenade" );
        secondary_player setweaponammoclip( "flash_grenade", level.ghc_vault_player_two_flashbangs );
        secondary_player setweaponammostock( "flash_grenade", level.ghc_vault_player_two_flashbangs );
    }

    level.ghc_vault_player_two = undefined;
}

vault_props()
{
    level.grenades = level.player getweaponammoclip( "fraggrenade" );
    level.flashbangs = level.player getweaponammoclip( "flash_grenade" );
    level.player takeweapon( "fraggrenade" );
    level.player takeweapon( "flash_grenade" );

    level.ghc_vault_player_two = undefined;
    level.ghc_vault_player_two_grenades = 0;
    level.ghc_vault_player_two_flashbangs = 0;
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
    {
        level.ghc_vault_player_two = secondary_player;
        level.ghc_vault_player_two_grenades = secondary_player getweaponammoclip( "fraggrenade" );
        level.ghc_vault_player_two_flashbangs = secondary_player getweaponammoclip( "flash_grenade" );
        secondary_player takeweapon( "fraggrenade" );
        secondary_player takeweapon( "flash_grenade" );
    }
}

handle_cqb_allies()
{
    level.player allowsprint( 1 );
    level.player allowjump( 1 );
    secondary_player = clockwork_get_secondary_player();

    if ( isdefined( secondary_player ) )
    {
        secondary_player allowsprint( 1 );
        secondary_player allowjump( 1 );
    }
}

cqb_dog()
{
    if ( isalive( level.override_crawl_death_anims ) )
    {
        level.player notify( "dog_attack_override" );
        secondary_player = clockwork_get_secondary_player();

        if ( isdefined( secondary_player ) )
            secondary_player notify( "dog_attack_override" );
    }
}
