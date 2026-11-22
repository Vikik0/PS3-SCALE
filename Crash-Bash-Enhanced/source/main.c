#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <tiny3d.h>
#include <libfont.h>
#include <ya2d/ya2d.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>
#include <net/net.h>
#include "font8x12.h"
#include "online_menu.h"
#include "game_launcher.h"
#include "disc_browser.h"

#define MENU_COUNT 5
static volatile int g_exit_requested = 0;
static u32 *g_font_texture = NULL;
static int g_selected = 0;
static int g_network_state = 0;
static int g_online_state = 0;
static char g_guest_name[32] = "Guest-00000000";
static char g_status[120] = "Ready. PSN is not required.";
static const char *g_menu[MENU_COUNT] = {
    "GUEST PROFILE", "NETWORK CHECK", "ONLINE ROOMS",
    "OPEN CRASH BASH ONLINE", "EXIT"
};

static void on_sysutil(u64 status, u64 param, void *userdata)
{
    (void)param; (void)userdata;
    if (status == SYSUTIL_EXIT_GAME) g_exit_requested = 1;
}
static void rect(float x, float y, float w, float h, u32 color)
{
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(x, y, 65535.0f);
    tiny3d_VertexColor(color);
    tiny3d_VertexPos(x + w, y, 65535.0f);
    tiny3d_VertexPos(x + w, y + h, 65535.0f);
    tiny3d_VertexPos(x, y + h, 65535.0f);
    tiny3d_End();
}
static void text(float x, float y, int size, u32 color, const char *value)
{
    SetCurrentFont(0);
    SetFontColor(color, 0x00000000);
    SetFontSize(size, size + 8);
    { char safe_text[256]; snprintf(safe_text, sizeof(safe_text), "%s", value ? value : ""); DrawString(x, y, safe_text); }
}
static void draw_home(void)
{
    int i;
    tiny3d_Clear(0x071019ff, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();
    rect(24, 22, 800, 468, 0x101e2aff);
    rect(24, 22, 800, 5, 0x46d6b2ff);
    rect(50, 55, 744, 94, 0x142a37ff);
    text(70, 70, 34, 0xf5fbffff, "CRASH BASH");
    text(70, 112, 18, 0x61d9baff, "ENHANCED  |  PS3 ONLINE PROJECT");
    text(60, 176, 17, 0x9bb4c2ff, "SELECT");
    for (i = 0; i < MENU_COUNT; i++) {
        u32 bg = (i == g_selected) ? 0x28594fff : 0x172a36ff;
        u32 fg = (i == g_selected) ? 0xffffffff : 0xc2d0d8ff;
        rect(58, 204 + i * 43, 360, 34, bg);
        text(76, 211 + i * 43, 16, fg, g_menu[i]);
    }
    rect(452, 176, 286, 232, 0x0b1720ff);
    text(472, 194, 18, 0x61d9baff, "PLAYER");
    text(472, 230, 16, 0xf0f6f8ff, g_guest_name);
    text(472, 274, 18, 0x61d9baff, "NETWORK");
    if (g_network_state == 1)
        text(472, 310, 16, 0x7ce0a8ff, "LOCAL STACK READY");
    else if (g_network_state < 0)
        text(472, 310, 16, 0xff9a80ff, "NETWORK INIT FAILED");
    else
        text(472, 310, 16, 0xb9c7ceff, "NOT TESTED");
    if (g_online_state == 1)
        text(472, 352, 15, 0x7ce0a8ff, "ROOM SERVICE OK - NO GAME SYNC");
    else if (g_online_state < 0)
        text(472, 352, 15, 0xff9a80ff, "CLOUD LOBBY OFFLINE");
    else
        text(472, 352, 15, 0x9bb4c2ff, "CLOUD NOT TESTED");
    rect(58, 438, 680, 32, 0x0b1720ff);
    text(72, 444, 13, 0x9bb4c2ff, g_status);
    text(60, 474, 12, 0x6d8794ff, "CROSS: SELECT     UP/DOWN: MOVE");
    text(666, 474, 12, 0x6d8794ff, "NO PSN");
    tiny3d_Flip();
}
static void make_guest_name(void)
{
    unsigned int seed = (unsigned int)(uintptr_t)&g_guest_name;
    seed ^= (unsigned int)rand();
    snprintf(g_guest_name, sizeof(g_guest_name), "Guest-%08X", seed);
}
static void check_network(void)
{
    int rc;
    if (g_network_state == 1) {
        snprintf(g_status, sizeof(g_status), "Network stack is already initialized.");
        return;
    }
    rc = netInitialize();
    if (rc == 0) {
        g_network_state = 1;
        snprintf(g_status, sizeof(g_status),
                 "Network stack initialized; open ONLINE ROOMS to check the cloud lobby.");
    } else {
        g_network_state = -1;
        snprintf(g_status, sizeof(g_status), "Network initialization failed: 0x%08X",
                 (unsigned int)rc);
    }
}
static void activate_selection(void)
{
    switch (g_selected) {
    case 0:
        make_guest_name();
        snprintf(g_status, sizeof(g_status), "Guest profile created locally. No PSN login used.");
        break;
    case 1: check_network(); break;
    case 2:
        cb_online_menu(g_guest_name, &g_network_state, &g_online_state,
                       &g_exit_requested, g_status, sizeof(g_status));
        break;
    case 3: {
        CBGameDisc discs[CB_DISC_MAX];
        size_t disc_count = 0;
        if (cb_disc_scan(discs, CB_DISC_MAX, &disc_count) != 0 || disc_count == 0) {
            snprintf(g_status, sizeof(g_status),
                     "No CUE found. Copy Crash Bash to a PSXISO folder.");
        } else {
            cb_launch_game(discs[0].path, 0, "CrashBashPlayer",
                           g_status, sizeof(g_status));
        }
        break;
    }
    default: g_exit_requested = 1; break;
    }
}
int main(int argc, char **argv)
{
    padInfo pad_info;
    padData pad_data;
    u16 old_up = 0, old_down = 0, old_cross = 0;
    int pad_ready = 0;
    (void)argc; (void)argv;

    if (!ya2d_init()) return 1;
    pad_ready = 1;
    g_font_texture = tiny3d_AllocTexture(512 * 1024);
    if (!g_font_texture) return 1;
    ResetFont();
    AddFontFromBitmapArray((u8 *)ps3scale_font8x12, (u8 *)g_font_texture,
                           32, 126, 8, 12, 1, BIT7_FIRST_PIXEL);
    SetCurrentFont(0);
    SetFontSize(18, 26);
    make_guest_name();
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, on_sysutil, NULL);

    while (!g_exit_requested) {
        u16 up = 0, down = 0, cross = 0;
        sysUtilCheckCallback();
        if (pad_ready && ioPadGetInfo(&pad_info) == 0) {
            int p;
            for (p = 0; p < MAX_PADS; p++) {
                if (!pad_info.status[p]) continue;
                if (ioPadGetData(p, &pad_data) == 0 && pad_data.len >= 8) {
                    up = pad_data.BTN_UP; down = pad_data.BTN_DOWN;
                    cross = pad_data.BTN_CROSS;
                    break;
                }
            }
        }
        if (up && !old_up) g_selected = (g_selected + MENU_COUNT - 1) % MENU_COUNT;
        if (down && !old_down) g_selected = (g_selected + 1) % MENU_COUNT;
        if (cross && !old_cross) activate_selection();
        draw_home();
        old_up = up; old_down = down; old_cross = cross;
    }
    if (g_network_state == 1) netDeinitialize();
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    ya2d_deinit();
    return 0;
}
