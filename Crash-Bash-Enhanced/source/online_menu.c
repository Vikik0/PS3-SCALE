#include "online_menu.h"
#include "online.h"
#include <stdio.h>
#include <string.h>
#include <tiny3d.h>
#include <libfont.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>
#include <sys/time.h>
struct timeval;
#include <net/net.h>

#define MENU_ROOM_LIMIT 8
#define ROOM_HEARTBEAT_FRAMES 300
#define ROOM_LIST_REFRESH_FRAMES 600
#define SCREEN_ROOMS 0
#define SCREEN_ROOM 1
#define SCREEN_JOIN_CODE 2

static const char g_room_code_chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

static void draw_rect(float x, float y, float w, float h, u32 color)
{
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(x, y, 65535.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexPos(x + w, y, 65535.0f);
    tiny3d_VertexPos(x + w, y + h, 65535.0f);
    tiny3d_VertexPos(x, y + h, 65535.0f);
    tiny3d_End();
}

static void draw_text(float x, float y, int size, u32 color, const char *value)
{
    char safe[256];
    SetCurrentFont(0);
    SetFontColor(color, 0x00000000);
    SetFontSize(size, size + 8);
    snprintf(safe, sizeof(safe), "%s", value ? value : "");
    DrawString(x, y, safe);
}

static int ensure_lobby(int *network_state, int *online_state, char *status, size_t size)
{
    int rc;
    char error[160] = "";
    if (!network_state || !online_state) return -1;
    if (*network_state != 1) {
        rc = netInitialize();
        if (rc != 0) {
            *network_state = -1;
            *online_state = -1;
            snprintf(status, size, "Network initialization failed: 0x%08X", (unsigned int)rc);
            return -1;
        }
        *network_state = 1;
    }
    if (cb_online_init() != 0) {
        *online_state = -1;
        snprintf(status, size, "Could not initialize the HTTPS client.");
        return -1;
    }
    if (cb_online_health(error, sizeof(error)) != 0) {
        *online_state = -1;
        snprintf(status, size, "Lobby service: %.120s", error);
        return -1;
    }
    *online_state = 1;
    snprintf(status, size, "Cloud lobby connected. PSN login is not used.");
    return 0;
}

static int load_rooms(CBOnlineRoom *rooms, int *count, int *selected, char *status, size_t size)
{
    size_t n = 0;
    char error[160] = "";
    if (cb_online_list_rooms(rooms, MENU_ROOM_LIMIT, &n, error, sizeof(error)) != 0) {
        *count = 0;
        snprintf(status, size, "Room list: %.120s", error);
        return -1;
    }
    *count = (int)n;
    if (*count == 0) *selected = 0;
    else if (*selected >= *count) *selected = *count - 1;
    snprintf(status, size, "Found %d public room(s).", *count);
    return 0;
}

static int session_is_gone(const char *error)
{
    if (!error) return 0;
    return strstr(error, "expired") != NULL ||
           strstr(error, "no longer") != NULL ||
           strstr(error, "not a member") != NULL ||
           strstr(error, "not found") != NULL;
}

static void return_to_rooms(CBOnlineSession *session, CBOnlineRoomView *view,
                            int *ready, int *screen, unsigned int *poll_frames,
                            CBOnlineRoom *rooms, int *room_count, int *selected,
                            char *status, size_t status_size)
{
    char ignored[160] = "";
    if (session && session->token[0]) cb_online_leave_room(session, ignored, sizeof(ignored));
    if (session) memset(session, 0, sizeof(*session));
    if (view) memset(view, 0, sizeof(*view));
    if (ready) *ready = 0;
    if (screen) *screen = SCREEN_ROOMS;
    if (poll_frames) *poll_frames = 0;
    load_rooms(rooms, room_count, selected, status, status_size);
    snprintf(status, status_size, "Your room ended or your seat expired. Choose another battle.");
}

static void draw_rooms(const CBOnlineRoom *rooms, int count, int selected, int private_create, const char *status)
{
    int i;
    char mode[64];
    tiny3d_Clear(0x071019ff, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();
    draw_rect(24, 22, 800, 468, 0x101e2aff);
    draw_rect(24, 22, 800, 5, 0x46d6b2ff);
    draw_text(56, 44, 27, 0xf5fbffff, "ONLINE ROOMS");
    snprintf(mode, sizeof(mode), "CREATE MODE: %s  |  R1: JOIN WITH CODE  |  NO PSN", private_create ? "PRIVATE" : "PUBLIC");
    draw_text(56, 82, 14, 0x9bb4c2ff, mode);
    if (count == 0) {
        draw_rect(58, 142, 720, 120, 0x0b1720ff);
        draw_text(82, 176, 18, 0xc2d0d8ff, "No public rooms are waiting.");
        draw_text(82, 214, 15, 0x61d9baff, "Triangle creates a room. L1 switches its privacy.");
    } else {
        for (i = 0; i < count; i++) {
            char row[96];
            u32 bg = i == selected ? 0x28594fff : 0x172a36ff;
            u32 fg = i == selected ? 0xffffffff : 0xc2d0d8ff;
            draw_rect(58, 130 + i * 38, 720, 31, bg);
            snprintf(row, sizeof(row), "%.6s   %-20.20s   %d/%d PLAYERS", rooms[i].code, rooms[i].host, rooms[i].players, rooms[i].max_players);
            draw_text(76, 136 + i * 38, 15, fg, row);
        }
    }
    draw_rect(58, 438, 720, 32, 0x0b1720ff);
    draw_text(72, 444, 13, 0x9bb4c2ff, status);
    draw_text(56, 474, 12, 0x6d8794ff, "UP/DOWN SELECT  L2 ANY BATTLE  CROSS JOIN  TRIANGLE CREATE  L1 PRIVACY  R1 CODE  SQUARE REFRESH");
    tiny3d_Flip();
}

static void draw_join_code(const char *code, int cursor, const char *status)
{
    int i;
    tiny3d_Clear(0x071019ff, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();
    draw_rect(24, 22, 800, 468, 0x101e2aff);
    draw_rect(24, 22, 800, 5, 0x46d6b2ff);
    draw_text(56, 44, 27, 0xf5fbffff, "JOIN PRIVATE ROOM");
    draw_text(56, 94, 15, 0x9bb4c2ff, "Enter the six-character invite code shared by the room host.");
    for (i = 0; i < 6; i++) {
        char letter[2];
        float x = 250.0f + (float)i * 58.0f;
        letter[0] = code[i];
        letter[1] = '\0';
        draw_rect(x, 180, 48, 62, i == cursor ? 0x28594fff : 0x172a36ff);
        draw_text(x + 13, 188, 34, 0xffffffff, letter);
    }
    draw_rect(58, 354, 720, 52, 0x0b1720ff);
    draw_text(72, 365, 15, 0x9bb4c2ff, status);
    draw_text(56, 474, 12, 0x6d8794ff, "LEFT/RIGHT MOVE  UP/DOWN CHANGE LETTER  CROSS JOIN  CIRCLE BACK");
    tiny3d_Flip();
}

static void draw_room(const CBOnlineRoomView *view, const CBOnlineSession *session, const char *status)
{
    int i;
    tiny3d_Clear(0x071019ff, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();
    draw_rect(24, 22, 800, 468, 0x101e2aff);
    draw_rect(24, 22, 800, 5, 0x46d6b2ff);
    draw_text(56, 44, 26, 0xf5fbffff, "BATTLE ROOM");
    draw_text(56, 86, 16, 0x61d9baff, view->is_private ? "PRIVATE BATTLE CODE" : "PUBLIC BATTLE CODE");
    draw_text(330, 78, 30, 0xf5fbffff, session->code);
    for (i = 0; i < CB_ONLINE_PLAYER_MAX; i++) {
        char slot[80];
        int col = i % 2;
        int row = i / 2;
        int x = 58 + col * 365;
        int y = 140 + row * 88;
        draw_rect((float)x, (float)y, 350, 72, 0x172a36ff);
        if (i < view->player_count) {
            snprintf(slot, sizeof(slot), "PLAYER %d   %.20s", i + 1, view->players[i].name);
            draw_text((float)x + 18, (float)y + 10, 16, 0xf0f6f8ff, slot);
            draw_text((float)x + 18, (float)y + 40, 14, view->players[i].ready ? 0x7ce0a8ff : 0xffc18aff, view->players[i].ready ? "READY" : "NOT READY");
        } else {
            snprintf(slot, sizeof(slot), "PLAYER %d   OPEN SEAT", i + 1);
            draw_text((float)x + 18, (float)y + 23, 16, 0x9bb4c2ff, slot);
        }
    }
    if (view->is_ready) draw_text(58, 338, 16, 0x7ce0a8ff, "FOUR SEATS READY - LOBBY ONLY");
    else draw_text(58, 338, 15, 0x9bb4c2ff, "Waiting for players. Game sync is not connected.");
    draw_rect(58, 390, 720, 38, 0x0b1720ff);
    draw_text(72, 398, 14, 0x9bb4c2ff, status);
    draw_text(56, 474, 12, 0x6d8794ff, "CROSS TOGGLE READY  SQUARE REFRESH  CIRCLE LEAVE ROOM");
    tiny3d_Flip();
}

int cb_online_menu(const char *guest_name, int *network_state, int *online_state, volatile int *exit_requested, char *status, size_t status_size)
{
    CBOnlineRoom rooms[MENU_ROOM_LIMIT];
    CBOnlineSession session;
    CBOnlineRoomView view;
    char error[160] = "";
    char join_code[7] = "AAAAAA";
    int screen = SCREEN_ROOMS;
    int room_count = 0;
    int selected = 0;
    int ready = 0;
    int private_create = 0;
    int code_cursor = 0;
    int curl_initialized = 0;
    int lobby_ready = 0;
    unsigned int poll_frames = 0;
    unsigned int room_list_poll_frames = 0;
    u16 old_up = 0, old_down = 0, old_left = 0, old_right = 0;
    u16 old_cross = 0, old_circle = 0, old_triangle = 0, old_square = 0;
    u16 old_l1 = 0, old_l2 = 0, old_r1 = 0;

    memset(&session, 0, sizeof(session));
    memset(&view, 0, sizeof(view));
    memset(rooms, 0, sizeof(rooms));

    if (ensure_lobby(network_state, online_state, status, status_size) == 0) lobby_ready = 1;
    else if (cb_online_init() == 0) curl_initialized = 1;

    while (!lobby_ready && (!exit_requested || !*exit_requested)) {
        u16 circle = 0, square = 0;
        padInfo info;
        padData data;
        sysUtilCheckCallback();
        if (ioPadGetInfo(&info) == 0) {
            int p;
            for (p = 0; p < MAX_PADS; p++) {
                if (info.status[p] && ioPadGetData(p, &data) == 0 && data.len >= 8) {
                    circle = data.BTN_CIRCLE;
                    square = data.BTN_SQUARE;
                    break;
                }
            }
        }
        tiny3d_Clear(0x071019ff, TINY3D_CLEAR_ALL);
        tiny3d_Project2D();
        draw_rect(24, 22, 800, 468, 0x101e2aff);
        draw_rect(24, 22, 800, 5, 0xff9a55ff);
        draw_text(56, 54, 27, 0xf5fbffff, "ONLINE SERVICE UNAVAILABLE");
        draw_text(56, 118, 16, 0xffc18aff, status);
        draw_text(56, 180, 15, 0x9bb4c2ff, "Check the network connection and try again.");
        draw_text(56, 474, 12, 0x6d8794ff, "SQUARE RETRY  CIRCLE BACK");
        tiny3d_Flip();
        if (circle) break;
        if (square && ensure_lobby(network_state, online_state, status, status_size) == 0) lobby_ready = 1;
    }
    if (!lobby_ready) {
        if (curl_initialized) cb_online_shutdown();
        return 0;
    }
    curl_initialized = 1;
    load_rooms(rooms, &room_count, &selected, status, status_size);
    room_list_poll_frames = 0;

    while (!exit_requested || !*exit_requested) {
        padInfo info;
        padData data;
        u16 up = 0, down = 0, left = 0, right = 0;
        u16 cross = 0, circle = 0, triangle = 0, square = 0;
        u16 l1 = 0, l2 = 0, r1 = 0;
        sysUtilCheckCallback();

        if (ioPadGetInfo(&info) == 0) {
            int p;
            for (p = 0; p < MAX_PADS; p++) {
                if (!info.status[p]) continue;
                if (ioPadGetData(p, &data) == 0 && data.len >= 8) {
                    up = data.BTN_UP; down = data.BTN_DOWN;
                    left = data.BTN_LEFT; right = data.BTN_RIGHT;
                    cross = data.BTN_CROSS; circle = data.BTN_CIRCLE;
                    triangle = data.BTN_TRIANGLE; square = data.BTN_SQUARE;
                    l1 = data.BTN_L1; l2 = data.BTN_L2; r1 = data.BTN_R1;
                    break;
                }
            }
        }

        if (screen == SCREEN_ROOMS) {
            if (++room_list_poll_frames >= ROOM_LIST_REFRESH_FRAMES) {
                room_list_poll_frames = 0;
                load_rooms(rooms, &room_count, &selected, status, status_size);
            }
            if (up && !old_up && room_count > 0) selected = (selected + room_count - 1) % room_count;
            if (down && !old_down && room_count > 0) selected = (selected + 1) % room_count;
            if (l1 && !old_l1) {
                private_create = !private_create;
                snprintf(status, status_size, "New room privacy: %s.", private_create ? "private" : "public");
            }
            if (r1 && !old_r1) { screen = SCREEN_JOIN_CODE; code_cursor = 0; }
            if (l2 && !old_l2) {
                if (cb_online_join_any_room(guest_name, &session, &view, error, sizeof(error)) == 0) {
                    ready = 0; screen = SCREEN_ROOM; poll_frames = 0; room_list_poll_frames = 0;
                    snprintf(status, status_size, "Joined public battle %.6s.", session.code);
                } else snprintf(status, status_size, "Any battle: %.120s", error);
            }
            if (cross && !old_cross && room_count > 0) {
                if (cb_online_join_room(rooms[selected].code, guest_name, &session, &view, error, sizeof(error)) == 0) {
                    ready = 0; screen = SCREEN_ROOM; poll_frames = 0; room_list_poll_frames = 0;
                    snprintf(status, status_size, "Joined room %.6s. Waiting for both players.", session.code);
                } else snprintf(status, status_size, "Join room: %.120s", error);
            }
            if (triangle && !old_triangle) {
                if (cb_online_create_room(guest_name, private_create, &session, &view, error, sizeof(error)) == 0) {
                    ready = 0; screen = SCREEN_ROOM; poll_frames = 0; room_list_poll_frames = 0;
                    snprintf(status, status_size, "%s room %.6s created. Share the code.", private_create ? "Private" : "Public", session.code);
                } else snprintf(status, status_size, "Create room: %.120s", error);
            }
            if (square && !old_square) load_rooms(rooms, &room_count, &selected, status, status_size);
            draw_rooms(rooms, room_count, selected, private_create, status);
            if (circle && !old_circle) break;
        } else if (screen == SCREEN_JOIN_CODE) {
            int code_len = (int)(sizeof(g_room_code_chars) - 1);
            if (left && !old_left) code_cursor = (code_cursor + 5) % 6;
            if (right && !old_right) code_cursor = (code_cursor + 1) % 6;
            if (up && !old_up) {
                const char *at = strchr(g_room_code_chars, join_code[code_cursor]);
                int index = at ? (int)(at - g_room_code_chars) : 0;
                join_code[code_cursor] = g_room_code_chars[(index + 1) % code_len];
            }
            if (down && !old_down) {
                const char *at = strchr(g_room_code_chars, join_code[code_cursor]);
                int index = at ? (int)(at - g_room_code_chars) : 0;
                join_code[code_cursor] = g_room_code_chars[(index + code_len - 1) % code_len];
            }
            if (cross && !old_cross) {
                if (cb_online_join_room(join_code, guest_name, &session, &view, error, sizeof(error)) == 0) {
                    ready = 0; screen = SCREEN_ROOM; poll_frames = 0; room_list_poll_frames = 0;
                    snprintf(status, status_size, "Joined private/public room %.6s.", session.code);
                } else snprintf(status, status_size, "Join code: %.120s", error);
            }
            if (circle && !old_circle) screen = SCREEN_ROOMS;
            draw_join_code(join_code, code_cursor, status);
        } else {
            if (cross && !old_cross) {
                int next_ready = !ready;
                if (cb_online_set_ready(&session, next_ready, &view, error, sizeof(error)) == 0) {
                    ready = next_ready;
                    snprintf(status, status_size, "%s", ready ? "You are ready." : "Ready status cancelled.");
                } else if (session_is_gone(error)) {
                    return_to_rooms(&session, &view, &ready, &screen, &poll_frames,
                                    rooms, &room_count, &selected, status, status_size);
                } else snprintf(status, status_size, "Ready update: %.120s", error);
            }
            if (square && !old_square && screen == SCREEN_ROOM) {
                if (cb_online_get_room(&session, &view, error, sizeof(error)) != 0) {
                    if (session_is_gone(error)) {
                        return_to_rooms(&session, &view, &ready, &screen, &poll_frames,
                                        rooms, &room_count, &selected, status, status_size);
                    } else snprintf(status, status_size, "Room refresh: %.120s", error);
                }
            }
            if (circle && !old_circle) {
                cb_online_leave_room(&session, error, sizeof(error));
                memset(&session, 0, sizeof(session));
                memset(&view, 0, sizeof(view));
                ready = 0; screen = SCREEN_ROOMS; poll_frames = 0; room_list_poll_frames = 0;
                load_rooms(rooms, &room_count, &selected, status, status_size);
            }
            if (screen == SCREEN_ROOM && ++poll_frames >= ROOM_HEARTBEAT_FRAMES) {
                poll_frames = 0;
                if (cb_online_set_ready(&session, ready, &view, error, sizeof(error)) != 0) {
                    if (session_is_gone(error)) {
                        return_to_rooms(&session, &view, &ready, &screen, &poll_frames,
                                        rooms, &room_count, &selected, status, status_size);
                    } else snprintf(status, status_size, "Heartbeat: %.120s", error);
                }
            }
            if (screen == SCREEN_ROOM) draw_room(&view, &session, status);
            else draw_rooms(rooms, room_count, selected, private_create, status);
        }

        old_up = up; old_down = down; old_left = left; old_right = right;
        old_cross = cross; old_circle = circle; old_triangle = triangle; old_square = square;
        old_l1 = l1; old_l2 = l2; old_r1 = r1;
    }

    if (session.token[0]) cb_online_leave_room(&session, error, sizeof(error));
    if (curl_initialized) cb_online_shutdown();
    return 0;
}
