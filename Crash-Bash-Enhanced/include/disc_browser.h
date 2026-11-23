#ifndef CB_DISC_BROWSER_H
#define CB_DISC_BROWSER_H

#include <stddef.h>

#define CB_DISC_MAX 12
#define CB_DISC_PATH_MAX 256
#define CB_DISC_NAME_MAX 64

typedef struct {
    char path[CB_DISC_PATH_MAX];
    char name[CB_DISC_NAME_MAX];
} CBGameDisc;

/* Scan the embedded game folder first, then standard PS3 HDD/USB PSXISO folders. */
int cb_disc_scan(CBGameDisc *items, size_t capacity, size_t *count);

/* Root-injected variant used by host regression tests. */
int cb_disc_scan_roots(CBGameDisc *items, size_t capacity, size_t *count,
                       const char *const *roots, size_t root_count);

#endif
