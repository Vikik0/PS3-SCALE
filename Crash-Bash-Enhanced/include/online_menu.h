#ifndef CB_ONLINE_MENU_H
#define CB_ONLINE_MENU_H
#include <stddef.h>
int cb_online_menu(const char *guest_name, int *network_state, int *online_state, volatile int *exit_requested, char *status, size_t status_size);
#endif
