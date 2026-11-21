#ifndef CB_NET_ADAPTER_H
#define CB_NET_ADAPTER_H

#include "input_hook.h"

int cb_net_init(void);
int cb_net_host(void);
int cb_net_join_code(const char *code);
/* Relay-backed room creation/join; currently plain HTTP/ws prototype transport. */
int cb_net_relay_host(const char *worker_host, unsigned short worker_port);
int cb_net_relay_join_code(const char *worker_host, unsigned short worker_port, const char *code);
int cb_net_relay_start(void);
int cb_net_lobby_poll(void);
int cb_net_lobby_start(void);
void cb_net_leave(void);
int cb_net_is_active(void);
const char *cb_net_join_code_text(void);

/* Called from the first controller read of each emulated game frame. */
void cb_net_frame(void);

#endif
