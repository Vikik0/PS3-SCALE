#include <cell/msg_dialog.h>
#include <cell/sysutil.h>
#include <sys/process.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

SYS_PROCESS_PARAM(1001, 0x10000);

#define BOOT_PLUGINS_PATH "/dev_hdd0/boot_plugins.txt"
#define LOADER_PATH "/dev_hdd0/game/CBAS00001/USRDIR/crash_bash_loader.fake.sprx"
#define INPUT_PATH "/dev_hdd0/game/CBAS00001/USRDIR/crash_bash_input.fake.sprx"

static volatile int g_dialog_done;
static volatile int g_dialog_button;

static void dialog_callback(CellMsgDialogCallback button, void *userdata)
{
    (void)userdata;
    g_dialog_button = (int)button;
    g_dialog_done = 1;
}

static void sysutil_callback(uint64_t status, uint64_t parameter, void *userdata)
{
    (void)parameter;
    (void)userdata;
    if (status == CELL_SYSUTIL_REQUEST_EXITGAME) {
        sysProcessExit(0);
    }
}

static int show_dialog(const char *message, uint32_t type)
{
    int result;
    g_dialog_done = 0;
    g_dialog_button = -1;

    result = cellMsgDialogOpen2(type, message, dialog_callback, NULL, NULL);
    if (result != CELL_OK) return -1;

    while (!g_dialog_done) {
        cellSysutilCheckCallback();
        usleep(10000);
    }
    return g_dialog_button;
}

static int plugin_files_exist(void)
{
    return access(LOADER_PATH, F_OK) == 0 &&
           access(INPUT_PATH, F_OK) == 0;
}

/* Returns 0 when added, 1 when already present, and -1 on I/O failure. */
static int append_loader_to_boot_list(void)
{
    FILE *file;
    char line[512];
    int found = 0;
    int has_content = 0;
    int last_character = '\n';

    errno = 0;
    file = fopen(BOOT_PLUGINS_PATH, "r");
    if (file) {
        while (fgets(line, sizeof(line), file)) {
            char *cursor = line;
            has_content = 1;
            while (*cursor == ' ' || *cursor == '\t') ++cursor;
            if (*cursor != '#' && strstr(cursor, LOADER_PATH) != NULL) {
                found = 1;
            }
        }
        if (ferror(file)) {
            fclose(file);
            return -1;
        }
        if (fseek(file, 0, SEEK_END) != 0) {
            fclose(file);
            return -1;
        }
        if (ftell(file) > 0) {
            if (fseek(file, -1, SEEK_END) != 0) {
                fclose(file);
                return -1;
            }
            last_character = fgetc(file);
        }
        fclose(file);
    } else if (errno != ENOENT) {
        return -1;
    }

    if (found) return 1;

    file = fopen(BOOT_PLUGINS_PATH, "a");
    if (!file) return -1;

    if (has_content && last_character != '\n') {
        if (fputc('\n', file) == EOF) {
            fclose(file);
            return -1;
        }
    }
    if (fputs(LOADER_PATH, file) == EOF || fputc('\n', file) == EOF ||
        fflush(file) != 0) {
        fclose(file);
        return -1;
    }
    if (fclose(file) != 0) return -1;
    return 0;
}

int main(void)
{
    const uint32_t yes_no_type =
        CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL |
        CELL_MSGDIALOG_TYPE_BG_VISIBLE |
        CELL_MSGDIALOG_TYPE_BUTTON_TYPE_YESNO |
        CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_YES;
    const uint32_t ok_type =
        CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL |
        CELL_MSGDIALOG_TYPE_BG_VISIBLE |
        CELL_MSGDIALOG_TYPE_BUTTON_TYPE_OK |
        CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_OK;
    int answer;
    int install_result;
    const char *message;

    if (cellSysutilRegisterCallback(0, sysutil_callback, NULL) != CELL_OK) {
        return 1;
    }

    answer = show_dialog(
        "Install Crash Bash Online?\n\n"
        "This package has installed the plugin files. The installer will add "
        "the VSH loader to /dev_hdd0/boot_plugins.txt.\n\n"
        "A Cobra-compatible CFW/HEN setup is required. Reboot after installation.",
        yes_no_type);

    if (answer == CELL_MSGDIALOG_BUTTON_YES) {
        if (!plugin_files_exist()) {
            message = "Plugin files are missing. Reinstall the Crash Bash Online PKG.";
        } else {
            install_result = append_loader_to_boot_list();
            if (install_result == 0) {
                message = "Installation complete.\n\n"
                          "The loader was added to boot_plugins.txt.\n"
                          "Reboot the PS3 to activate Crash Bash Online.";
            } else if (install_result == 1) {
                message = "Crash Bash Online is already enabled in boot_plugins.txt.\n\n"
                          "Reboot the PS3 if you have not done so yet.";
            } else {
                message = "Could not update /dev_hdd0/boot_plugins.txt.\n\n"
                          "Check that the HDD is writable, then add the loader path manually.";
            }
        }
    } else {
        message = "Installation cancelled.";
    }

    (void)show_dialog(message, ok_type);
    cellSysutilUnregisterCallback(0);
    return 0;
}
