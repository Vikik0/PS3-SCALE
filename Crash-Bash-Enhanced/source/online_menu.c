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
#define LOBBY_WORK_JOIN_ANY 3
#define LOBBY_WORK_JOIN 4
#define LOBBY_WORK_CREATE 5
#define LOBBY_WORK_GET_ROOM 6
#define LOBBY_WORK_SET_READY 7
#define LOBBY_WORK_HEARTBEAT 8
#define LOBBY_WORK_LEAVE 9
#define LOBBY_WORK_SET_DISC 10
#define LOBBY_WORK_CLEAR_DISC 11
#define LOBBY_WORK_MARK_STARTED 12
#define LOBBY_WORK_CANCEL_STARTED 13
#define LOBBY_WORK_NETPLAY_ENDPOINT 14
#define LOBBY_WORK_CONFIRM_DISC 15
#define LOBBY_WORK_SCAN_DISCS 16
#define DISC_ACTION_NONE 0
#define DISC_ACTION_SELECT 1
#define DISC_ACTION_RESCAN 2
#define DISC_ACTION_BACK 3
#define LOBBY_WORKER_STACK_SIZE 0x20000

typedef struct {
    volatile int done;
    int operation;
    int result;
    int curl_initialized;
    int ready;
    int is_private;
    size_t room_count;
    CBOnlineRoom rooms[MENU_ROOM_LIMIT];
    size_t disc_count;
    CBGameDisc discs[CB_DISC_MAX];
    int scan_open_selector;
    CBOnlineSession session;
    CBOnlineRoomView view;
    CBOnlineNetplayEndpoint endpoint;
    char fingerprint[17];
    char disc_path[CB_DISC_PATH_MAX];
    char guest_name[CB_ONLINE_NAME_MAX];
    char room_code[7];
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
    int rc = -1;

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
        switch (task->operation) {
        case LOBBY_WORK_JOIN_ANY:
            rc = cb_online_join_any_room(task->guest_name, &task->session,
                                          &task->view, error, sizeof(error));
            break;
        case LOBBY_WORK_JOIN:
            rc = cb_online_join_room(task->room_code, task->guest_name,
                                     &task->session, &task->view,
                                     error, sizeof(error));
            break;
        case LOBBY_WORK_CREATE:
            rc = cb_online_create_room(task->guest_name, task->is_private,
                                       &task->session, &task->view,
                                       error, sizeof(error));
            break;
        case LOBBY_WORK_GET_ROOM:
            rc = cb_online_get_room(&task->session, &task->view,
                                    error, sizeof(error));
            break;
        case LOBBY_WORK_SET_READY:
            rc = cb_online_set_ready(&task->session, task->ready,
                                     &task->view, error, sizeof(error));
            break;
        case LOBBY_WORK_HEARTBEAT:
            rc = cb_online_heartbeat(&task->session, &task->view,
                                     error, sizeof(error));
            break;
        case LOBBY_WORK_SET_DISC:
            rc = cb_online_set_disc_fingerprint(&task->session, task->fingerprint,
                                                  &task->view, error, sizeof(error));
            break;
        case LOBBY_WORK_CONFIRM_DISC:
            if (cb_disc_fingerprint(task->disc_path, task->fingerprint,
                                    sizeof(task->fingerprint)) != 0) {
                snprintf(error, sizeof(error),
                         "Could not read the selected CUE or one of its track files.");
                rc = -1;
            } else {
                rc = cb_online_set_disc_fingerprint(&task->session, task->fingerprint,
                                                     &task->view, error, sizeof(error));
            }
            break;
        case LOBBY_WORK_CLEAR_DISC:
            rc = cb_online_clear_disc_fingerprint(&task->session, &task->view,
                                                   error, sizeof(error));
            break;
        case LOBBY_WORK_MARK_STARTED:
            rc = cb_online_mark_game_started(&task->session, &task->view,
                                              error, sizeof(error));
            break;
        case LOBBY_WORK_CANCEL_STARTED:
            rc = cb_online_cancel_game_start(&task->session, error, sizeof(error));
            break;
        case LOBBY_WORK_NETPLAY_ENDPOINT:
            rc = cb_online_get_netplay_endpoint(&task->session, &task->endpoint,
                                                 error, sizeof(error));
            break;
        case LOBBY_WORK_SCAN_DISCS:
            rc = cb_disc_scan(task->discs, CB_DISC_MAX, &task->disc_count);
            if (rc != 0)
                snprintf(error, sizeof(error), "Could not scan the PS3 disc folders.");
            break;
        case LOBBY_WORK_LEAVE:
            rc = cb_online_leave_room(&task->session, error, sizeof(error));
            break;
        default:
            snprintf(error, sizeof(error), "Unknown lobby task.");
            rc = -1;
            break;
        }
        task->result = rc;
        if (rc == 0) {
            if (task->operation == LOBBY_WORK_GET_ROOM)
                snprintf(task->status, sizeof(task->status), "Room refreshed.");
            else if (task->operation == LOBBY_WORK_SET_READY)
                snprintf(task->status, sizeof(task->status), "Ready status updated.");
            else if (task->operation == LOBBY_WORK_HEARTBEAT)
                snprintf(task->status, sizeof(task->status), "Room connection active.");
            else if (task->operation == LOBBY_WORK_SET_DISC ||
                     task->operation == LOBBY_WORK_CONFIRM_DISC)
                snprintf(task->status, sizeof(task->status), "Disc confirmation updated.");
            else if (task->operation == LOBBY_WORK_CLEAR_DISC)
                snprintf(task->status, sizeof(task->status), "Disc confirmation cleared.");
            else if (task->operation == LOBBY_WORK_MARK_STARTED)
                snprintf(task->status, sizeof(task->status), "Room locked for Netplay launch.");
            else if (task->operation == LOBBY_WORK_CANCEL_STARTED)
                snprintf(task->status, sizeof(task->status), "Room launch lock released.");
            else if (task->operation == LOBBY_WORK_NETPLAY_ENDPOINT)
                snprintf(task->status, sizeof(task->status), "Netplay endpoint checked.");
            else if (task->operation == LOBBY_WORK_SCAN_DISCS)
                snprintf(task->status, sizeof(task->status), "Found %d CUE file(s).",
                         (int)task->disc_count);
            else if (task->operation == LOBBY_WORK_LEAVE)
                snprintf(task->status, sizeof(task->status), "Room left.");
            else if (task->operation == LOBBY_WORK_CREATE)
                snprintf(task->status, sizeof(task->status), "%s room %.6s created.",
                         task->is_private ? "Private" : "Public", task->session.code);
            else if (task->operation == LOBBY_WORK_JOIN_ANY)
                snprintf(task->status, sizeof(task->status), "Joined public battle %.6s.",
                         task->session.code);
            else
                snprintf(task->status, sizeof(task->status), "Joined room %.6s.",
                         task->session.code);
        } else {
            snprintf(task->status, sizeof(task->status), "%s",
                     error[0] ? error : "Room request failed.");
        }
    }

    __sync_synchronize();
    task->done = 1;
    sysThreadExit(0);
}

