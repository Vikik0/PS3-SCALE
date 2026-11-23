#ifndef CB_DISC_FINGERPRINT_H
#define CB_DISC_FINGERPRINT_H
#include <stddef.h>
/* Stable 64-bit fingerprint of CUE track layout and referenced track contents. */
int cb_disc_fingerprint(const char *cue_path, char *output, size_t output_size);
#endif
