#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>

#include <tiny3d.h>
#include <libfont.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>
#include <ya2d/ya2d.h>

#include "font8x12.h"

#define APP_PLUGIN_SOURCE "/dev_hdd0/game/DSFMOD001/USRDIR/simple-cheat-menu.sprx"
#define PLUGIN_DIR        "/dev_hdd0/plugins"
#define PLUGIN_PATH       "/dev_hdd0/plugins/simple-cheat-menu.sprx"
#define PLUGIN_TEMP       "/dev_hdd0/plugins/simple-cheat-menu.sprx.dsf-tmp"
#define PLUGIN_BACKUP     "/dev_hdd0/plugins/simple-cheat-menu.sprx.dsf-backup"
#define BOOT_PATH         "/dev_hdd0/boot_plugins.txt"
#define BOOT_TEMP         "/dev_hdd0/boot_plugins.txt.dsf-tmp"
#define BOOT_BACKUP       "/dev_hdd0/boot_plugins.txt.dsf-backup"
#define SETTINGS_DIR      "/dev_hdd0/tmp/simple-cheat-menu"
#define SETTINGS_PATH     "/dev_hdd0/tmp/simple-cheat-menu/settings.txt"
#define BOOT_CAP          65536u
#define COPY_CAP          8192u

static volatile int g_exit_requested = 0;
static int g_selected = 0;
static char g_message[160] = "Choose INSTALL, REMOVE, STATUS or EXIT.";
static u32 *g_font_texture = NULL;

static void app_sysutil_callback(u64 status, u64 param, void *userdata)
{
    (void)param;
    (void)userdata;
    if (status == SYSUTIL_EXIT_GAME)
        g_exit_requested = 1;
}

static int file_exists(const char *path)
{
    FILE *fp;
    if (!path || !path[0]) return 0;
    fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

static int ensure_dir(const char *path)
{
    if (mkdir(path, 0777) == 0) return 0;
    if (errno == EEXIST) return 0;
    return -1;
}

static int copy_file(const char *source, const char *destination)
{
    FILE *in = NULL;
    FILE *out = NULL;
    unsigned char buffer[COPY_CAP];
    size_t count;
    int result = -1;

    in = fopen(source, "rb");
    if (!in) goto done;
    out = fopen(destination, "wb");
    if (!out) goto done;

    while ((count = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, count, out) != count)
            goto done;
    }
    if (ferror(in) || fflush(out) != 0)
        goto done;
    result = 0;

done:
    if (in) fclose(in);
    if (out && fclose(out) != 0) result = -1;
    if (result != 0) remove(destination);
    return result;
}

static int read_text_file(const char *path, char *buffer,
                          size_t capacity, size_t *length)
{
    FILE *fp;
    size_t count;

    if (!buffer || capacity < 2 || !length) return -1;
    *length = 0;
    buffer[0] = '\0';

    fp = fopen(path, "rb");
    if (!fp) return 0;

    count = fread(buffer, 1, capacity - 1, fp);
    if (ferror(fp)) {
        fclose(fp);
        return -1;
    }
    if (!feof(fp) && count == capacity - 1) {
        fclose(fp);
        return -2;
    }
    fclose(fp);
    buffer[count] = '\0';
    *length = count;
    return 0;
}

static int write_atomic(const char *path, const char *temp_path,
                        const char *data, size_t length)
{
    FILE *fp = fopen(temp_path, "wb");
    int failed = 0;

    if (!fp) return -1;
    if (length && fwrite(data, 1, length, fp) != length)
        failed = 1;
    if (fflush(fp) != 0)
        failed = 1;
    if (fclose(fp) != 0)
        failed = 1;
    if (failed) {
        remove(temp_path);
        return -1;
    }

    if (rename(temp_path, path) == 0) return 0;

    /* The previous file is backed up before this function is called. */
    if (remove(path) != 0 && file_exists(path)) {
        remove(temp_path);
        return -1;
    }
    if (rename(temp_path, path) != 0) {
        remove(temp_path);
        return -1;
    }
    return 0;
}

static int backup_once(const char *source, const char *backup)
{
    if (!file_exists(source) || file_exists(backup)) return 0;
    return copy_file(source, backup);
}

static int line_is_plugin(const char *line, size_t length)
{
    const size_t wanted = sizeof(PLUGIN_PATH) - 1;
    if (length && line[length - 1] == '\r') length--;
    return length == wanted && memcmp(line, PLUGIN_PATH, wanted) == 0;
}

static int boot_has_plugin_line(const char *data, size_t length)
{
    size_t pos = 0;
    while (pos < length) {
        size_t start = pos;
        while (pos < length && data[pos] != '\n') pos++;
        if (line_is_plugin(data + start, pos - start)) return 1;
        if (pos < length) pos++;
    }
    return 0;
}

