
new_trench_setup()
{
    if ( common_scripts\utility::flag( "vault_blast_area" ) )
    {
        var_3 = getent( "smoke_spot", "targetname" );
        level.player dodamage( level.player.health - 5, var_3.origin );
        level.player shellshock( "default", 5 );
        level.player setstance( "prone" );
        level.player pushplayervector( ( 100, 0, 0 ), 1 );
        wait 0.2;
        level.player pushplayervector( ( 0, 0, 0 ), 1 );
    }

    level.player playrumbleonentity( "artillery_rumble" );
}

setup_interior_cqb()
{
    level.player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" );
    level.player setactionslot( 1, "nightvision" );
    level.player thread maps\_nightvision::nightvision_off();
    level.player nightvisiongogglesforceon();
}

setup_interior()
{
    level.player switchtoweapon( "cz805bren+reflex_sp+silencer_sp" );
}

vault_nag()
{
    common_scripts\utility::flag_wait( "at_vault_door" );
    level.player thread maps\_utility::display_hint_timeout( "disable_nvg", 5 );
    common_scripts\utility::flag_wait( "approaching_vault_door" );

    while ( level.player isswitchingweapon() )
        wait 0.01;

    thread maps\clockwork_code::nvg_blackout_anims();
}

animate_vault_door()
{
    common_scripts\utility::flag_wait( "glow_start" );
    level.player thread maps\_utility::display_hint( "disable_nvg" );
    common_scripts\utility::flag_wait( "thermite_start" );
    level.player setactionslot( 1, "" );
    thread maps\clockwork_code::nvg_blackout_anims();
    level.player setactionslot( 1, "" );
    level.player giveweapon( "fraggrenade" );
    level.player setweaponammoclip( "fraggrenade", level.grenades );
    level.player setweaponammostock( "fraggrenade", level.grenades );
    level.player setweaponammoclip( "flash_grenade", level.flashbangs );
}

vault_props()
{
    level.grenades = level.player getweaponammoclip( "fraggrenade" );
    level.flashbangs = level.player getweaponammoclip( "flash_grenade" );
    level.player takeweapon( "fraggrenade" );
    level.player takeweapon( "flash_grenade" );
}

handle_cqb_allies()
{
    level.player allowsprint( 1 );
    level.player allowjump( 1 );
}

cqb_dog()
{
    if ( isalive( level.override_crawl_death_anims ) )
    {
        level.player notify( "dog_attack_override" );
    }
}
