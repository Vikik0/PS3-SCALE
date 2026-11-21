#ifndef CB_INPUT_H
#define CB_INPUT_H

#include <stdint.h>

#include <cell/pad.h>

/* The PS1 pad state in PS3 libpad's native report layout. */
typedef struct {
    uint16_t digital1;
    uint16_t digital2;
    uint8_t right_x;
    uint8_t right_y;
    uint8_t left_x;
    uint8_t left_y;
} CbPadState;

/* Install/remove process-local cellPad API hooks. */
int cb_input_install(void);
void cb_input_remove(void);

/* Enable the four-player input bridge. local_slot is 0..3.
 * The physical controller on PS3 port 0 is routed to that slot.
 */
void cb_input_set_active(int active, int local_slot);

/* Update one remote PS1 player slot. The local slot is ignored. */
void cb_input_set_remote_state(int slot, const CbPadState *state);

#endif
