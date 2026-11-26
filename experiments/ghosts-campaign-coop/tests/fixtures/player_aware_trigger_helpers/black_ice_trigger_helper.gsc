waittill_trigger( var_0, var_1, var_2, var_3, var_4, var_5, var_6 )
{
    waittill_trigger_for_player( level.player, var_0, var_1, var_2, var_3, var_4, var_5, var_6 );
}

waittill_trigger_for_player( player, var_0, var_1, var_2, var_3, var_4, var_5, var_6 )
{
    if ( player ismeleeing() || player isthrowinggrenade() || !player isonground() )
        var_0 common_scripts\\utility::trigger_nobloodpool();
    else if ( player usebuttonpressed() )
        var_0 notify( "valid_trigger" );
    wait 0.05;
}
