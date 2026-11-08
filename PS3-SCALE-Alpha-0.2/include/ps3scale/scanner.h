#ifndef PS3SCALE_SCANNER_H
#define PS3SCALE_SCANNER_H

#include "ps3scale/game.h"

/*
 * Scans one mounted device root, e.g. "/dev_hdd0" or "/dev_usb000".
 * Read-only: this module never creates, renames, deletes, or writes game files.
 * Returns the number of newly discovered entries, or a negative error.
 */
int ps3scale_scan_device(const char *device_root, ps3scale_game_list_t *list);

/* Scans HDD, mounted Blu-ray, and USB000..USB007. */
int ps3scale_scan_all(ps3scale_game_list_t *list);

#endif
