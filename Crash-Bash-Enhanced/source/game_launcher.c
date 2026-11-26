#include "game_launcher.h"
#include "game_launch_args.h"

#include <stdio.h>
#include <sys/process.h>
#include <sys/stat.h>

#define CB_GAME_EBOOT "/dev_hdd0/game/CBON00001/USRDIR/retroarch/EBOOT.BIN"

int cb_launch_game(const char *content_path, int is_host, const char *nickname,
                   const CBOnlineNetplayEndpoint *endpoint,
                   char *error, size_t error_size)
{
    struct stat target;
    const char *arguments[CB_GAME_LAUNCH_ARGV_CAPACITY];

    if (!content_path || !content_path[0] ||
        stat(content_path, &target) != 0 || !S_ISREG(target.st_mode)) {
        if (error && error_size) {
            snprintf(error, error_size,
                     "Crash Bash CUE not found in the embedded or external PSXISO folders.");
        }
        return -1;
    }

    if (!nickname || !nickname[0] ||
        cb_game_build_argv(content_path, is_host, nickname, endpoint,
                           arguments, CB_GAME_LAUNCH_ARGV_CAPACITY) < 0) {
        if (error && error_size) {
            snprintf(error, error_size, "Invalid game launch settings.");
        }
        return -1;
    }

    if (stat(CB_GAME_EBOOT, &target) != 0 || !S_ISREG(target.st_mode)) {
        if (error && error_size) {
            snprintf(error, error_size,
                     "The embedded RetroArch runtime is missing from this Crash Bash Online installation.");
        }
        return -1;
    }

    /* PSL1GHT expects argv[0] to be the first user argument, not the EBOOT path. */
    sysProcessExitSpawn2(CB_GAME_EBOOT, arguments, NULL, NULL, 0, 1001,
                         SYS_PROCESS_SPAWN_STACK_SIZE_1M);

    if (error && error_size) {
        snprintf(error, error_size,
                 "PS3 could not start Crash Bash Online. Check the installed PKG.");
    }
    return -1;
}
