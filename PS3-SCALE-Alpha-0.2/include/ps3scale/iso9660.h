#ifndef PS3SCALE_ISO9660_H
#define PS3SCALE_ISO9660_H

#include <stddef.h>
#include <stdint.h>

/*
 * Reads the ISO9660 primary volume descriptor and root SYSTEM.CNF from a
 * PS2 disc image. Returns 0 and writes a serial such as SLUS_203.12 on
 * success. The image is opened read-only.
 */
int ps3scale_ps2_iso_read_serial(const char *iso_path,
                                 char *out, size_t out_size);

/*
 * Load the root-level BOOT/BOOT2 ELF referenced by SYSTEM.CNF.
 * The caller owns *elf_data and must free it. The returned hash matches
 * PCSX2's ELF hash used in <serial>_<hash>.pnach filenames.
 */
int ps3scale_ps2_iso_load_boot_elf(const char *iso_path,
                                  unsigned char **elf_data,
                                  size_t *elf_size,
                                  char *serial, size_t serial_size,
                                  uint32_t *pcsx2_elf_hash);

/* Read a 32-bit word from a file-backed executable EE PT_LOAD segment. */
int ps3scale_ps2_elf_read_exec_word(const unsigned char *elf_data,
                                   size_t elf_size,
                                   uint32_t ee_address,
                                   uint32_t *word_out);

/* Extracts a conventional PS3 Title ID (e.g. BLUS31156) from a filename. */
int ps3scale_title_id_from_filename(const char *filename,
                                    char *out, size_t out_size);

/* Extracts a conventional PS2 serial (e.g. SLUS_203.12) from a filename. */
int ps3scale_ps2_serial_from_filename(const char *filename,
                                      char *out, size_t out_size);

#endif
