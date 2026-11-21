#include "relay_ui.h"

#include "input_hook.h"
#include "net_adapter.h"

#include <cell/msg_dialog.h>
#include <cell/sysmodule.h>
#include <cell/sysutil.h>
#include <cell/sysutil_oskdialog.h>
#include <sys/memory.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CB_RELAY_UI_CALLBACK_SLOT_PRIMARY 3
#define CB_RELAY_UI_OSK_HEAP_BYTES (2u * 1024u * 1024u)
#define CB_RELAY_UI_CHORD (CELL_PAD_CTRL_SELECT | CELL_PAD_CTRL_L3 | CELL_PAD_CTRL_R3)

static int ui_sysutil_loaded;
static int ui_chord_was_down;
static int ui_busy;
static int ui_host_room;
static int ui_callback_slot = -1;
static char ui_worker_host[128];
static char ui_worker_port[8] = "80";

static volatile int dialog_done;
static volatile int dialog_button;

static volatile int osk_unloaded;
static volatile int osk_finished;
static volatile int osk_result_code;
static volatile int osk_result_chars;
static uint16_t osk_result_buffer[CELL_OSKDIALOG_STRING_SIZE];

static void sysutil_event(uint64_t status, uint64_t param, void *userdata)
{
    (void)param;
    (void)userdata;
    if (status == CELL_SYSUTIL_REQUEST_EXITGAME) {
        dialog_done = 1;
        osk_unloaded = 1;
    }
}

static int register_sysutil_callback(void)
{
    static const int slots[] = { 3, 2, 1 };
    size_t i;

    if (!ui_sysutil_loaded) {
        (void)cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL);
        ui_sysutil_loaded = 1;
    }
    for (i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
        if (cellSysutilRegisterCallback(slots[i], sysutil_event, NULL) == 0) {
            ui_callback_slot = slots[i];
            return 0;
        }
    }
    ui_callback_slot = -1;
    return -1;
}

static void unregister_sysutil_callback(void)
{
    if (ui_callback_slot >= 0) {
        (void)cellSysutilUnregisterCallback(ui_callback_slot);
        ui_callback_slot = -1;
    }
}

static void dialog_button_callback(int button, void *userdata)
{
    (void)userdata;
    dialog_button = button;
    dialog_done = 1;
}

static int show_dialog(const char *message, int yes_no)
{
    unsigned int type = CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL |
                        CELL_MSGDIALOG_TYPE_BG_VISIBLE;
    int i;
    int result;

    if (register_sysutil_callback() != 0) return -1;
    dialog_done = 0;
    dialog_button = CELL_MSGDIALOG_BUTTON_INVALID;

    if (yes_no) {
        type |= CELL_MSGDIALOG_TYPE_BUTTON_TYPE_YESNO |
                CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_YES;
    } else {
        type |= CELL_MSGDIALOG_TYPE_BUTTON_TYPE_OK;
    }

    result = cellMsgDialogOpen2(type, message, dialog_button_callback, NULL, NULL);
    if (result != 0) {
        unregister_sysutil_callback();
        return -1;
    }

    for (i = 0; i < 600 && !dialog_done; ++i) {
        (void)cellSysutilCheckCallback();
        usleep(100000);
    }
    if (!dialog_done) {
        (void)cellMsgDialogAbort();
        unregister_sysutil_callback();
        return -1;
    }

    unregister_sysutil_callback();
    if (dialog_button == CELL_MSGDIALOG_BUTTON_ESCAPE) return -1;
    return dialog_button;
}

static size_t ascii_to_utf16(const char *input, uint16_t *output, size_t capacity)
{
    size_t i = 0;
    if (!output || capacity == 0) return 0;
    while (input && input[i] && i + 1u < capacity) {
        output[i] = (uint16_t)(unsigned char)input[i];
        ++i;
    }
    output[i] = 0;
    return i;
}