static int start_lobby_action_worker(sys_ppu_thread_t *thread,
                                     LobbyWorkerTask *task, int operation,
                                     const CBOnlineSession *session,
                                     const CBOnlineRoomView *view,
                                     const char *guest_name,
                                     const char *room_code,
                                     int ready, int is_private)
{
    if (!thread || !task) return -1;
    memset(task, 0, sizeof(*task));
    task->operation = operation;
    if (session) task->session = *session;
    if (view) task->view = *view;
    if (guest_name)
        snprintf(task->guest_name, sizeof(task->guest_name), "%.20s", guest_name);
    if (room_code)
        snprintf(task->room_code, sizeof(task->room_code), "%.6s", room_code);
    task->ready = ready;
    task->is_private = is_private;
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

static int start_lobby_fingerprint_worker(sys_ppu_thread_t *thread,
                                         LobbyWorkerTask *task,
                                         const CBOnlineSession *session,
                                         const CBOnlineRoomView *view,
                                         const char *disc_path)
{
    if (!thread || !task || !session || !view || !disc_path || !disc_path[0] ||
        strlen(disc_path) >= CB_DISC_PATH_MAX) return -1;
    memset(task, 0, sizeof(*task));
    task->operation = LOBBY_WORK_CONFIRM_DISC;
    task->session = *session;
    task->view = *view;
    snprintf(task->disc_path, sizeof(task->disc_path), "%s", disc_path);
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

static int start_lobby_disc_scan_worker(sys_ppu_thread_t *thread,
                                        LobbyWorkerTask *task,
                                        int open_selector)
{
    if (!thread || !task) return -1;
    memset(task, 0, sizeof(*task));
    task->operation = LOBBY_WORK_SCAN_DISCS;
    task->scan_open_selector = open_selector;
    if (sysThreadCreate(thread, lobby_worker, task, 1500,
                        LOBBY_WORKER_STACK_SIZE, THREAD_JOINABLE,
                        g_lobby_worker_name) != 0) {
        task->result = -1;
        snprintf(task->status, sizeof(task->status),
                 "Could not start the disc scan worker.");
        return -1;
    }
    return 0;
}

static int start_lobby_worker(sys_ppu_thread_t *thread,
                              LobbyWorkerTask *task, int operation)
{
    return start_lobby_action_worker(thread, task, operation,
                                     NULL, NULL, NULL, NULL, 0, 0);
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
        draw_text(82, 214, 15, 0x61d9baff, "CROSS or TRIANGLE creates a room. L1 switches its privacy.");
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
    if (count == 0)
        draw_text(56, 474, 12, 0x6d8794ff, "CROSS CREATE  TRIANGLE CREATE  L1 PRIVACY  R1 CODE  L2 ANY BATTLE  SQUARE REFRESH");
    else
        draw_text(56, 474, 12, 0x6d8794ff, "UP/DOWN SELECT  CROSS JOIN  TRIANGLE CREATE  L1 PRIVACY  R1 CODE  L2 ANY BATTLE  SQUARE REFRESH");
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

static int player_has_disc_confirmation(const CBOnlineSession *session,
                                          const CBOnlineRoomView *view)
{
    int i;
    if (!session || !view || view->game_started || !session->player_id[0]) return 0;
    for (i = 0; i < view->player_count; i++) {
        if (strcmp(view->players[i].id, session->player_id) == 0)
            return view->players[i].disc_fingerprint[0] != '\0';
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
        if (is_host) snprintf(output, output_size, "CB-%.16s-HOST", room_id);
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
    int pending_disc_action = DISC_ACTION_NONE;
    int pending_disc_index = -1;
    char pending_launch_error[160] = "";
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
            int completed_operation = worker.operation;
            join_lobby_worker(worker_thread);
            worker_active = 0;

            if (completed_operation == LOBBY_WORK_LIST) {
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
            } else if (completed_operation == LOBBY_WORK_SCAN_DISCS) {
                if (worker.result == 0) {
                    memcpy(discs, worker.discs, sizeof(discs));
                    disc_count = (int)worker.disc_count;
                    if (worker.scan_open_selector) {
                        disc_selected = 0;
                        netplay_waiting = 0;
                        netplay_wait_attempts = 0;
                        netplay_poll_frames = 0;
                        if (disc_count > 0) {
                            screen = SCREEN_DISC_SELECT;
                            snprintf(status, status_size,
                                     "Select the same Crash Bash CUE file on every console.");
                        } else {
                            disc_selected = 0;
                            snprintf(status, status_size,
                                     "Copy the Crash Bash CUE file to a PSXISO folder, then rescan.");
                        }
                    } else {
                        if (disc_count == 0) disc_selected = 0;
                        else if (disc_selected >= disc_count) disc_selected = disc_count - 1;
                        snprintf(status, status_size, "%s", worker.status);
                    }
                } else {
                    if (worker.scan_open_selector) disc_count = 0;
                    snprintf(status, status_size, "%s",
                             worker.status[0] ? worker.status : "Disc scan failed.");
                }
            } else if (completed_operation == LOBBY_WORK_JOIN_ANY ||
                       completed_operation == LOBBY_WORK_JOIN ||
                       completed_operation == LOBBY_WORK_CREATE) {
                if (worker.result == 0) {
                    session = worker.session;
                    view = worker.view;
                    ready = session.ready;
                    screen = SCREEN_ROOM;
                    poll_frames = 0;
                    room_list_poll_frames = 0;
                    snprintf(status, status_size, "%s", worker.status);
                } else {
                    snprintf(status, status_size, "%s", worker.status);
                }
            } else if (completed_operation == LOBBY_WORK_SET_READY) {
                if (worker.result == 0) {
                    session = worker.session;
                    view = worker.view;
                    ready = session.ready;
                    snprintf(status, status_size, "%s",
                             ready ? "You are ready." : "Ready status cancelled.");
                } else if (session_is_gone(worker.status)) {
                    return_to_rooms(&session, &view, &ready, &screen, &poll_frames,
                                    rooms, &room_count, &selected, status, status_size);
                } else {
                    snprintf(status, status_size, "Ready update: %.120s", worker.status);
                }
            } else if (completed_operation == LOBBY_WORK_GET_ROOM ||
                       completed_operation == LOBBY_WORK_HEARTBEAT) {
                if (worker.result == 0) {
                    session = worker.session;
                    view = worker.view;
                    ready = session.ready;
                    if (completed_operation == LOBBY_WORK_GET_ROOM &&
                        screen == SCREEN_DISC_SELECT && host_disc_waiting) {
                        if (!view.is_ready) {
                            host_disc_waiting = 0;
                            screen = SCREEN_ROOM;
                            snprintf(status, status_size,
                                     "Room changed. All active players must be READY again.");
                        } else if (view.disc_match) {
                            host_disc_waiting = 0;
                            if (start_lobby_action_worker(&worker_thread, &worker,
                                                          LOBBY_WORK_MARK_STARTED,
                                                          &session, &view, NULL, NULL, 0, 0) == 0) {
                                worker_active = 1;
                                snprintf(status, status_size, "All discs match. Locking the room...");
                            } else {
                                snprintf(status, status_size, "%s", worker.status);
                            }
                        } else if (view.disc_mismatch) {
                            snprintf(status, status_size,
                                     "Disc mismatch. Select identical CUE content on every console.");
                        } else {
                            snprintf(status, status_size,
                                     "Disc confirmation: %d/%d players. Waiting...",
                                     view.disc_selection_count, view.player_count);
                        }
                    } else if (completed_operation == LOBBY_WORK_GET_ROOM) {
                        if (pending_launch_error[0]) {
                            snprintf(status, status_size, "%.120s", pending_launch_error);
                            pending_launch_error[0] = '\0';
                        } else {
                            snprintf(status, status_size, "Room refreshed.");
                        }
                    }
                } else if (session_is_gone(worker.status)) {
                    host_disc_waiting = 0;
                    netplay_waiting = 0;
                    return_to_rooms(&session, &view, &ready, &screen, &poll_frames,
                                    rooms, &room_count, &selected, status, status_size);
                } else if (completed_operation == LOBBY_WORK_HEARTBEAT) {
                    snprintf(status, status_size, "Heartbeat: %.120s", worker.status);
                } else {
                    snprintf(status, status_size, "Room refresh: %.120s", worker.status);
                }
            } else if (completed_operation == LOBBY_WORK_CONFIRM_DISC) {
                if (worker.result == 0) {
                    int is_host;
                    session = worker.session;
                    view = worker.view;
                    ready = session.ready;
                    is_host = session_is_room_host(&session, &view);
                    if (view.disc_mismatch) {
                        netplay_waiting = 0;
                        host_disc_waiting = is_host;
                        host_disc_selected = disc_selected;
                        disc_poll_frames = 0;
                        snprintf(status, status_size,
                                 "Disc mismatch. All players must select identical CUE content.");
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
                            snprintf(status, status_size,
                                     "Disc confirmed. Waiting for all players and the host...");
                        }
                    } else if (is_host) {
                        host_disc_waiting = 0;
                        host_disc_selected = disc_selected;
                        if (start_lobby_action_worker(&worker_thread, &worker,
                                                      LOBBY_WORK_MARK_STARTED,
                                                      &session, &view, NULL, NULL, 0, 0) == 0) {
                            worker_active = 1;
                            snprintf(status, status_size, "All discs match. Locking the room...");
                        } else {
                            snprintf(status, status_size, "%s", worker.status);
                        }
                    } else {
                        host_disc_waiting = 0;
                        netplay_waiting = 1;
                        netplay_wait_attempts = 0;
                        netplay_poll_frames = NETPLAY_DISCOVERY_POLL_FRAMES - 1;
                        snprintf(status, status_size,
                                 "Disc content matches. Waiting for the host to start...");
                    }
                } else if (session_is_gone(worker.status)) {
                    host_disc_waiting = 0;
                    netplay_waiting = 0;
                    return_to_rooms(&session, &view, &ready, &screen, &poll_frames,
                                    rooms, &room_count, &selected, status, status_size);
                } else {
                    snprintf(status, status_size, "Disc confirmation: %.110s", worker.status);
                }
            } else if (completed_operation == LOBBY_WORK_CLEAR_DISC) {
                int action = pending_disc_action;
                pending_disc_action = DISC_ACTION_NONE;
                if (worker.result == 0) {
                    session = worker.session;
                    view = worker.view;
                    ready = session.ready;
                    if (action == DISC_ACTION_SELECT) {
                        disc_selected = pending_disc_index;
                        netplay_waiting = 0;
                        host_disc_waiting = 0;
                        host_disc_selected = -1;
                        snprintf(status, status_size,
                                 "Selection changed. Press CROSS to confirm this disc.");
                    } else if (action == DISC_ACTION_RESCAN) {
                        size_t scanned = 0;
                        netplay_waiting = 0;
                        host_disc_waiting = 0;
                        host_disc_selected = -1;
                        if (start_lobby_disc_scan_worker(&worker_thread, &worker, 0) == 0) {
                            worker_active = 1;
                            snprintf(status, status_size, "Scanning PSXISO folders...");
                        } else {
                            snprintf(status, status_size, "%s", worker.status);
                        }
                    } else if (action == DISC_ACTION_BACK) {
                        netplay_waiting = 0;
                        host_disc_waiting = 0;
                        host_disc_selected = -1;
                        screen = SCREEN_ROOM;
                    }
                } else if (session_is_gone(worker.status)) {
                    return_to_rooms(&session, &view, &ready, &screen, &poll_frames,
                                    rooms, &room_count, &selected, status, status_size);
                } else {
                    snprintf(status, status_size, "Could not clear disc confirmation: %.100s",
                             worker.status);
                }
                pending_disc_index = -1;
            } else if (completed_operation == LOBBY_WORK_MARK_STARTED) {
                if (worker.result == 0) {
                    int launch_disc = host_disc_selected >= 0 && host_disc_selected < disc_count
                        ? host_disc_selected : disc_selected;
                    int launch_result;
                    session = worker.session;
                    view = worker.view;
                    ready = session.ready;
                    host_disc_waiting = 0;
                    if (launch_disc < 0 || launch_disc >= disc_count) {
                        snprintf(status, status_size, "Selected Crash Bash disc is no longer available.");
                    } else {
                        make_netplay_nickname(netplay_nickname, sizeof(netplay_nickname),
                                              &session, &view, 1);
                        launch_result = cb_launch_game(discs[launch_disc].path, 1,
                                                       netplay_nickname, NULL,
                                                       error, sizeof(error));
                        if (launch_result < 0) {
                            snprintf(pending_launch_error, sizeof(pending_launch_error),
                                     "%.150s", error);
                            if (start_lobby_action_worker(&worker_thread, &worker,
                                                          LOBBY_WORK_CANCEL_STARTED,
                                                          &session, &view, NULL, NULL, 0, 0) == 0) {
                                worker_active = 1;
                                snprintf(status, status_size,
                                         "Game did not start. Releasing the room lock...");
                            } else {
                                snprintf(status, status_size,
                                         "Launch failed: %.90s / room unlock could not be started.",
                                         pending_launch_error);
                                pending_launch_error[0] = '\0';
                            }
                        }
                    }
                } else if (session_is_gone(worker.status)) {
                    return_to_rooms(&session, &view, &ready, &screen, &poll_frames,
                                    rooms, &room_count, &selected, status, status_size);
                } else {
                    snprintf(status, status_size, "Could not lock the room for launch: %.100s",
                             worker.status);
                }
            } else if (completed_operation == LOBBY_WORK_CANCEL_STARTED) {
                session = worker.session;
                if (worker.result == 0) {
                    if (start_lobby_action_worker(&worker_thread, &worker,
                                                  LOBBY_WORK_GET_ROOM,
                                                  &session, &view, NULL, NULL, 0, 0) == 0) {
                        worker_active = 1;
                        snprintf(status, status_size, "Room unlocked. Refreshing room state...");
                    } else {
                        snprintf(status, status_size, "%.120s", pending_launch_error);
                        pending_launch_error[0] = '\0';
                    }
                } else {
                    snprintf(status, status_size, "Launch failed: %.52s / unlock failed: %.52s",
                             pending_launch_error, worker.status);
                    pending_launch_error[0] = '\0';
                }
            } else if (completed_operation == LOBBY_WORK_NETPLAY_ENDPOINT) {
                session = worker.session;
                endpoint = worker.endpoint;
                if (worker.result == 0) {
                    netplay_waiting = 0;
                    make_netplay_nickname(netplay_nickname, sizeof(netplay_nickname),
                                          &session, &view, 0);
                    snprintf(status, status_size, "Host found: %.42s / %.32s. Connecting...",
                             endpoint.game_name, endpoint.core_name);
                    if (cb_launch_game(discs[disc_selected].path, 0, netplay_nickname,
                                       &endpoint, error, sizeof(error)) < 0) {
                        snprintf(status, status_size, "%.120s", error);
                    }
                } else if (worker.result == 1) {
                    if (strcmp(endpoint.state, "disc_mismatch") == 0) {
                        netplay_waiting = 0;
                        snprintf(status, status_size,
                                 "Disc mismatch. Select the same CUE content as the other players.");
                    } else if (strcmp(endpoint.state, "host_password_required") == 0) {
                        netplay_waiting = 0;
                        snprintf(status, status_size,
                                 "Host Netplay is password-protected. Disable the password, then press CROSS to retry.");
                    } else {
                        netplay_wait_attempts++;
                        if (netplay_wait_attempts >= NETPLAY_DISCOVERY_MAX_ATTEMPTS) {
                            netplay_waiting = 0;
                            if (strcmp(endpoint.state, "waiting_for_disc_selection") == 0) {
                                snprintf(status, status_size,
                                         "Timed out waiting for every player to confirm the same disc.");
                            } else if (strcmp(endpoint.state, "waiting_for_host_start") == 0) {
                                snprintf(status, status_size,
                                         "The host has not started Netplay yet. Ask the host to start the match.");
                            } else if (strcmp(endpoint.state, "host_not_connectable") == 0) {
                                snprintf(status, status_size,
                                         "Host is not connectable. Check NAT, relay and host network settings.");
                            } else {
                                snprintf(status, status_size,
                                         "Host did not appear in the RetroArch lobby. Check Netplay announcement and network.");
                            }
                        } else if (strcmp(endpoint.state, "waiting_for_disc_selection") == 0) {
                            snprintf(status, status_size,
                                     "Waiting for all players to confirm the same disc...");
                        } else if (strcmp(endpoint.state, "waiting_for_host_start") == 0) {
                            snprintf(status, status_size,
                                     "Disc confirmed. Waiting for the host to start Netplay...");
                        } else if (strcmp(endpoint.state, "host_not_connectable") == 0) {
                            snprintf(status, status_size,
                                     "Host is visible but not connectable. Checking relay... %d/%d",
                                     netplay_wait_attempts, NETPLAY_DISCOVERY_MAX_ATTEMPTS);
                        } else {
                            snprintf(status, status_size,
                                     "Waiting for host announcement... %d/%d",
                                     netplay_wait_attempts, NETPLAY_DISCOVERY_MAX_ATTEMPTS);
                        }
                    }
                } else if (session_is_gone(worker.status)) {
                    netplay_waiting = 0;
                    return_to_rooms(&session, &view, &ready, &screen, &poll_frames,
                                    rooms, &room_count, &selected, status, status_size);
                } else if (strstr(worker.status, "Every active player must be READY") != NULL) {
                    netplay_waiting = 0;
                    screen = SCREEN_ROOM;
                    snprintf(status, status_size,
                             "Room is no longer READY. Every active player must press CROSS again.");
                } else {
                    netplay_wait_attempts++;
                    snprintf(status, status_size, "Host lookup retry %d: %.90s",
                             netplay_wait_attempts, worker.status);
                    if (netplay_wait_attempts >= NETPLAY_DISCOVERY_MAX_ATTEMPTS) {
                        netplay_waiting = 0;
                        snprintf(status, status_size,
                                 "Could not locate the host. Check the network and try again.");
                    }
                }
            } else if (completed_operation == LOBBY_WORK_LEAVE) {
                memset(&session, 0, sizeof(session));
                memset(&view, 0, sizeof(view));
                ready = 0;
                screen = SCREEN_ROOMS;
                poll_frames = 0;
                room_list_poll_frames = 0;
                if (!exit_after_worker) {
                    if (start_lobby_worker(&worker_thread, &worker, LOBBY_WORK_LIST) == 0) {
                        worker_active = 1;
                        snprintf(status, status_size, "Refreshing public rooms...");
                    } else {
                        snprintf(status, status_size, "%s", worker.status);
                    }
                }
            }
            if (exit_after_worker) break;
        }

        if (worker_active && worker.operation != LOBBY_WORK_LIST) {
            if (circle && !old_circle) exit_after_worker = 1;
            if (screen == SCREEN_ROOMS)
                draw_rooms(rooms, room_count, selected, private_create, status);
            else if (screen == SCREEN_JOIN_CODE)
                draw_join_code(join_code, code_cursor, status);
            else if (screen == SCREEN_DISC_SELECT)
                draw_disc_select(discs, disc_count, disc_selected, &session,
                                 session_is_room_host(&session, &view), status);
            else
                draw_room(&view, &session, status);
            old_up = up; old_down = down; old_left = left; old_right = right;
            old_cross = cross; old_circle = circle; old_triangle = triangle; old_square = square;
            old_l1 = l1; old_l2 = l2; old_r1 = r1; old_start = start;
            continue;
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
                if (start_lobby_action_worker(&worker_thread, &worker, LOBBY_WORK_JOIN_ANY,
                                              NULL, NULL, guest_name, NULL, 0, 0) == 0) {
                    worker_active = 1;
                    snprintf(status, status_size, "Joining a public battle...");
                } else snprintf(status, status_size, "%s", worker.status);
            }
            if (cross && !old_cross && !worker_active) {
                if (room_count > 0) {
                    if (start_lobby_action_worker(&worker_thread, &worker, LOBBY_WORK_JOIN,
                                                  NULL, NULL, guest_name, rooms[selected].code, 0, 0) == 0) {
                        worker_active = 1;
                        snprintf(status, status_size, "Joining room %.6s...", rooms[selected].code);
                    } else snprintf(status, status_size, "%s", worker.status);
                } else {
                    if (start_lobby_action_worker(&worker_thread, &worker, LOBBY_WORK_CREATE,
                                                  NULL, NULL, guest_name, NULL, 0, private_create) == 0) {
                        worker_active = 1;
                        snprintf(status, status_size, "Creating %s room...",
                                 private_create ? "private" : "public");
                    } else snprintf(status, status_size, "%s", worker.status);
                }
            }
            if (triangle && !old_triangle && !worker_active) {
                if (start_lobby_action_worker(&worker_thread, &worker, LOBBY_WORK_CREATE,
                                              NULL, NULL, guest_name, NULL, 0, private_create) == 0) {
                    worker_active = 1;
                    snprintf(status, status_size, "Creating %s room...",
                             private_create ? "private" : "public");
                } else snprintf(status, status_size, "%s", worker.status);
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
                if (start_lobby_action_worker(&worker_thread, &worker, LOBBY_WORK_JOIN,
                                              NULL, NULL, guest_name, join_code, 0, 0) == 0) {
                    worker_active = 1;
                    snprintf(status, status_size, "Joining room %.6s...", join_code);
                } else snprintf(status, status_size, "%s", worker.status);
            }
            if (circle && !old_circle) screen = SCREEN_ROOMS;
            draw_join_code(join_code, code_cursor, status);
        } else if (screen == SCREEN_DISC_SELECT) {
            if (up && !old_up && disc_count > 0 && !worker_active) {
                int next_selection = (disc_selected + disc_count - 1) % disc_count;
                if (player_has_disc_confirmation(&session, &view)) {
                    pending_disc_action = DISC_ACTION_SELECT;
                    pending_disc_index = next_selection;
                    if (start_lobby_action_worker(&worker_thread, &worker,
                                                  LOBBY_WORK_CLEAR_DISC,
                                                  &session, &view, NULL, NULL, 0, 0) == 0) {
                        worker_active = 1;
                        snprintf(status, status_size, "Clearing previous disc confirmation...");
                    } else {
                        pending_disc_action = DISC_ACTION_NONE;
                        pending_disc_index = -1;
                        snprintf(status, status_size, "%s", worker.status);
                    }
                } else {
                    disc_selected = next_selection;
                    netplay_waiting = 0;
                    host_disc_waiting = 0;
                    host_disc_selected = -1;
                    snprintf(status, status_size,
                             "Selection changed. Press CROSS to confirm this disc.");
                }
            }
            if (down && !old_down && disc_count > 0 && !worker_active) {
                int next_selection = (disc_selected + 1) % disc_count;
                if (player_has_disc_confirmation(&session, &view)) {
                    pending_disc_action = DISC_ACTION_SELECT;
                    pending_disc_index = next_selection;
                    if (start_lobby_action_worker(&worker_thread, &worker,
                                                  LOBBY_WORK_CLEAR_DISC,
                                                  &session, &view, NULL, NULL, 0, 0) == 0) {
                        worker_active = 1;
                        snprintf(status, status_size, "Clearing previous disc confirmation...");
                    } else {
                        pending_disc_action = DISC_ACTION_NONE;
                        pending_disc_index = -1;
                        snprintf(status, status_size, "%s", worker.status);
                    }
                } else {
                    disc_selected = next_selection;
                    netplay_waiting = 0;
                    host_disc_waiting = 0;
                    host_disc_selected = -1;
                    snprintf(status, status_size,
                             "Selection changed. Press CROSS to confirm this disc.");
                }
            }
            if (square && !old_square && !worker_active) {
                if (player_has_disc_confirmation(&session, &view)) {
                    pending_disc_action = DISC_ACTION_RESCAN;
                    if (start_lobby_action_worker(&worker_thread, &worker,
                                                  LOBBY_WORK_CLEAR_DISC,
                                                  &session, &view, NULL, NULL, 0, 0) == 0) {
                        worker_active = 1;
                        snprintf(status, status_size, "Clearing disc confirmation before rescan...");
                    } else {
                        pending_disc_action = DISC_ACTION_NONE;
                        snprintf(status, status_size, "%s", worker.status);
                    }
                } else {
                    netplay_waiting = 0;
                    host_disc_waiting = 0;
                    host_disc_selected = -1;
                    if (start_lobby_disc_scan_worker(&worker_thread, &worker, 0) == 0) {
                        worker_active = 1;
                        snprintf(status, status_size, "Scanning PSXISO folders...");
                    } else {
                        snprintf(status, status_size, "%s", worker.status);
                    }
                }
            }
            if (cross && !old_cross && disc_count > 0 && !worker_active) {
                if (!view.is_ready) {
                    screen = SCREEN_ROOM;
                    snprintf(status, status_size,
                             "Room changed. All active players must be READY again.");
                } else {
                    char fingerprint[17];
                    if (start_lobby_fingerprint_worker(&worker_thread, &worker,
                                                       &session, &view,
                                                       discs[disc_selected].path) == 0) {
                        worker_active = 1;
                        snprintf(status, status_size, "Reading and confirming selected disc...");
                    } else {
                        snprintf(status, status_size, "%s", worker.status);
                    }
                }
            }
            if (circle && !old_circle && !worker_active) {
                if (player_has_disc_confirmation(&session, &view)) {
                    pending_disc_action = DISC_ACTION_BACK;
                    if (start_lobby_action_worker(&worker_thread, &worker,
                                                  LOBBY_WORK_CLEAR_DISC,
                                                  &session, &view, NULL, NULL, 0, 0) == 0) {
                        worker_active = 1;
                        snprintf(status, status_size, "Clearing disc confirmation...");
                    } else {
                        pending_disc_action = DISC_ACTION_NONE;
                        snprintf(status, status_size, "%s", worker.status);
                    }
                } else {
                    netplay_waiting = 0;
                    host_disc_waiting = 0;
                    host_disc_selected = -1;
                    screen = SCREEN_ROOM;
                }
            }
            draw_disc_select(discs, disc_count, disc_selected, &session,
                             session_is_room_host(&session, &view), status);
        } else {
            if (cross && !old_cross && !worker_active) {
                int next_ready = !ready;
                if (start_lobby_action_worker(&worker_thread, &worker, LOBBY_WORK_SET_READY,
                                              &session, &view, NULL, NULL, next_ready, 0) == 0) {
                    worker_active = 1;
                    snprintf(status, status_size, "Updating READY status...");
                } else snprintf(status, status_size, "%s", worker.status);
            }
            if (start && !old_start && screen == SCREEN_ROOM) {
                if (!view.is_ready) {
                    snprintf(status, status_size, "At least two players must join and all must be READY.");
                } else {
                    if (start_lobby_disc_scan_worker(&worker_thread, &worker, 1) == 0) {
                        worker_active = 1;
                        snprintf(status, status_size, "Scanning PSXISO folders...");
                    } else {
                        snprintf(status, status_size, "%s", worker.status);
                    }
                }
            }
            if (square && !old_square && screen == SCREEN_ROOM && !worker_active) {
                if (start_lobby_action_worker(&worker_thread, &worker, LOBBY_WORK_GET_ROOM,
                                              &session, &view, NULL, NULL, 0, 0) == 0) {
                    worker_active = 1;
                    snprintf(status, status_size, "Refreshing room...");
                } else snprintf(status, status_size, "%s", worker.status);
            }
            if (circle && !old_circle && !worker_active) {
                if (start_lobby_action_worker(&worker_thread, &worker, LOBBY_WORK_LEAVE,
                                              &session, &view, NULL, NULL, 0, 0) == 0) {
                    worker_active = 1;
                    snprintf(status, status_size, "Leaving battle room...");
                } else snprintf(status, status_size, "%s", worker.status);
            }
            if ((screen == SCREEN_ROOM || screen == SCREEN_DISC_SELECT) &&
                ++poll_frames >= ROOM_HEARTBEAT_FRAMES) {
                poll_frames = 0;
                if (!worker_active) {
                    if (start_lobby_action_worker(&worker_thread, &worker, LOBBY_WORK_HEARTBEAT,
                                                  &session, &view, NULL, NULL, 0, 0) == 0) {
                        worker_active = 1;
                        snprintf(status, status_size, "Checking room connection...");
                    } else snprintf(status, status_size, "%s", worker.status);
                }
            }
            if (screen == SCREEN_DISC_SELECT && host_disc_waiting &&
                ++disc_poll_frames >= NETPLAY_DISCOVERY_POLL_FRAMES && !worker_active) {
                disc_poll_frames = 0;
                if (start_lobby_action_worker(&worker_thread, &worker, LOBBY_WORK_GET_ROOM,
                                              &session, &view, NULL, NULL, 0, 0) == 0) {
                    worker_active = 1;
                    snprintf(status, status_size, "Checking player disc confirmations...");
                } else {
                    snprintf(status, status_size, "%s", worker.status);
                }
            }
            if (screen == SCREEN_DISC_SELECT && netplay_waiting &&
                ++netplay_poll_frames >= NETPLAY_DISCOVERY_POLL_FRAMES && !worker_active) {
                netplay_poll_frames = 0;
                if (!view.is_ready) {
                    netplay_waiting = 0;
                    snprintf(status, status_size,
                             "Room changed. All active players must be READY again.");
                    screen = SCREEN_ROOM;
                } else if (start_lobby_action_worker(&worker_thread, &worker,
                                                     LOBBY_WORK_NETPLAY_ENDPOINT,
                                                     &session, &view, NULL, NULL, 0, 0) == 0) {
                    worker_active = 1;
                    snprintf(status, status_size, "Looking for the host Netplay session...");
                } else {
                    snprintf(status, status_size, "%s", worker.status);
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
                               "Finishing the pending room request...", "", 0);
        }
        join_lobby_worker(worker_thread);
        worker_active = 0;
        if (worker.operation != LOBBY_WORK_LIST &&
            worker.operation != LOBBY_WORK_LEAVE) {
            session = worker.session;
            view = worker.view;
        } else if (worker.operation == LOBBY_WORK_LEAVE) {
            memset(&session, 0, sizeof(session));
            memset(&view, 0, sizeof(view));
        }
        if (worker.curl_initialized) curl_initialized = 1;
    }
    if (session.token[0] && curl_initialized) {
        if (start_lobby_action_worker(&worker_thread, &worker, LOBBY_WORK_LEAVE,
                                      &session, &view, NULL, NULL, 0, 0) == 0) {
            while (!worker.done) {
                sysUtilCheckCallback();
                draw_lobby_message("LEAVING BATTLE ROOM",
                                   "Closing your online room session...", "", 0);
            }
            join_lobby_worker(worker_thread);
        }
    }
    if (curl_initialized) cb_online_shutdown();
    return 0;
}