static int boot_add_plugin_line(void)
{
    char data[BOOT_CAP];
    size_t length = 0;
    const size_t line_length = sizeof(PLUGIN_PATH) - 1;

    if (read_text_file(BOOT_PATH, data, sizeof(data), &length) != 0)
        return -1;
    if (boot_has_plugin_line(data, length))
        return 0;

    if (length + line_length + 2 >= sizeof(data))
        return -2;

    if (backup_once(BOOT_PATH, BOOT_BACKUP) != 0)
        return -3;

    if (length && data[length - 1] != '\n')
        data[length++] = '\n';
    memcpy(data + length, PLUGIN_PATH, line_length);
    length += line_length;
    data[length++] = '\n';
    data[length] = '\0';

    return write_atomic(BOOT_PATH, BOOT_TEMP, data, length);
}

static int boot_remove_plugin_line(void)
{
    char input[BOOT_CAP];
    char output[BOOT_CAP];
    size_t input_length = 0;
    size_t output_length = 0;
    size_t pos = 0;
    int removed = 0;

    if (read_text_file(BOOT_PATH, input, sizeof(input), &input_length) != 0)
        return -1;

    while (pos < input_length) {
        size_t start = pos;
        size_t end;
        while (pos < input_length && input[pos] != '\n') pos++;
        end = pos;
        if (pos < input_length) pos++;

        if (line_is_plugin(input + start, end - start)) {
            removed = 1;
            continue;
        }

        if (output_length + (pos - start) >= sizeof(output))
            return -2;
        memcpy(output + output_length, input + start, pos - start);
        output_length += pos - start;
    }

    if (!removed) return 0;
    if (backup_once(BOOT_PATH, BOOT_BACKUP) != 0)
        return -3;

    output[output_length] = '\0';
    return write_atomic(BOOT_PATH, BOOT_TEMP, output, output_length);
}

static int ensure_fetch_settings(void)
{
    static const char defaults[] = "mode=fetch\n";
    if (ensure_dir("/dev_hdd0/tmp") != 0) return -1;
    if (ensure_dir(SETTINGS_DIR) != 0) return -1;
    if (file_exists(SETTINGS_PATH)) return 0;
    return write_atomic(SETTINGS_PATH,
                        "/dev_hdd0/tmp/simple-cheat-menu/settings.dsf-tmp",
                        defaults, sizeof(defaults) - 1);
}

static void rollback_plugin_file(void)
{
    remove(PLUGIN_TEMP);
    if (file_exists(PLUGIN_BACKUP)) {
        remove(PLUGIN_PATH);
        (void)rename(PLUGIN_BACKUP, PLUGIN_PATH);
    } else {
        remove(PLUGIN_PATH);
    }
}

static int install_plugin(void)
{
    struct stat info;

    if (!file_exists(APP_PLUGIN_SOURCE)) {
        snprintf(g_message, sizeof(g_message),
                 "ERROR: plugin payload is missing from this PKG.");
        return -1;
    }
    if (stat(APP_PLUGIN_SOURCE, &info) != 0 ||
        info.st_size < 20000 || info.st_size > 512000) {
        snprintf(g_message, sizeof(g_message),
                 "ERROR: plugin payload size check failed.");
        return -2;
    }
    if (ensure_dir(PLUGIN_DIR) != 0) {
        snprintf(g_message, sizeof(g_message),
                 "ERROR: cannot create /dev_hdd0/plugins.");
        return -3;
    }
    if (ensure_fetch_settings() != 0) {
        snprintf(g_message, sizeof(g_message),
                 "ERROR: cannot create safe fetch settings.");
        return -4;
    }

    if (backup_once(PLUGIN_PATH, PLUGIN_BACKUP) != 0) {
        snprintf(g_message, sizeof(g_message),
                 "ERROR: existing plugin backup failed; nothing replaced.");
        return -5;
    }
    if (copy_file(APP_PLUGIN_SOURCE, PLUGIN_TEMP) != 0) {
        snprintf(g_message, sizeof(g_message),
                 "ERROR: plugin copy failed.");
        return -6;
    }

    /* Back up first, then remove the old destination for cellFs rename compatibility. */
    if (file_exists(PLUGIN_PATH) && remove(PLUGIN_PATH) != 0) {
        remove(PLUGIN_TEMP);
        snprintf(g_message, sizeof(g_message),
                 "ERROR: existing plugin could not be replaced.");
        return -7;
    }
    if (rename(PLUGIN_TEMP, PLUGIN_PATH) != 0) {
        rollback_plugin_file();
        snprintf(g_message, sizeof(g_message),
                 "ERROR: plugin rename failed; previous plugin restored.");
        return -8;
    }

    if (boot_add_plugin_line() != 0) {
        rollback_plugin_file();
        snprintf(g_message, sizeof(g_message),
                 "ERROR: boot_plugins update failed; plugin rolled back.");
        return -9;
    }

    snprintf(g_message, sizeof(g_message),
             "INSTALLED. Reboot PS3, then launch Driver: San Francisco.");
    return 0;
}

