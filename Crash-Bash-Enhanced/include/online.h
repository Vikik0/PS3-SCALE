#ifndef CB_ONLINE_H
#define CB_ONLINE_H
#include <stddef.h>
#define CB_ONLINE_ROOM_MAX 8
#define CB_ONLINE_PLAYER_MAX 2
#define CB_ONLINE_NAME_MAX 21
#define CB_ONLINE_TOKEN_MAX 2048
#define CB_ONLINE_ID_MAX 40
#ifndef CB_ONLINE_API_BASE
#define CB_ONLINE_API_BASE "https://crash-bash-enhanced-git-crash-bash-enhanced-pcidea00-7781.vercel.app"
#endif
typedef struct { char code[7]; char host[CB_ONLINE_NAME_MAX]; int players; int max_players; } CBOnlineRoom;
typedef struct { char id[CB_ONLINE_ID_MAX]; char name[CB_ONLINE_NAME_MAX]; int ready; } CBOnlinePlayer;
typedef struct { char code[7]; char token[CB_ONLINE_TOKEN_MAX]; char player_id[CB_ONLINE_ID_MAX]; char player_name[CB_ONLINE_NAME_MAX]; int ready; } CBOnlineSession;
typedef struct { char code[7]; int is_private; int is_ready; int player_count; CBOnlinePlayer players[CB_ONLINE_PLAYER_MAX]; } CBOnlineRoomView;
int cb_online_init(void);
void cb_online_shutdown(void);
int cb_online_health(char *error, size_t error_size);
int cb_online_list_rooms(CBOnlineRoom *rooms, size_t capacity, size_t *count, char *error, size_t error_size);
int cb_online_create_room(const char *display_name, int is_private, CBOnlineSession *session, CBOnlineRoomView *view, char *error, size_t error_size);
int cb_online_join_room(const char *code, const char *display_name, CBOnlineSession *session, CBOnlineRoomView *view, char *error, size_t error_size);
int cb_online_get_room(CBOnlineSession *session, CBOnlineRoomView *view, char *error, size_t error_size);
int cb_online_set_ready(CBOnlineSession *session, int ready, CBOnlineRoomView *view, char *error, size_t error_size);
int cb_online_leave_room(CBOnlineSession *session, char *error, size_t error_size);
#endif
