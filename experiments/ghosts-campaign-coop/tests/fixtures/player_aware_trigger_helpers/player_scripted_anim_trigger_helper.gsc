waittill_trigger( var_0, var_1, var_2, var_3, var_4, var_5 )
{
    waittill_trigger_for_player( level.player, var_0, var_1, var_2, var_3, var_4, var_5 );
}

waittill_trigger_for_player( player, var_0, var_1, var_2, var_3, var_4, var_5 )
{
    var_7 = isdefined( var_4 ) && var_4;
    var_0 thread _trigger_handle_triggering( player, var_7 );
    if ( player getstance() == "prone" )
        var_0 common_scripts\utility::trigger_nobloodpool();
    wait 0.1;
}

_trigger_handle_triggering( player, var_0 )
{
    self waittill( "trigger", var_7 );
    if ( !isdefined( var_7 ) || var_7 != player )
        continue;
    if ( !player ismeleeing() )
        self notify( "valid_trigger" );
}
