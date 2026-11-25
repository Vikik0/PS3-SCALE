struct timeval;
#include "online_menu.h"
#include "game_launcher.h"
#include "disc_browser.h"
#include "disc_fingerprint.h"
#include "online.h"
#include <stdio.h>
#include <string.h>
#include <tiny3d.h>
#include <libfont.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>
#include <sys/thread.h>
#include <sys/time.h>
#include <net/net.h>

#define MENU_ROOM_LIMIT 8
#define ROOM_HEARTBEAT_FRAMES 300
#define ROOM_LIST_REFRESH_FRAMES 600
#define NETPLAY_DISCOVERY_POLL_FRAMES 120
#define NETPLAY_DISCOVERY_MAX_ATTEMPTS 60
#define SCREEN_ROOMS 0
#define SCREEN_ROOM 1
#define SCREEN_JOIN_CODE 2
#define SCREEN_DISC_SELECT 3

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

#define LOBBY_WORK_CONNECT 1
#define LOBBY_WORK_LIST 2
#define LOBBY_WORKER_STACK_SIZE 0x20000

typedef struct {
    volatile int done;
    int operation;
    int result;
    int curl_initialized;
    size_t room_count;
    CBOnlineRoom rooms[MENU_ROOM_LIMIT];
    char status[160];
} LobbyWorkerTask;

static char g_lobby_worker_name[] = "cb-lobby";

static int ensure_network_stack(int *network_state, int *online_state,
                                char *status, size_t size)
{
    int rc;
    if (!network_state || !online_state) return -1;
    if (*network_state == 1) return 0;
    rc = netInitialize();
    if (rc != 0) {
        *network_state = -1;
        *online_state = -1;
        snprintf(status, size, "Network initialization failed: 0x%08X",
                 (unsigned int)rc);
        return -1;
    }
    *network_state = 1;
    return 0;
}

static void lobby_worker(void *opaque)
{
    LobbyWorkerTask *task = (LobbyWorkerTask *)opaque;
    char error[160] = "";
    size_t count = 0;

    if (!task) {
        sysThreadExit(0);
        return;
    }

    if (task->operation == LOBBY_WORK_CONNECT) {
        if (cb_online_init() != 0) {
            task->result = -1;
            snprintf(task->status, sizeof(task->status),
                     "Could not initialize the HTTPS client.");
        } else {
            task->curl_initialized = 1;
            if (cb_online_health(error, sizeof(error)) != 0) {
                task->result = -1;
                snprintf(task->status, sizeof(task->status),
                         "Lobby service: %.120s", error);
            } else if (cb_online_list_rooms(task->rooms, MENU_ROOM_LIMIT,
                                            &count, error, sizeof(error)) != 0) {
                task->result = -1;
                snprintf(task->status, sizeof(task->status),
                         "Room list: %.120s", error);
            } else {
                task->result = 0;
                task->room_count = count;
                snprintf(task->status, sizeof(task->status),
                         "Found %d public room(s).", (int)count);
            }
        }
    } else if (task->operation == LOBBY_WORK_LIST) {
        if (cb_online_list_rooms(task->rooms, MENU_ROOM_LIMIT,
                                 &count, error, sizeof(error)) != 0) {
            task->result = -1;
            snprintf(task->status, sizeof(task->status),
                     "Room list: %.120s", error);
        } else {
            task->result = 0;
            task->room_count = count;
            snprintf(task->status, sizeof(task->status),
                     "Found %d public room(s).", (int)count);
        }
    } else {
        task->result = -1;
        snprintf(task->status, sizeof(task->status), "Unknown lobby task.");
    }

    __sync_synchronize();
    task->done = 1;
    sysThreadExit(0);
}

static int start_lobby_worker(sys_ppu_thread_t *thread,
                              LobbyWorkerTask *task, int operation)
{
    if (!thread || !task) return -1;
    memset(task, 0, sizeof(*task));
    task->operation = operation;
    if (sysThreadCreate(thread, lobby_worker, task, 1500,
                        LOBBY_WORKER_STACK_SIZE, THREAD_JOINABLE,
                        g_lobby_worker_name) != 0) {
        task->result = -1;
        snprintf(task->status, sizeof(task->status),
                 "Could not start the lobby network worker.");
        return -1;
    }
    return 0;
}