static int prompt_text(const char *message, const char *initial,
                       int max_chars, char *output, size_t output_capacity)
{
    sys_mem_container_t container = 0;
    CellOskDialogParam dialog_param;
    CellOskDialogInputFieldInfo field_info;
    uint16_t message_utf16[160];
    uint16_t initial_utf16[160];
    int callback_registered = 0;
    int loaded = 0;
    int result = -1;
    int i;
    int rc;

    if (!output || output_capacity == 0 || max_chars <= 0) return -1;
    output[0] = '\0';
    if (register_sysutil_callback() != 0) return -1;
    callback_registered = 1;

    rc = sysMemContainerCreate(&container, CB_RELAY_UI_OSK_HEAP_BYTES);
    if (rc != 0) goto cleanup;

    memset(&dialog_param, 0, sizeof(dialog_param));
    memset(&field_info, 0, sizeof(field_info));
    memset(osk_result_buffer, 0, sizeof(osk_result_buffer));
    ascii_to_utf16(message, message_utf16, sizeof(message_utf16) / sizeof(message_utf16[0]));
    ascii_to_utf16(initial ? initial : "", initial_utf16,
                   sizeof(initial_utf16) / sizeof(initial_utf16[0]));

    dialog_param.allowOskPanelFlg = CELL_OSKDIALOG_PANELMODE_ENGLISH |
                                    CELL_OSKDIALOG_PANELMODE_NUMERAL;
    dialog_param.firstViewPanel = CELL_OSKDIALOG_PANELMODE_ENGLISH;
    dialog_param.controlPoint.x = 0.0f;
    dialog_param.controlPoint.y = 0.0f;
    dialog_param.prohibitFlgs = CELL_OSKDIALOG_NO_RETURN;

    field_info.message = message_utf16;
    field_info.init_text = initial_utf16;
    field_info.limit_length = max_chars;

    (void)cellOskDialogSetLayoutMode(CELL_OSKDIALOG_LAYOUTMODE_X_ALIGN_CENTER |
                                     CELL_OSKDIALOG_LAYOUTMODE_Y_ALIGN_CENTER);
    (void)cellOskDialogSetInitialKeyLayout(CELL_OSKDIALOG_INITIAL_PANEL_LAYOUT_FULLKEY);
    (void)cellOskDialogSetInitialInputDevice(CELL_OSKDIALOG_INPUT_DEVICE_PAD);
    (void)cellOskDialogAddSupportLanguage(CELL_OSKDIALOG_PANELMODE_ENGLISH |
                                          CELL_OSKDIALOG_PANELMODE_NUMERAL);
    (void)cellOskDialogSetKeyLayoutOption(0);
    (void)cellOskDialogDisableDimmer();
    (void)cellOskDialogSetDeviceMask(CELL_OSKDIALOG_DEVICE_MASK_PAD);

    osk_unloaded = 0;
    osk_finished = 0;
    osk_result_code = CELL_OSKDIALOG_INPUT_FIELD_RESULT_ABORT;
    osk_result_chars = 0;

    rc = cellOskDialogLoadAsync(container, &dialog_param, &field_info);
    if (rc != 0) goto cleanup;
    loaded = 1;

    for (i = 0; i < 600 && !osk_unloaded; ++i) {
        (void)cellSysutilCheckCallback();
        usleep(100000);
    }
    if (!osk_unloaded) {
        (void)cellOskDialogAbort();
        for (i = 0; i < 30 && !osk_unloaded; ++i) {
            (void)cellSysutilCheckCallback();
            usleep(100000);
        }
        goto cleanup;
    }

    if (!osk_finished || osk_result_code != CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK) {
        result = 0; /* user cancelled */
        goto cleanup;
    }

    {
        size_t limit = osk_result_chars > 0
            ? (size_t)osk_result_chars : CELL_OSKDIALOG_STRING_SIZE;
        size_t out = 0;
        for (i = 0; (size_t)i < limit && i < CELL_OSKDIALOG_STRING_SIZE &&
                    osk_result_buffer[i] != 0 && out + 1u < output_capacity; ++i) {
            uint16_t ch = osk_result_buffer[i];
            if (ch > 0x7fu) {
                result = -1;
                goto cleanup;
            }
            output[out++] = (char)ch;
        }
        output[out] = '\0';
        result = out > 0 ? 1 : 0;
    }

cleanup:
    if (loaded && !osk_unloaded) {
        (void)cellOskDialogAbort();
    }
    if (container) (void)sysMemContainerDestroy(container);
    if (callback_registered) unregister_sysutil_callback();
    return result;
}

