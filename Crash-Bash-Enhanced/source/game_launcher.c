#include "game_launcher.h"

#include <stdio.h>
#include <sys/process.h>
#include <sys/stat.h>

#define CB_GAME_EBOOT "/dev_hdd0/game/CBON00001/USRDIR/EBOOT.BIN"

int cb_launch_game(char *error, size_t error_size)
{
    struct stat target;

    if (stat(CB_GAME_EBOOT, &target) != 0 || !S_ISREG(target.st_mode)) {
        if (error && error_size) {
            snprintf(error, error_size,
                     "Install Crash Bash Online (CBON00001) before launching the game.");
        }
        return -1;
    }

    sysProcessExitSpawn2(CB_GAME_EBOOT, NULL, NULL, NULL, 0, 1001,
                         SYS_PROCESS_SPAWN_STACK_SIZE_1M);

    if (error && error_size) {
        snprintf(error, error_size,
                 "PS3 could not start Crash Bash Online. Check the installed PKG.");
    }
    return -1;
}