static void join_lobby_worker(sys_ppu_thread_t thread)
{
    u64 result = 0;
    (void)sysThreadJoin(thread, &result);
    __sync_synchronize();
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
    if (rooms) memset(rooms, 0, sizeof(CBOnlineRoom) * MENU_ROOM_LIMIT);
    if (room_count) *room_count = 0;
    if (selected) *selected = 0;
    snprintf(status, status_size, "Your room ended or your seat expired. Press SQUARE to refresh.");
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

static void draw_lobby_message(const char *title, const char *message,
                               const char *controls, int warning)
{
    tiny3d_Clear(0x071019ff, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();
    draw_rect(24, 22, 800, 468, 0x101e2aff);
    draw_rect(24, 22, 800, 5, warning ? 0xff9a55ff : 0x46d6b2ff);
    draw_text(56, 54, 27, 0xf5fbffff, title);
    draw_text(56, 118, 16, warning ? 0xffc18aff : 0x61d9baff, message);
    draw_text(56, 180, 15, 0x9bb4c2ff,
              warning ? "Check the network connection, then retry."
                      : "The lobby request is running in the background.");
    draw_rect(58, 438, 720, 32, 0x0b1720ff);
    draw_text(72, 444, 13, 0x9bb4c2ff, controls ? controls : "");
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
    if (view->game_started) {
        draw_text(58, 338, 16, 0x7ce0a8ff, "MATCH STARTED - PRESS START, THEN SELECT THE SAME CUE TO JOIN");
    } else if (view->is_ready) {
        draw_text(58, 338, 16, 0x7ce0a8ff, "ALL PLAYERS READY - PRESS START TO SELECT THE GAME");
    } else if (view->player_count < 2) {
        draw_text(58, 338, 15, 0x9bb4c2ff, "Waiting for at least two players.");
    } else {
        draw_text(58, 338, 15, 0x9bb4c2ff, "Waiting for every player to mark READY.");
    }
    if (view->disc_mismatch) {
        draw_text(58, 365, 13, 0xff9a80ff, "DISC MISMATCH - ALL PLAYERS MUST SELECT THE SAME CUE CONTENT");
    } else if (view->disc_match) {
        draw_text(58, 365, 13, 0x7ce0a8ff, "DISC CONTENT MATCHED ON ALL PLAYERS");
    } else {
        char disc_status[80];
        snprintf(disc_status, sizeof(disc_status), "DISC CONFIRMATION: %d/%d PLAYERS", view->disc_selection_count, view->player_count);
        draw_text(58, 365, 13, 0x9bb4c2ff, disc_status);
    }
    draw_rect(58, 390, 720, 38, 0x0b1720ff);
    draw_text(72, 398, 14, 0x9bb4c2ff, status);
    if (view->game_started)
        draw_text(56, 474, 12, 0x6d8794ff, "START JOIN MATCH  SQUARE REFRESH  CIRCLE BACK");
    else
        draw_text(56, 474, 12, 0x6d8794ff, "CROSS READY  START SELECT GAME  SQUARE REFRESH  CIRCLE LEAVE");
    tiny3d_Flip();
}

static int session_is_room_host(const CBOnlineSession *session,
                                const CBOnlineRoomView *view)
{
    const char *host_id;
    if (!session || !view || !session->player_id[0]) return 0;
    host_id = view->host_player_id;
    if (!host_id[0] && view->player_count > 0) host_id = view->players[0].id;
    return host_id[0] && strcmp(host_id, session->player_id) == 0;
}

static int clear_disc_confirmation(CBOnlineSession *session,
                                     CBOnlineRoomView *view,
                                     char *error, size_t error_size)
{
    int i;
    if (!session || !view) return -1;
    if (view->game_started) return 0;
    for (i = 0; i < view->player_count; i++) {
        if (strcmp(view->players[i].id, session->player_id) == 0 &&
            view->players[i].disc_fingerprint[0]) {
            return cb_online_clear_disc_fingerprint(session, view, error, error_size);
        }
    }
    return 0;
}

static void make_netplay_nickname(char *output, size_t output_size,
                                  const CBOnlineSession *session,
                                  const CBOnlineRoomView *view, int is_host)
{
    int i;
    int player_number = 2;
    if (!output || output_size == 0 || !session || !view) return;
    for (i = 0; i < view->player_count; i++) {
        if (strcmp(view->players[i].id, session->player_id) == 0) {
            player_number = i + 1;
            break;
        }
    }
    {
        const char *room_id = view->netplay_id[0] ? view->netplay_id : session->code;
        if (is_host) snprintf(output, output_size, "CB-%.12s-HOST", room_id);
        else snprintf(output, output_size, "CB-%.12s-P%d", room_id, player_number);
    }
}

static void draw_disc_select(const CBGameDisc *discs, int count, int selected,
                             const CBOnlineSession *session, int is_host,
                             const char *status)
{
    int i;
    char line[112];
    tiny3d_Clear(0x071019ff, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();
    draw_rect(24, 22, 800, 468, 0x101e2aff);
    draw_rect(24, 22, 800, 5, 0x46d6b2ff);
    draw_text(56, 42, 26, 0xf5fbffff, "SELECT CRASH BASH DISC");
    snprintf(line, sizeof(line), "ROOM %.6s  |  ROLE: %s",
             session ? session->code : "", is_host ? "HOST" : "CLIENT");
    draw_text(56, 82, 15, 0x61d9baff, line);

    if (count == 0) {
        draw_rect(58, 130, 720, 170, 0x0b1720ff);
        draw_text(82, 158, 18, 0xffc18aff, "No built-in or external CUE file was found.");
        draw_text(82, 198, 14, 0xc2d0d8ff, "Check the installed package game folder:");
        draw_text(82, 232, 14, 0x9bb4c2ff, "/dev_hdd0/game/CBON00001/USRDIR/PSXISO/");
        draw_text(82, 258, 14, 0x9bb4c2ff, "External dumps: /dev_hdd0/PSXISO/ or USB.");
    } else {
        for (i = 0; i < count; i++) {
            u32 background = i == selected ? 0x28594fff : 0x172a36ff;
            u32 foreground = i == selected ? 0xffffffff : 0xc2d0d8ff;
            snprintf(line, sizeof(line), "%c  %.52s",
                     i == selected ? '>' : ' ', discs[i].name);
            draw_rect(58, 118 + i * 23, 720, 22, background);
            draw_text(76, 120 + i * 23, 14, foreground, line);
        }
        if (selected >= 0 && selected < count) {
            draw_text(58, 430, 11, 0x9bb4c2ff, discs[selected].path);
        }
    }

    if (is_host) {
        draw_text(58, 398, 13, 0x7ce0a8ff, "HOST: CROSS CONFIRMS DISC; LAUNCHES WHEN ALL MATCH.");
    } else {
        draw_text(58, 398, 13, 0xffc18aff, "CLIENT: CROSS CONFIRMS DISC; THEN WAITS FOR HOST.");
    }
    draw_text(58, 450, 13, 0x9bb4c2ff, status ? status : "");
    draw_text(56, 474, 12, 0x6d8794ff, "UP/DOWN SELECT  CROSS START / JOIN  SQUARE RESCAN  CIRCLE BACK");
    tiny3d_Flip();
}

int cb_online_menu(const char *guest_name, int *network_state, int *online_state, volatile int *exit_requested, char *status, size_t status_size)
{
    CBOnlineRoom rooms[MENU_ROOM_LIMIT];
    CBOnlineSession session;
    CBOnlineRoomView view;
    CBGameDisc discs[CB_DISC_MAX];
    CBOnlineNetplayEndpoint endpoint;
    char error[160] = "";
    char netplay_nickname[32] = "";
    char join_code[7] = "AAAAAA";
    int screen = SCREEN_ROOMS;
    int room_count = 0;
    int selected = 0;
    int ready = 0;
    int private_create = 0;
    int code_cursor = 0;
    int disc_count = 0;
    int disc_selected = 0;
    int netplay_waiting = 0;
    int netplay_wait_attempts = 0;
    int host_disc_waiting = 0;
    int host_disc_selected = -1;
    unsigned int disc_poll_frames = 0;
    unsigned int netplay_poll_frames = 0;
    int curl_initialized = 0;
    int lobby_ready = 0;
    unsigned int poll_frames = 0;
    unsigned int room_list_poll_frames = 0;
    LobbyWorkerTask worker;
    sys_ppu_thread_t worker_thread = 0;
    int worker_active = 0;
    int exit_after_worker = 0;
    u16 old_up = 0, old_down = 0, old_left = 0, old_right = 0;
    u16 old_cross = 0, old_circle = 0, old_triangle = 0, old_square = 0;
    u16 old_l1 = 0, old_l2 = 0, old_r1 = 0, old_start = 0;

    memset(&session, 0, sizeof(session));
    memset(&view, 0, sizeof(view));
    memset(&endpoint, 0, sizeof(endpoint));
    memset(rooms, 0, sizeof(rooms));
    memset(&worker, 0, sizeof(worker));

    if (ensure_network_stack(network_state, online_state, status, status_size) == 0) {
        if (start_lobby_worker(&worker_thread, &worker, LOBBY_WORK_CONNECT) == 0)
            worker_active = 1;
        else
            snprintf(status, status_size, "%s", worker.status);
    }

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
        if (circle) exit_after_worker = 1;

        if (worker_active) {
            if (worker.done) {
                join_lobby_worker(worker_thread);
                worker_active = 0;
                if (worker.curl_initialized) curl_initialized = 1;
                if (exit_after_worker) break;
                if (worker.result == 0) {
                    memcpy(rooms, worker.rooms, sizeof(rooms));
                    room_count = (int)worker.room_count;
                    selected = 0;
                    if (online_state) *online_state = 1;
                    snprintf(status, status_size, "%s", worker.status);
                    lobby_ready = 1;
                    break;
                }
                if (online_state) *online_state = -1;
                snprintf(status, status_size, "%s",
                         worker.status[0] ? worker.status : "Lobby request failed.");
            } else {
                draw_lobby_message("CONNECTING TO ONLINE ROOMS",
                                   "Checking the cloud lobby and loading public rooms...",
                                   "CIRCLE BACK", 0);
                if (circle)
                    snprintf(status, status_size, "Waiting for the lobby request to finish...");
                continue;
            }
        }

        draw_lobby_message("ONLINE SERVICE UNAVAILABLE", status,
                           "SQUARE RETRY  CIRCLE BACK", 1);
        if (circle) break;
        if (square) {
            if (ensure_network_stack(network_state, online_state, status, status_size) == 0) {
                if (start_lobby_worker(&worker_thread, &worker, LOBBY_WORK_CONNECT) == 0) {
                    worker_active = 1;
                    snprintf(status, status_size, "Connecting to the cloud lobby...");
                } else {
                    snprintf(status, status_size, "%s", worker.status);
                }
            }
        }
    }

    if (worker_active) {
        while (!worker.done) {
            sysUtilCheckCallback();
            draw_lobby_message("CLOSING ONLINE ROOMS",
                               "Finishing the pending network request...", "", 0);
        }
        join_lobby_worker(worker_thread);
        worker_active = 0;
        if (worker.curl_initialized) curl_initialized = 1;
        if (exit_requested && *exit_requested) exit_after_worker = 1;
        if (!exit_after_worker && worker.result == 0) {
            memcpy(rooms, worker.rooms, sizeof(rooms));
            room_count = (int)worker.room_count;
            selected = 0;
            if (online_state) *online_state = 1;
            snprintf(status, status_size, "%s", worker.status);
            lobby_ready = 1;
        } else if (!exit_after_worker && worker.status[0]) {
            snprintf(status, status_size, "%s", worker.status);
            if (online_state) *online_state = -1;
        }
    }

    if (exit_after_worker) lobby_ready = 0;
    if (!lobby_ready) {
        if (curl_initialized) cb_online_shutdown();
        return 0;
    }
    curl_initialized = 1;
    room_list_poll_frames = 0;

    while (!exit_requested || !*exit_requested) {
        padInfo info;
        padData data;
        u16 up = 0, down = 0, left = 0, right = 0;
        u16 cross = 0, circle = 0, triangle = 0, square = 0;
        u16 l1 = 0, l2 = 0, r1 = 0, start = 0;
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
                    l1 = data.BTN_L1; l2 = data.BTN_L2; r1 = data.BTN_R1; start = data.BTN_START;
                    break;
                }
            }
        }

        if (worker_active && worker.done) {
            join_lobby_worker(worker_thread);
            worker_active = 0;
            if (worker.result == 0) {
                memcpy(rooms, worker.rooms, sizeof(rooms));
                room_count = (int)worker.room_count;
                if (room_count == 0) selected = 0;
                else if (selected >= room_count) selected = room_count - 1;
                snprintf(status, status_size, "%s", worker.status);
            } else {
                snprintf(status, status_size, "%s",
                         worker.status[0] ? worker.status : "Room refresh failed.");
            }
            if (exit_after_worker) break;
        }

        if (screen == SCREEN_ROOMS) {
            if (++room_list_poll_frames >= ROOM_LIST_REFRESH_FRAMES) {
                room_list_poll_frames = 0;
                if (!worker_active) {
                    if (start_lobby_worker(&worker_thread, &worker, LOBBY_WORK_LIST) == 0) {
                        worker_active = 1;
                        snprintf(status, status_size, "Refreshing public rooms...");
                    } else {
                        snprintf(status, status_size, "%s", worker.status);
                    }
                }
            }
            if (up && !old_up && room_count > 0) selected = (selected + room_count - 1) % room_count;
            if (down && !old_down && room_count > 0) selected = (selected + 1) % room_count;
            if (l1 && !old_l1) {
                private_create = !private_create;
                snprintf(status, status_size, "New room privacy: %s.", private_create ? "private" : "public");
            }
            if (r1 && !old_r1) { screen = SCREEN_JOIN_CODE; code_cursor = 0; }
            if (l2 && !old_l2 && !worker_active) {
                if (cb_online_join_any_room(guest_name, &session, &view, error, sizeof(error)) == 0) {
                    ready = 0; screen = SCREEN_ROOM; poll_frames = 0; room_list_poll_frames = 0;
                    snprintf(status, status_size, "Joined public battle %.6s.", session.code);
                } else snprintf(status, status_size, "Any battle: %.120s", error);
            }
            if (cross && !old_cross && room_count > 0 && !worker_active) {
                if (cb_online_join_room(rooms[selected].code, guest_name, &session, &view, error, sizeof(error)) == 0) {
                    ready = 0; screen = SCREEN_ROOM; poll_frames = 0; room_list_poll_frames = 0;
                    snprintf(status, status_size, "Joined room %.6s. Waiting for both players.", session.code);
                } else snprintf(status, status_size, "Join room: %.120s", error);
            }
            if (triangle && !old_triangle && !worker_active) {
                if (cb_online_create_room(guest_name, private_create, &session, &view, error, sizeof(error)) == 0) {
                    ready = 0; screen = SCREEN_ROOM; poll_frames = 0; room_list_poll_frames = 0;
                    snprintf(status, status_size, "%s room %.6s created. Share the code.", private_create ? "Private" : "Public", session.code);
                } else snprintf(status, status_size, "Create room: %.120s", error);
            }
            if (square && !old_square && !worker_active) {
                if (start_lobby_worker(&worker_thread, &worker, LOBBY_WORK_LIST) == 0) {
                    worker_active = 1;
                    snprintf(status, status_size, "Refreshing public rooms...");
                } else {
                    snprintf(status, status_size, "%s", worker.status);
                }
            }
            draw_rooms(rooms, room_count, selected, private_create, status);
            if (circle && !old_circle) {
                if (worker_active) exit_after_worker = 1;
                else break;
            }
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
            if (cross && !old_cross && !worker_active) {
                if (cb_online_join_room(join_code, guest_name, &session, &view, error, sizeof(error)) == 0) {
                    ready = 0; screen = SCREEN_ROOM; poll_frames = 0; room_list_poll_frames = 0;
                    snprintf(status, status_size, "Joined private/public room %.6s.", session.code);
                } else snprintf(status, status_size, "Join code: %.120s", error);
            }
            if (circle && !old_circle) screen = SCREEN_ROOMS;
            draw_join_code(join_code, code_cursor, status);
        } else if (screen == SCREEN_DISC_SELECT) {
            if (up && !old_up && disc_count > 0) {
                if (clear_disc_confirmation(&session, &view, error, sizeof(error)) != 0) {
                    snprintf(status, status_size, "Could not clear disc confirmation: %.90s", error);
                } else {
                    disc_selected = (disc_selected + disc_count - 1) % disc_count;
                    netplay_waiting = 0; host_disc_waiting = 0; host_disc_selected = -1;
                    snprintf(status, status_size, "Selection changed. Press CROSS to confirm this disc.");
                }
            }
            if (down && !old_down && disc_count > 0) {
                if (clear_disc_confirmation(&session, &view, error, sizeof(error)) != 0) {
                    snprintf(status, status_size, "Could not clear disc confirmation: %.90s", error);
                } else {
                    disc_selected = (disc_selected + 1) % disc_count;
                    netplay_waiting = 0; host_disc_waiting = 0; host_disc_selected = -1;
                    snprintf(status, status_size, "Selection changed. Press CROSS to confirm this disc.");
                }
            }
            if (square && !old_square) {
                size_t scanned = 0;
                if (clear_disc_confirmation(&session, &view, error, sizeof(error)) != 0) {
                    snprintf(status, status_size, "Could not clear disc confirmation: %.90s", error);
                } else {
                    netplay_waiting = 0; host_disc_waiting = 0; host_disc_selected = -1;
                    if (cb_disc_scan(discs, CB_DISC_MAX, &scanned) == 0) {
                        disc_count = (int)scanned;
                        if (disc_count == 0) disc_selected = 0;
                        else if (disc_selected >= disc_count) disc_selected = disc_count - 1;
                        snprintf(status, status_size, "Found %d CUE file(s).", disc_count);
                    } else {
                        snprintf(status, status_size, "Disc scan failed.");
                    }
                }
            }
            if (cross && !old_cross && disc_count > 0) {
                if (!view.is_ready) {
                    screen = SCREEN_ROOM;
                    snprintf(status, status_size, "Room changed. All active players must be READY again.");
                } else {
                    int is_host = session_is_room_host(&session, &view);
                    char fingerprint[17];
                    if (cb_disc_fingerprint(discs[disc_selected].path, fingerprint, sizeof(fingerprint)) != 0) {
                        snprintf(status, status_size, "Could not read the selected CUE or one of its track files.");
                    } else if (cb_online_set_disc_fingerprint(&session, fingerprint, &view, error, sizeof(error)) != 0) {
                        snprintf(status, status_size, "Disc confirmation: %.110s", error);
                    } else if (view.disc_mismatch) {
                        netplay_waiting = 0;
                        host_disc_waiting = is_host;
                        host_disc_selected = disc_selected;
                        disc_poll_frames = 0;
                        snprintf(status, status_size, "Disc mismatch. All players must select identical CUE content.");
                    } else if (!view.disc_match) {
                        if (is_host) {
                            host_disc_waiting = 1;
                            host_disc_selected = disc_selected;
                            disc_poll_frames = 0;
                            snprintf(status, status_size, "Disc confirmed: %d/%d players. Waiting...",
                                     view.disc_selection_count, view.player_count);
                        } else {
                            netplay_waiting = 1;
                            netplay_wait_attempts = 0;
                            netplay_poll_frames = NETPLAY_DISCOVERY_POLL_FRAMES - 1;
                            snprintf(status, status_size, "Disc confirmed. Waiting for all players and the host...");
                        }
                    } else if (is_host) {
                        host_disc_waiting = 0;
                        make_netplay_nickname(netplay_nickname, sizeof(netplay_nickname), &session, &view, 1);
                        if (cb_online_mark_game_started(&session, error, sizeof(error)) != 0) {
                            snprintf(status, status_size, "Could not lock the room for launch: %.100s", error);
                        } else {
                            int launch_result = cb_launch_game(discs[disc_selected].path, 1, netplay_nickname, NULL, error, sizeof(error));
                            if (launch_result < 0) {
                                char launch_error[160];
                                snprintf(launch_error, sizeof(launch_error), "%.150s", error);
                                if (cb_online_cancel_game_start(&session, error, sizeof(error)) == 0)
                                    snprintf(status, status_size, "%.120s", launch_error);
                                else
                                    snprintf(status, status_size, "Launch failed: %.52s / unlock failed: %.52s", launch_error, error);
                            }
                        }
                    } else {
                        host_disc_waiting = 0;
                        netplay_waiting = 1;
                        netplay_wait_attempts = 0;
                        netplay_poll_frames = NETPLAY_DISCOVERY_POLL_FRAMES - 1;
                        snprintf(status, status_size, "Disc content matches. Waiting for the host to start...");
                    }
                }
            }
            if (circle && !old_circle) {
                if (clear_disc_confirmation(&session, &view, error, sizeof(error)) != 0) {
                    snprintf(status, status_size, "Could not clear disc confirmation: %.90s", error);
                } else {
                    netplay_waiting = 0; host_disc_waiting = 0; host_disc_selected = -1;
                    screen = SCREEN_ROOM;
                }
            }
            draw_disc_select(discs, disc_count, disc_selected, &session,
                             session_is_room_host(&session, &view), status);
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
            if (start && !old_start && screen == SCREEN_ROOM) {
                if (!view.is_ready) {
                    snprintf(status, status_size, "At least two players must join and all must be READY.");
                } else {
                    size_t scanned = 0;
                    if (cb_disc_scan(discs, CB_DISC_MAX, &scanned) != 0) {
                        disc_count = 0;
                        snprintf(status, status_size, "Could not scan the PS3 disc folders.");
                    } else {
                        disc_count = (int)scanned;
                        disc_selected = 0;
                        netplay_waiting = 0;
                        netplay_wait_attempts = 0;
                        netplay_poll_frames = 0;
                        screen = SCREEN_DISC_SELECT;
                    }
                    if (disc_count == 0) {
                        snprintf(status, status_size, "Copy the Crash Bash CUE file to a PSXISO folder, then rescan.");
                    } else {
                        snprintf(status, status_size, "Select the same Crash Bash CUE file on every console.");
                    }
                }
            }
            if (square && !old_square && screen == SCREEN_ROOM) {
                if (cb_online_get_room(&session, &view, error, sizeof(error)) != 0) {
                    if (session_is_gone(error)) {
                        return_to_rooms(&session, &view, &ready, &screen, &poll_frames,
                                        rooms, &room_count, &selected, status, status_size);
                    } else snprintf(status, status_size, "Room refresh: %.120s", error);
                } else {
                    ready = session.ready;
                }
            }
            if (circle && !old_circle) {
                cb_online_leave_room(&session, error, sizeof(error));
                memset(&session, 0, sizeof(session));
                memset(&view, 0, sizeof(view));
                ready = 0; screen = SCREEN_ROOMS; poll_frames = 0; room_list_poll_frames = 0;
                if (start_lobby_worker(&worker_thread, &worker, LOBBY_WORK_LIST) == 0) {
                    worker_active = 1;
                    snprintf(status, status_size, "Refreshing public rooms...");
                } else {
                    snprintf(status, status_size, "%s", worker.status);
                }
            }
            if ((screen == SCREEN_ROOM || screen == SCREEN_DISC_SELECT) &&
                ++poll_frames >= ROOM_HEARTBEAT_FRAMES) {
                poll_frames = 0;
                if (cb_online_heartbeat(&session, &view, error, sizeof(error)) != 0) {
                    if (session_is_gone(error)) {
                        return_to_rooms(&session, &view, &ready, &screen, &poll_frames,
                                        rooms, &room_count, &selected, status, status_size);
                    } else snprintf(status, status_size, "Heartbeat: %.120s", error);
                } else {
                    ready = session.ready;
                }
            }
            if (screen == SCREEN_DISC_SELECT && host_disc_waiting &&
                ++disc_poll_frames >= NETPLAY_DISCOVERY_POLL_FRAMES) {
                int launch_result;
                int launch_disc = host_disc_selected >= 0 && host_disc_selected < disc_count
                    ? host_disc_selected : disc_selected;
                disc_poll_frames = 0;
                if (cb_online_get_room(&session, &view, error, sizeof(error)) != 0) {
                    if (session_is_gone(error)) {
                        host_disc_waiting = 0;
                        return_to_rooms(&session, &view, &ready, &screen, &poll_frames,
                                        rooms, &room_count, &selected, status, status_size);
                    } else snprintf(status, status_size, "Disc check: %.110s", error);
                } else if (!view.is_ready) {
                    host_disc_waiting = 0; screen = SCREEN_ROOM;
                    snprintf(status, status_size, "Room changed. All active players must be READY again.");
                } else if (view.disc_match) {
                    host_disc_waiting = 0;
                    make_netplay_nickname(netplay_nickname, sizeof(netplay_nickname), &session, &view, 1);
                    if (cb_online_mark_game_started(&session, error, sizeof(error)) != 0) {
                        snprintf(status, status_size, "Could not lock the room for launch: %.100s", error);
                    } else {
                        launch_result = cb_launch_game(discs[launch_disc].path, 1, netplay_nickname, NULL, error, sizeof(error));
                        if (launch_result < 0) {
                            char launch_error[160];
                            snprintf(launch_error, sizeof(launch_error), "%.150s", error);
                            if (cb_online_cancel_game_start(&session, error, sizeof(error)) == 0)
                                snprintf(status, status_size, "%.120s", launch_error);
                            else
                                snprintf(status, status_size, "Launch failed: %.52s / unlock failed: %.52s", launch_error, error);
                        }
                    }
                } else if (view.disc_mismatch) {
                    snprintf(status, status_size, "Disc mismatch. Select identical CUE content on every console.");
                } else {
                    snprintf(status, status_size, "Disc confirmation: %d/%d players. Waiting...",
                             view.disc_selection_count, view.player_count);
                }
            }
            if (screen == SCREEN_DISC_SELECT && netplay_waiting &&
                ++netplay_poll_frames >= NETPLAY_DISCOVERY_POLL_FRAMES) {
                int endpoint_result;
                netplay_poll_frames = 0;
                if (!view.is_ready) {
                    netplay_waiting = 0;
                    snprintf(status, status_size, "Room changed. All active players must be READY again.");
                    screen = SCREEN_ROOM;
                } else {
                    endpoint_result = cb_online_get_netplay_endpoint(&session, &endpoint,
                                                                      error, sizeof(error));
                    if (endpoint_result == 0) {
                        netplay_waiting = 0;
                        make_netplay_nickname(netplay_nickname, sizeof(netplay_nickname),
                                              &session, &view, 0);
                        snprintf(status, status_size, "Host found: %.42s / %.32s. Connecting...",
                                 endpoint.game_name, endpoint.core_name);
                        cb_launch_game(discs[disc_selected].path, 0, netplay_nickname,
                                       &endpoint, error, sizeof(error));
                        snprintf(status, status_size, "%.120s", error);
                    } else if (endpoint_result == 1) {
                        if (strcmp(endpoint.state, "disc_mismatch") == 0) {
                            netplay_waiting = 0;
                            snprintf(status, status_size, "Disc mismatch. Select the same CUE content as the other players.");
                        } else if (strcmp(endpoint.state, "host_password_required") == 0) {
                            netplay_waiting = 0;
                            snprintf(status, status_size, "Host Netplay is password-protected. Disable the password, then press CROSS to retry.");
                        } else {
                            netplay_wait_attempts++;
                            if (netplay_wait_attempts >= NETPLAY_DISCOVERY_MAX_ATTEMPTS) {
                                netplay_waiting = 0;
                                if (strcmp(endpoint.state, "waiting_for_disc_selection") == 0) {
                                    snprintf(status, status_size, "Timed out waiting for every player to confirm the same disc.");
                                } else if (strcmp(endpoint.state, "waiting_for_host_start") == 0) {
                                    snprintf(status, status_size, "The host has not started Netplay yet. Ask the host to start the match.");
                                } else if (strcmp(endpoint.state, "host_not_connectable") == 0) {
                                    snprintf(status, status_size, "Host is not connectable. Check NAT, relay and host network settings.");
                                } else {
                                    snprintf(status, status_size, "Host did not appear in the RetroArch lobby. Check Netplay announcement and network.");
                                }
                            } else if (strcmp(endpoint.state, "waiting_for_disc_selection") == 0) {
                                snprintf(status, status_size, "Waiting for all players to confirm the same disc...");
                            } else if (strcmp(endpoint.state, "waiting_for_host_start") == 0) {
                                snprintf(status, status_size, "Disc confirmed. Waiting for the host to start Netplay...");
                            } else if (strcmp(endpoint.state, "host_not_connectable") == 0) {
                                snprintf(status, status_size, "Host is visible but not connectable. Checking relay... %d/%d",
                                         netplay_wait_attempts, NETPLAY_DISCOVERY_MAX_ATTEMPTS);
                            } else {
                                snprintf(status, status_size, "Waiting for host announcement... %d/%d",
                                         netplay_wait_attempts, NETPLAY_DISCOVERY_MAX_ATTEMPTS);
                            }
                        }
                    } else if (session_is_gone(error)) {
                        netplay_waiting = 0;
                        return_to_rooms(&session, &view, &ready, &screen, &poll_frames,
                                        rooms, &room_count, &selected, status, status_size);
                    } else if (strstr(error, "Every active player must be READY") != NULL) {
                        netplay_waiting = 0;
                        screen = SCREEN_ROOM;
                        snprintf(status, status_size, "Room is no longer READY. Every active player must press CROSS again.");
                    } else {
                        netplay_wait_attempts++;
                        snprintf(status, status_size, "Host lookup retry %d: %.90s",
                                 netplay_wait_attempts, error);
                        if (netplay_wait_attempts >= NETPLAY_DISCOVERY_MAX_ATTEMPTS) {
                            netplay_waiting = 0;
                            snprintf(status, status_size, "Could not locate the host. Check the network and try again.");
                        }
                    }
                }
            }
            if (screen == SCREEN_ROOM) draw_room(&view, &session, status);
            else if (screen == SCREEN_DISC_SELECT) {
                draw_disc_select(discs, disc_count, disc_selected, &session,
                                 session_is_room_host(&session, &view), status);
            } else draw_rooms(rooms, room_count, selected, private_create, status);
        }

        old_up = up; old_down = down; old_left = left; old_right = right;
        old_cross = cross; old_circle = circle; old_triangle = triangle; old_square = square;
        old_l1 = l1; old_l2 = l2; old_r1 = r1; old_start = start;
    }

    if (worker_active) {
        while (!worker.done) {
            sysUtilCheckCallback();
            draw_lobby_message("CLOSING ONLINE ROOMS",
                               "Finishing the pending room refresh...", "", 0);
        }
        join_lobby_worker(worker_thread);
        worker_active = 0;
    }
    if (session.token[0]) cb_online_leave_room(&session, error, sizeof(error));
    if (curl_initialized) cb_online_shutdown();
    return 0;
}
