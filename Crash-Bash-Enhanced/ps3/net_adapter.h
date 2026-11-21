#ifndef CB_NET_ADAPTER_H
#define CB_NET_ADAPTER_H

#include "input_hook.h"

int cb_net_init(void);
int cb_net_host(void);
int cb_net_join_code(const char *code);
int cb_net_lobby_poll(void);
int cb_net_lobby_start(void);
void cb_net_leave(void);
int cb_net_is_active(void);
const char *cb_net_join_code_text(void);

/* Called from the first controller read of each emulated game frame. */
void cb_net_frame(void);

#endif
