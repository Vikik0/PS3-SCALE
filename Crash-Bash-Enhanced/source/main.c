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

#define MENU_COUNT 5
static volatile int g_exit_requested = 0;
static u32 *g_font_texture = NULL;
static int g_selected = 0;
static int g_network_state = 0;
static int g_screen = 0;
static int g_demo_x = 410, g_demo_y = 270;
static char g_guest_name[32] = "Guest-00000000";
static char g_status[120] = "Ready. PSN is not required.";
static const char *g_menu[MENU_COUNT] = {
    "GUEST PROFILE", "NETWORK CHECK", "ONLINE ROOMS",
    "OFFLINE ARENA TEST", "EXIT"
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
    DrawString(x, y, value ? value : "");
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
    text(472, 352, 15, 0x9bb4c2ff, "CLOUD SERVICE: NOT LINKED");
    rect(58, 438, 680, 32, 0x0b1720ff);
    text(72, 444, 13, 0x9bb4c2ff, g_status);
    text(60, 474, 12, 0x6d8794ff, "CROSS: SELECT     UP/DOWN: MOVE     CIRCLE: BACK");
    text(666, 474, 12, 0x6d8794ff, "NO PSN");
    tiny3d_Flip();
}
static void draw_demo(void)
{
    tiny3d_Clear(0x071019ff, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();
    rect(24, 22, 800, 468, 0x101e2aff);
    rect(24, 22, 800, 5, 0x46d6b2ff);
    text(56, 44, 25, 0xf5fbffff, "TRAINING ARENA");
    text(56, 82, 14, 0x9bb4c2ff, "LOCAL INPUT TEST - NOT THE ORIGINAL GAME");
    rect(70, 125, 708, 300, 0x0b1720ff);
    rect((float)g_demo_x, (float)g_demo_y, 24, 24, 0x46d6b2ff);
    rect(500, 240, 16, 16, 0xff9a55ff);
    text(56, 474, 12, 0x6d8794ff, "DPAD: MOVE     CIRCLE: BACK");
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
    int rc = netInitialize();
    if (rc == 0) {
        g_network_state = 1;
        snprintf(g_status, sizeof(g_status),
                 "Network stack initialized; cloud matchmaking is not configured.");
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
        snprintf(g_status, sizeof(g_status),
                 "Cloud rooms are not linked yet; this build will not fake an online session.");
        break;
    case 3: g_screen = 1; break;
    default: g_exit_requested = 1; break;
    }
}
int main(int argc, char **argv)
{
    padInfo pad_info;
    padData pad_data;
    u16 old_up = 0, old_down = 0, old_cross = 0, old_circle = 0;
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
        u16 up = 0, down = 0, cross = 0, circle = 0;
        sysUtilCheckCallback();
        if (pad_ready && ioPadGetInfo(&pad_info) == 0) {
            int p;
            for (p = 0; p < MAX_PADS; p++) {
                if (!pad_info.status[p]) continue;
                if (ioPadGetData(p, &pad_data) == 0 && pad_data.len >= 8) {
                    up = pad_data.BTN_UP; down = pad_data.BTN_DOWN;
                    cross = pad_data.BTN_CROSS; circle = pad_data.BTN_CIRCLE;
                    break;
                }
            }
        }
        if (g_screen == 0) {
            if (up && !old_up) g_selected = (g_selected + MENU_COUNT - 1) % MENU_COUNT;
            if (down && !old_down) g_selected = (g_selected + 1) % MENU_COUNT;
            if (cross && !old_cross) activate_selection();
            draw_home();
        } else {
            if (pad_ready && ioPadGetInfo(&pad_info) == 0) {
                int p;
                for (p = 0; p < MAX_PADS; p++) {
                    if (!pad_info.status[p]) continue;
                    if (ioPadGetData(p, &pad_data) == 0 && pad_data.len >= 8) {
                        if (pad_data.BTN_LEFT && g_demo_x > 78) g_demo_x -= 3;
                        if (pad_data.BTN_RIGHT && g_demo_x < 748) g_demo_x += 3;
                        if (pad_data.BTN_UP && g_demo_y > 132) g_demo_y -= 3;
                        if (pad_data.BTN_DOWN && g_demo_y < 390) g_demo_y += 3;
                        break;
                    }
                }
            }
            if (circle && !old_circle) g_screen = 0;
            draw_demo();
        }
        old_up = up; old_down = down; old_cross = cross; old_circle = circle;
    }
    if (g_network_state == 1) netDeinitialize();
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    return 0;
}