static int uninstall_plugin(void)
{
    if (boot_remove_plugin_line() != 0) {
        snprintf(g_message, sizeof(g_message),
                 "ERROR: boot_plugins update failed; plugin kept in place.");
        return -1;
    }

    if (file_exists(PLUGIN_BACKUP)) {
        remove(PLUGIN_PATH);
        if (rename(PLUGIN_BACKUP, PLUGIN_PATH) != 0) {
            snprintf(g_message, sizeof(g_message),
                     "ERROR: backup restore failed. Keep console on CFW.");
            return -2;
        }
    } else {
        remove(PLUGIN_PATH);
    }

    snprintf(g_message, sizeof(g_message),
             "REMOVED. Other plugins and game files were left untouched.");
    return 0;
}

static void check_status(void)
{
    char data[BOOT_CAP];
    size_t length = 0;
    int plugin = file_exists(PLUGIN_PATH);
    int boot = 0;

    if (read_text_file(BOOT_PATH, data, sizeof(data), &length) == 0)
        boot = boot_has_plugin_line(data, length);

    if (plugin && boot)
        snprintf(g_message, sizeof(g_message),
                 "STATUS: plugin installed and enabled at boot.");
    else if (plugin)
        snprintf(g_message, sizeof(g_message),
                 "STATUS: plugin file exists; boot entry is missing.");
    else if (boot)
        snprintf(g_message, sizeof(g_message),
                 "STATUS: boot entry exists but plugin file is missing.");
    else
        snprintf(g_message, sizeof(g_message),
                 "STATUS: Simple Cheat Menu is not installed.");
}

static void draw_text(float x, float y, u32 color, const char *text)
{
    SetFontColor(color, 0x00000000);
    DrawString(x, y, (char *)(text ? text : ""));
}

static void draw_ui(void)
{
    static const char *items[] = {
        "INSTALL / UPDATE PLUGIN",
        "REMOVE PLUGIN",
        "CHECK INSTALLATION",
        "EXIT"
    };
    int i;

    tiny3d_Clear(0xff070a0d, TINY3D_CLEAR_ALL);
    tiny3d_Project2D();

    SetFontSize(20, 30);
    draw_text(38, 34, 0xcaff39ff, "DSF MOD MENU");
    SetFontSize(14, 22);
    draw_text(38, 78, 0xffffffff, "DRIVER: SAN FRANCISCO  |  BLES00891");
    draw_text(38, 108, 0x9fb0c0ff, "CFW / COBRA VSH PLUGIN INSTALLER");

    for (i = 0; i < 4; i++) {
        char row[64];
        snprintf(row, sizeof(row), "%s %s",
                 (i == g_selected) ? ">" : " ", items[i]);
        draw_text(70, 166.0f + (float)i * 42.0f,
                  (i == g_selected) ? 0xcaff39ff : 0xe3e8eeff, row);
    }

    draw_text(38, 356, 0x8bd450ff, g_message);
    draw_text(38, 402, 0xffffffff, "IN GAME: SHORT PS BUTTON OPENS THE MENU.");
    draw_text(38, 432, 0xb6c1ccff, "UP/DOWN: MOVE   CROSS: SELECT   CIRCLE: EXIT");
    draw_text(38, 466, 0xffc36bff, "Only activate cheats marked GREEN for your build.");

    tiny3d_Flip();
}

int main(int argc, char *argv[])
{
    padInfo pad_info;
    padData pad_data;
    u16 old_up = 0, old_down = 0, old_cross = 0, old_circle = 0;
    int pad_ready = 0;

    (void)argc;
    (void)argv;

    if (!ya2d_init()) {
        snprintf(g_message, sizeof(g_message), "ERROR: graphics initialization failed.");
        return 1;
    }
    pad_ready = 1;

    g_font_texture = tiny3d_AllocTexture(2 * 1024 * 1024);
    if (!g_font_texture) return 1;

    ResetFont();
    AddFontFromBitmapArray((u8 *)ps3scale_font8x12, (u8 *)g_font_texture,
                           32, 126, 8, 12, 1, BIT7_FIRST_PIXEL);
    SetCurrentFont(0);
    SetFontSize(14, 22);

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, app_sysutil_callback, NULL);

    while (!g_exit_requested) {
        u16 up = 0, down = 0, cross = 0, circle = 0;

        sysUtilCheckCallback();
        if (pad_ready && ioPadGetInfo(&pad_info) == 0) {
            int p;
            for (p = 0; p < MAX_PADS; p++) {
                if (!pad_info.status[p]) continue;
                if (ioPadGetData(p, &pad_data) == 0 && pad_data.len >= 8) {
                    up = pad_data.BTN_UP;
                    down = pad_data.BTN_DOWN;
                    cross = pad_data.BTN_CROSS;
                    circle = pad_data.BTN_CIRCLE;
                    break;
                }
            }
        }

        if (up && !old_up) g_selected = (g_selected + 3) % 4;
        if (down && !old_down) g_selected = (g_selected + 1) % 4;

        if (cross && !old_cross) {
            if (g_selected == 0) (void)install_plugin();
            else if (g_selected == 1) (void)uninstall_plugin();
            else if (g_selected == 2) check_status();
            else g_exit_requested = 1;
        }
        if (circle && !old_circle)
            g_exit_requested = 1;

        old_up = up;
        old_down = down;
        old_cross = cross;
        old_circle = circle;

        draw_ui();
    }

    return 0;
}
