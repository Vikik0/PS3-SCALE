#ifndef PS3SCALE_ISO9660_H
#define PS3SCALE_ISO9660_H

#include <stddef.h>

/*
 * Reads the ISO9660 primary volume descriptor and root SYSTEM.CNF from a
 * PS2 disc image. Returns 0 and writes a serial such as SLUS_203.12 on
 * success. The image is opened read-only.
 */
int ps3scale_ps2_iso_read_serial(const char *iso_path,
                                 char *out, size_t out_size);

/* Extracts a conventional PS3 Title ID (e.g. BLUS31156) from a filename. */
int ps3scale_title_id_from_filename(const char *filename,
                                    char *out, size_t out_size);

/* Extracts a conventional PS2 serial (e.g. SLUS_203.12) from a filename. */
int ps3scale_ps2_serial_from_filename(const char *filename,
                                      char *out, size_t out_size);

#endif
