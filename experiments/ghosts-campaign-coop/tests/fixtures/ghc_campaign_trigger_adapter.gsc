/*
 * Compile-only player-aware trigger adapter fixture for IW6 / PS3.
 *
 * This is a reference pattern, not a retail mission patch and not a PS3
 * payload. A single mission coordinator should call it for only the intended
 * shared interaction, then pass the returned player entity through every
 * player-specific handler. If it returns undefined while the session remains
 * ready, the caller may wait for the next trigger event.
 */

main()
{
    return 1;
}

ghc_campaign_wait_for_owned_trigger( trigger_ent, event_id )
{
    if ( !isdefined( trigger_ent )
        || !isdefined( level.ghc_campaign_session_ready )
        || !level.ghc_campaign_session_ready
        || event_id < 0 || event_id > 15 )
        return undefined;

    if ( !ghc_campaign_native_pair_is_current() )
    {
        ghc_campaign_invalidate_local_session();
        return undefined;
    }

    /* IW6 trigger event supplies the actual activating player as its argument. */
    trigger_ent waittill( "trigger", activator );

    if ( !isdefined( activator ) )
        return undefined;

    /* The pair may have changed while this function was waiting on the trigger. */
    if ( !ghc_campaign_native_pair_is_current() )
    {
        ghc_campaign_invalidate_local_session();
        return undefined;
    }

    if ( ghc_campaign_event_lock_player_slot( activator ) < 0 )
        return undefined;

    if ( !ghc_campaign_event_lock_acquire( activator, event_id ) )
        return undefined;

    return activator;
}

ghc_campaign_release_owned_trigger( player, event_id, completed )
{
    if ( !isdefined( player ) )
        return 0;

    return ghc_campaign_event_lock_release( player, event_id, completed );
}

ghc_campaign_abort_owned_trigger( player )
{
    if ( !isdefined( player ) )
        return 0;

    return ghc_campaign_event_lock_abort_player( player );
}


/*
 * Local signatures used only so GSC Tool can compile this isolated fixture.
 * The retail bridge owns the real implementations; these stubs are never
 * copied into a PS3 payload or loaded beside the real bridge.
 */
ghc_campaign_native_pair_is_current()
{
    return 1;
}

ghc_campaign_invalidate_local_session()
{
    return 0;
}

ghc_campaign_event_lock_player_slot( player )
{
    return 0;
}

ghc_campaign_event_lock_acquire( player, event_id )
{
    return 1;
}

ghc_campaign_event_lock_release( player, event_id, completed )
{
    return 1;
}

ghc_campaign_event_lock_abort_player( player )
{
    return 1;
}
