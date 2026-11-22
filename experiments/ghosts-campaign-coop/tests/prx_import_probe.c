#include <stddef.h>
#include <stdint.h>
#include <sys/prx_module.h>

/* Import-only smoke test: this PRX is built, never installed or run. */
extern int32_t ghc_local_input_refresh(void);
extern uint32_t ghc_local_input_connected_mask(void);
extern uint32_t ghc_local_input_valid_mask(void);
extern uint32_t ghc_local_input_player_buttons(uint32_t player_slot);
extern uint32_t ghc_local_input_player_pressed_buttons(uint32_t player_slot);
extern uint32_t ghc_local_input_player_released_buttons(uint32_t player_slot);
extern uint32_t ghc_local_input_player_left_stick_normalized(uint32_t player_slot);

int module_start(size_t args, void *argp)
{
    (void)args;
    (void)argp;
    (void)ghc_local_input_refresh();
    (void)ghc_local_input_connected_mask();
    (void)ghc_local_input_valid_mask();
    (void)ghc_local_input_player_buttons(1u);
    (void)ghc_local_input_player_buttons(2u);
    (void)ghc_local_input_player_pressed_buttons(1u);
    (void)ghc_local_input_player_released_buttons(2u);
    (void)ghc_local_input_player_left_stick_normalized(1u);
    return SYS_PRX_RESIDENT;
}