static int validate_worker_host(const char *host)
{
    size_t i;
    if (!host || !*host) return 0;
    for (i = 0; host[i]; ++i) {
        char c = host[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-')) return 0;
    }
    return i < sizeof(ui_worker_host);
}

static int get_worker_endpoint(void)
{
    char value[128];
    char port_text[8];
    char *end;
    unsigned long port;
    int answer;

    answer = prompt_text("Worker hostname or LAN IP (no http://)", ui_worker_host,
                         120, value, sizeof(value));
    if (answer <= 0) return answer;
    if (!validate_worker_host(value)) {
        (void)show_dialog("Invalid hostname. Use letters, numbers, dots and hyphens only.", 0);
        return -1;
    }

    answer = prompt_text("Worker port (local test often uses 8787)", ui_worker_port,
                         5, port_text, sizeof(port_text));
    if (answer <= 0) return answer;
    port = strtoul(port_text, &end, 10);
    if (end == port_text || *end != '\0' || port == 0 || port > 65535u) {
        (void)show_dialog("Invalid port number.", 0);
        return -1;
    }

    strcpy(ui_worker_host, value);
    snprintf(ui_worker_port, sizeof(ui_worker_port), "%lu", port);
    return 1;
}

static int open_relay_menu(void)
{
    char code[16];
    char message[192];
    int answer;

    if (cb_net_is_active()) return 0;

    if (ui_host_room) {
        answer = show_dialog("Start the Crash Bash match now? Another PS3 must have joined first.", 1);
        if (answer == CELL_MSGDIALOG_BUTTON_YES) {
            if (cb_net_relay_start() == 0) {
                ui_host_room = 0;
                return 1;
            }
            (void)show_dialog("Players are not ready yet. Wait for the other PS3 to join, then try again.", 0);
        }
        return 0;
    }

    if (show_dialog("Create a room? YES = Host a room, NO = Join with a room code.", 1)
        != CELL_MSGDIALOG_BUTTON_YES &&
        dialog_button != CELL_MSGDIALOG_BUTTON_NO) return 0;

    answer = get_worker_endpoint();
    if (answer <= 0) return 0;

    if (dialog_button == CELL_MSGDIALOG_BUTTON_YES) {
        if (cb_net_relay_host(ui_worker_host,
                              (unsigned short)strtoul(ui_worker_port, NULL, 10)) != 0) {
            (void)show_dialog("Could not create the room. Check the Worker host and port.", 0);
            return -1;
        }
        snprintf(code, sizeof(code), "%s", cb_net_join_code_text());
        snprintf(message, sizeof(message),
                 "Room created. Share this code with the other player:\n\n%s\n\nPress the menu buttons again when the player has joined.",
                 code);
        ui_host_room = 1;
        (void)show_dialog(message, 0);
        return 1;
    }

    answer = prompt_text("Enter the 6-character room code", "", 6, code, sizeof(code));
    if (answer <= 0) return 0;
    for (int i = 0; code[i]; ++i) {
        if (code[i] >= 'a' && code[i] <= 'z') code[i] = (char)(code[i] - 'a' + 'A');
        if (!((code[i] >= 'A' && code[i] <= 'Z') ||
              (code[i] >= '0' && code[i] <= '9'))) {
            (void)show_dialog("Room codes use six letters or numbers.", 0);
            return -1;
        }
    }
    if (strlen(code) != 6u ||
        cb_net_relay_join_code(ui_worker_host,
                               (unsigned short)strtoul(ui_worker_port, NULL, 10),
                               code) != 0) {
        (void)show_dialog("Could not join the room. Check the code and Worker endpoint.", 0);
        return -1;
    }
    (void)show_dialog("Joined the room. Wait for the host to start the match.", 0);
    return 1;
}

void cb_net_ui_poll(void)
{
    CbPadState physical;
    int chord_down;

    if (ui_busy || cb_net_is_active()) return;
    if (cb_input_read_physical(&physical) != 0) return;

    chord_down = (physical.digital1 & CB_RELAY_UI_CHORD) == CB_RELAY_UI_CHORD;
    if (chord_down && !ui_chord_was_down) {
        ui_busy = 1;
        (void)open_relay_menu();
        ui_busy = 0;
    }
    ui_chord_was_down = chord_down;
}
