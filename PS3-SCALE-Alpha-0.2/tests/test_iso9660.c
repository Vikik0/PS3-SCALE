#include "ps3scale/iso9660.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR 2048

static void put16le(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
}

static void put32le(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)((v >> 24) & 0xff);
}

static void make_record(unsigned char *p, unsigned int lba,
                        unsigned int size, const char *name, int is_dir)
{
    size_t n = strlen(name);
    size_t len = 33 + n + ((n % 2) == 0 ? 1 : 0);
    memset(p, 0, len);
    p[0] = (unsigned char)len;
    p[1] = 0;
    put32le(p + 2, lba);
    put32le(p + 10, size);
    p[25] = is_dir ? 2 : 0;
    p[28] = 1;
    p[32] = (unsigned char)n;
    memcpy(p + 33, name, n);
}

static void make_test_elf(unsigned char *elf, size_t size)
{
    unsigned char *ph;

    assert(size >= 512u);
    memset(elf, 0, size);
    elf[0] = 0x7F;
    memcpy(elf + 1, "ELF", 3);
    elf[4] = 1; /* ELFCLASS32 */
    elf[5] = 1; /* ELFDATA2LSB */
    elf[6] = 1;
    put16le(elf + 16, 2); /* ET_EXEC */
    put16le(elf + 18, 8); /* EM_MIPS */
    put32le(elf + 20, 1);
    put32le(elf + 24, 0x00100000);
    put32le(elf + 28, 52);
    put16le(elf + 40, 52);
    put16le(elf + 42, 32);
    put16le(elf + 44, 1);

    ph = elf + 52;
    put32le(ph, 1); /* PT_LOAD */
    put32le(ph + 4, 0x100);
    put32le(ph + 8, 0x00100000);
    put32le(ph + 12, 0x00100000);
    put32le(ph + 16, 0x100);
    put32le(ph + 20, 0x100);
    put32le(ph + 24, 5); /* PF_R | PF_X */
    put32le(ph + 28, 0x1000);

    put32le(elf + 0x100, 0x24020001);
    put32le(elf + 0x104, 0x24030002);
}

static unsigned int pcsx2_hash(const unsigned char *data, size_t size)
{
    size_t i;
    unsigned int hash = 0;
    for (i = 0; i + 4u <= size; i += 4u)
        hash ^= (unsigned int)data[i] |
                ((unsigned int)data[i + 1u] << 8) |
                ((unsigned int)data[i + 2u] << 16) |
                ((unsigned int)data[i + 3u] << 24);
    return hash;
}

int main(void)
{
    const char *path = "build-host/sample_ps2.iso";
    FILE *fp;
    unsigned char *image;
    unsigned char *pvd;
    unsigned char *root;
    unsigned char elf[512];
    const char cnf[] = "BOOT2 = cdrom0:\\SLUS_203.12;1\r\n";
    char serial[16];
    char ps3id[16];
    char ps2id[16];
    unsigned char *loaded_elf = NULL;
    size_t loaded_elf_size = 0;
    uint32_t elf_hash = 0;
    uint32_t word = 0;
    size_t image_size = 23 * SECTOR;
    size_t pos = 0;

    make_test_elf(elf, sizeof(elf));
    image = (unsigned char *)calloc(1, image_size);
    assert(image != NULL);

    pvd = image + 16 * SECTOR;
    pvd[0] = 1;
    memcpy(pvd + 1, "CD001", 5);
    pvd[6] = 1;
    make_record(pvd + 156, 20, SECTOR, "\0", 1);

    image[17 * SECTOR] = 255;
    memcpy(image + 17 * SECTOR + 1, "CD001", 5);
    image[17 * SECTOR + 6] = 1;

    root = image + 20 * SECTOR;
    make_record(root + pos, 21, (unsigned int)(sizeof(cnf) - 1),
                "SYSTEM.CNF;1", 0);
    pos += root[pos];
    make_record(root + pos, 22, sizeof(elf), "SLUS_203.12;1", 0);
    memcpy(image + 21 * SECTOR, cnf, sizeof(cnf) - 1);
    memcpy(image + 22 * SECTOR, elf, sizeof(elf));

    fp = fopen(path, "wb");
    assert(fp != NULL);
    assert(fwrite(image, 1, image_size, fp) == image_size);
    fclose(fp);
    free(image);

    assert(ps3scale_ps2_iso_read_serial(path, serial, sizeof(serial)) == 0);
    assert(strcmp(serial, "SLUS_203.12") == 0);

    assert(ps3scale_ps2_iso_load_boot_elf(
        path, &loaded_elf, &loaded_elf_size,
        serial, sizeof(serial), &elf_hash) == 0);
    assert(strcmp(serial, "SLUS_203.12") == 0);
    assert(loaded_elf_size == sizeof(elf));
    assert(elf_hash == pcsx2_hash(elf, sizeof(elf)));
    assert(ps3scale_ps2_elf_read_exec_word(
        loaded_elf, loaded_elf_size, 0x00100004, &word) == 0);
    assert(word == 0x24030002);
    assert(ps3scale_ps2_elf_read_exec_word(
        loaded_elf, loaded_elf_size, 0x00100004 + 1, &word) < 0);
    assert(ps3scale_ps2_elf_read_exec_word(
        loaded_elf, loaded_elf_size, 0x00110000, &word) < 0);
    free(loaded_elf);

    assert(ps3scale_title_id_from_filename(
        "Grand Theft Auto V [BLUS31156].iso", ps3id, sizeof(ps3id)) == 0);
    assert(strcmp(ps3id, "BLUS31156") == 0);

    assert(ps3scale_ps2_serial_from_filename(
        "Black [SLUS_213.76].iso", ps2id, sizeof(ps2id)) == 0);
    assert(strcmp(ps2id, "SLUS_213.76") == 0);

    puts("PS3 SCALE ISO metadata and ELF tests: PASS");
    return 0;
}
