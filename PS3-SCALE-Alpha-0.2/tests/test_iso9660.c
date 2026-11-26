#include "ps3scale/iso9660.h"
#include "ps3scale/ps2_game_patch.h"

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

static size_t make_special_record(unsigned char *p, unsigned int lba,
                                  unsigned int size, unsigned char identifier)
{
    memset(p, 0, 34u);
    p[0] = 34u;
    put32le(p + 2u, lba);
    put32le(p + 10u, size);
    p[25] = 2u;
    p[28] = 1u;
    p[32] = 1u;
    p[33] = identifier;
    return 34u;
}

static void make_record_flags(unsigned char *p, unsigned int lba,
                              unsigned int size, const char *name,
                              unsigned char flags)
{
    make_record(p, lba, size, name, (flags & 0x02u) != 0u);
    p[25] = flags;
}

static void write_u32_at(const char *path, long offset, unsigned int value)
{
    unsigned char bytes[4];
    FILE *fp;
    put32le(bytes, value);
    fp = fopen(path, "r+b");
    assert(fp != NULL);
    assert(fseek(fp, offset, SEEK_SET) == 0);
    assert(fwrite(bytes, 1, sizeof(bytes), fp) == sizeof(bytes));
    assert(fclose(fp) == 0);
}

static void write_byte_at(const char *path, long offset, unsigned char value)
{
    FILE *fp = fopen(path, "r+b");
    assert(fp != NULL);
    assert(fseek(fp, offset, SEEK_SET) == 0);
    assert(fputc(value, fp) != EOF);
    assert(fclose(fp) == 0);
}

static void write_sector(const char *path, unsigned int lba,
                         const void *data, size_t size)
{
    FILE *fp;
    unsigned char sector[SECTOR];

    assert(size <= sizeof(sector));
    memset(sector, 0, sizeof(sector));
    if (size)
        memcpy(sector, data, size);

    fp = fopen(path, "r+b");
    assert(fp != NULL);
    assert(fseek(fp, (long)lba * SECTOR, SEEK_SET) == 0);
    assert(fwrite(sector, 1, sizeof(sector), fp) == sizeof(sector));
    assert(fclose(fp) == 0);
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
    unsigned char *boot_dir;
    unsigned char elf[4096];
    const char cnf_root[] = "BOOT2 = cdrom0:\\SLUS_203.12;1\r\n";
    const char cnf_tail[] = "\r\nBOOT2 = cdrom0:\\SLUS_203.12;1\r\n";
    unsigned char cnf_first[SECTOR];
    const char cnf_nested[] = "BOOT2 = cdrom0:\\BOOT\\SLUS_203.12;1\r\n";
    const char cnf_traversal[] = "BOOT2 = cdrom0:\\..\\SLUS_203.12;1\r\n";
    char serial[16];
    char ps3id[16];
    char ps2id[16];
    unsigned char *loaded_elf = NULL;
    size_t loaded_elf_size = 0;
    uint32_t elf_hash = 0;
    uint32_t word = 0;
    size_t image_size = 28 * SECTOR;
    size_t pos = 0;
    size_t boot_pos = 0;
    size_t nested_boot_first_offset = 0;
    size_t nested_boot_second_offset = 0;

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
    make_record_flags(root + pos, 21, SECTOR, "SYSTEM.CNF", 0x80u);
    pos += root[pos];
    make_record(root + pos, 26, (unsigned int)(sizeof(cnf_tail) - 1u),
                "SYSTEM.CNF", 0);
    pos += root[pos];

    make_record_flags(root + pos, 24, SECTOR, "SLUS_203.12;1", 0x80u);
    pos += root[pos];
    make_record(root + pos, 27, SECTOR, "SLUS_203.12;1", 0);
    pos += root[pos];
    make_record(root + pos, 23, SECTOR, "BOOT;1", 1);

    boot_dir = image + 23 * SECTOR;
    boot_pos += make_special_record(boot_dir + boot_pos, 23, SECTOR, 0);
    boot_pos += make_special_record(boot_dir + boot_pos, 20, SECTOR, 1);
    nested_boot_first_offset = boot_pos;
    make_record_flags(boot_dir + boot_pos, 24, SECTOR,
                      "SLUS_203.12", 0x80u);
    boot_pos += boot_dir[boot_pos];
    nested_boot_second_offset = boot_pos;
    make_record(boot_dir + boot_pos, 27, SECTOR, "SLUS_203.12", 0);

    memset(cnf_first, ' ', sizeof(cnf_first));
    memcpy(image + 21 * SECTOR, cnf_first, sizeof(cnf_first));
    memcpy(image + 26 * SECTOR, cnf_tail, sizeof(cnf_tail) - 1u);
    memcpy(image + 24 * SECTOR, elf, SECTOR);
    memcpy(image + 27 * SECTOR, elf + SECTOR, SECTOR);
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
    assert(memcmp(loaded_elf, elf, sizeof(elf)) == 0);
    assert(elf_hash == pcsx2_hash(elf, sizeof(elf)));
    assert(ps3scale_ps2_elf_read_exec_word(
        loaded_elf, loaded_elf_size, 0x00100004, &word) == 0);
    assert(word == 0x24030002);

    {
        static const ps3scale_ps2_patch_word_t valid_words[] = {
            {0x00100000u, 0x00000000u},
            {0x00100004u, 0x00000000u}
        };
        static const ps3scale_ps2_patch_word_t invalid_words[] = {
            {0x00100000u, 0x00000000u},
            {0x00110000u, 0x00000000u}
        };
        const ps3scale_ps2_game_patch_t valid_patch = {
            "SLUS_203.12", 0xB0AE2D8Au, "fixture", NULL,
            PS3SCALE_PS2_PATCH_FPS, valid_words,
            sizeof(valid_words) / sizeof(valid_words[0])
        };
        const ps3scale_ps2_game_patch_t invalid_patch = {
            "SLUS_203.12", 0xB0AE2D8Au, "fixture", NULL,
            PS3SCALE_PS2_PATCH_FPS, invalid_words,
            sizeof(invalid_words) / sizeof(invalid_words[0])
        };
        assert(ps3scale_ps2_game_patch_elf_addresses_valid(
            &valid_patch, loaded_elf, loaded_elf_size) == 1);
        assert(ps3scale_ps2_game_patch_elf_addresses_valid(
            &invalid_patch, loaded_elf, loaded_elf_size) == 0);
    }

    assert(ps3scale_ps2_elf_read_exec_word(
        loaded_elf, loaded_elf_size, 0x00100004 + 1, &word) < 0);
    assert(ps3scale_ps2_elf_read_exec_word(
        loaded_elf, loaded_elf_size, 0x00110000, &word) < 0);
    {
        const ps3scale_ps2_game_patch_t *patch;
        patch = ps3scale_ps2_game_patch_find("SLUS_212.60",0xB0AE2D8Au,PS3SCALE_PS2_PATCH_FPS);
        assert(patch && patch->word_count == 1u);
        assert(patch->words[0].ee_address == 0x00161860u);
        assert(patch->words[0].replacement_word == 0x2C620000u);
        assert(ps3scale_ps2_game_patch_find("SLUS_212.60",0xB0AE2D8Bu,PS3SCALE_PS2_PATCH_FPS)==NULL);
        assert(ps3scale_ps2_game_patch_find("SLPM_662.46",0x67D76160u,PS3SCALE_PS2_PATCH_FPS)!=NULL);
        patch=ps3scale_ps2_game_patch_find("SLPM_662.46",0x67D76160u,PS3SCALE_PS2_PATCH_FPS);
        assert(patch && patch->word_count == 1u);
        assert(patch->words[0].ee_address == 0x0011B560u);
        assert(patch->words[0].replacement_word == 0x10000006u);
        assert(ps3scale_ps2_game_patch_find("SLPM_662.46",0x67D76161u,PS3SCALE_PS2_PATCH_FPS)==NULL);
        assert(ps3scale_ps2_game_patch_find("SLUS_203.12",0xB0AE2D8Au,PS3SCALE_PS2_PATCH_FPS)==NULL);
        assert(ps3scale_ps2_game_patch_find("SLUS_203.49",0x10B1F370u,PS3SCALE_PS2_PATCH_FPS)!=NULL);
        assert(ps3scale_ps2_game_patch_find("SLUS_211.83",0xE1F17139u,PS3SCALE_PS2_PATCH_FPS)!=NULL);
        assert(ps3scale_ps2_game_patch_find("SLUS_214.37",0x86B490D1u,PS3SCALE_PS2_PATCH_FPS)!=NULL);
        assert(ps3scale_ps2_game_patch_find("SLUS_214.55",0xBAFEFC57u,PS3SCALE_PS2_PATCH_FPS)!=NULL);
        assert(ps3scale_ps2_game_patch_find("SLUS_214.15",0xE10A865Bu,PS3SCALE_PS2_PATCH_FPS)!=NULL);
        assert(ps3scale_ps2_game_patch_find("SLUS_218.85",0xB15F0BD6u,PS3SCALE_PS2_PATCH_FPS)!=NULL);
        assert(ps3scale_ps2_game_patch_find("SLUS_206.96",0xFCC09959u,PS3SCALE_PS2_PATCH_FPS)!=NULL);
        patch=ps3scale_ps2_game_patch_find("SLUS_203.49",0x10B1F370u,PS3SCALE_PS2_PATCH_WIDESCREEN);
        assert(patch && patch->word_count == 8u);
        patch=ps3scale_ps2_game_patch_find("SLUS_211.83",0xE1F17139u,PS3SCALE_PS2_PATCH_WIDESCREEN);
        assert(patch && patch->word_count == 13u);
        patch=ps3scale_ps2_game_patch_find("SLUS_214.37",0x86B490D1u,PS3SCALE_PS2_PATCH_WIDESCREEN);
        assert(patch && patch->word_count == 7u);
        patch=ps3scale_ps2_game_patch_find("SLUS_206.96",0xFCC09959u,PS3SCALE_PS2_PATCH_WIDESCREEN);
        assert(patch && patch->word_count == 3u);
        patch=ps3scale_ps2_game_patch_find("SLES_548.43",0xAEFD7D25u,PS3SCALE_PS2_PATCH_480P);
        assert(patch && patch->word_count == 3u);
        patch=ps3scale_ps2_game_patch_find("SLES_549.75",0x278B7BEAu,PS3SCALE_PS2_PATCH_480P);
        assert(patch && patch->word_count == 3u);
        patch=ps3scale_ps2_game_patch_find("SLES_513.93",0xD1ACD489u,PS3SCALE_PS2_PATCH_480P);
        assert(patch && patch->word_count == 1u);
        assert(ps3scale_ps2_game_patch_find("SLES_548.43",0xAEFD7D24u,PS3SCALE_PS2_PATCH_480P)==NULL);
        assert(ps3scale_ps2_game_patch_find("SLUS_203.49",0x10B1F370u,(ps3scale_ps2_patch_kind_t)99)==NULL);
    }
    free(loaded_elf);


    /* A nested SYSTEM.CNF boot path resolves through ISO9660 directories. */
    write_sector(path, 21, cnf_nested, sizeof(cnf_nested) - 1u);
    assert(ps3scale_ps2_iso_load_boot_elf(
        path, &loaded_elf, &loaded_elf_size,
        serial, sizeof(serial), &elf_hash) == 0);
    assert(strcmp(serial, "SLUS_203.12") == 0);
    assert(loaded_elf_size == sizeof(elf));
    assert(memcmp(loaded_elf, elf, sizeof(elf)) == 0);
    assert(elf_hash == pcsx2_hash(elf, sizeof(elf)));
    free(loaded_elf);
    loaded_elf = NULL;

    /* Parent-directory traversal is rejected and output pointers stay clear. */
    write_sector(path, 21, cnf_traversal, sizeof(cnf_traversal) - 1u);
    assert(ps3scale_ps2_iso_load_boot_elf(
        path, &loaded_elf, &loaded_elf_size,
        serial, sizeof(serial), &elf_hash) < 0);
    assert(loaded_elf == NULL);
    assert(loaded_elf_size == 0u);
    assert(elf_hash == 0u);
    assert(serial[0] == '\0');

    write_sector(path, 21, cnf_root, sizeof(cnf_root) - 1u);
    /* Root-level multi-extent BOOT ELF is assembled in directory-record order. */
    assert(loaded_elf == NULL);
    assert(ps3scale_ps2_iso_load_boot_elf(
        path, &loaded_elf, &loaded_elf_size,
        serial, sizeof(serial), &elf_hash) == 0);
    assert(loaded_elf_size == sizeof(elf));
    assert(memcmp(loaded_elf, elf, sizeof(elf)) == 0);
    assert(elf_hash == pcsx2_hash(elf, sizeof(elf)));
    free(loaded_elf);
    loaded_elf = NULL;

    /* Nested multi-extent BOOT ELF is assembled from both extents as well. */
    write_sector(path, 21, cnf_nested, sizeof(cnf_nested) - 1u);
    assert(ps3scale_ps2_iso_load_boot_elf(
        path, &loaded_elf, &loaded_elf_size,
        serial, sizeof(serial), &elf_hash) == 0);
    assert(loaded_elf_size == sizeof(elf));
    assert(memcmp(loaded_elf, elf, sizeof(elf)) == 0);
    assert(elf_hash == pcsx2_hash(elf, sizeof(elf)));
    free(loaded_elf);
    loaded_elf = NULL;

    /* A continuation record with a different identifier is rejected. */
    write_byte_at(path,
        23L * SECTOR + (long)nested_boot_second_offset + 33L, 'X');
    assert(ps3scale_ps2_iso_load_boot_elf(
        path, &loaded_elf, &loaded_elf_size,
        serial, sizeof(serial), &elf_hash) < 0);
    assert(loaded_elf == NULL && loaded_elf_size == 0u && elf_hash == 0u);
    assert(serial[0] == '\0');
    write_byte_at(path,
        23L * SECTOR + (long)nested_boot_second_offset + 33L, 'S');

    /* A non-final extent must be sector-aligned. */
    write_u32_at(path,
        23L * SECTOR + (long)nested_boot_first_offset + 10L, SECTOR - 1u);
    assert(ps3scale_ps2_iso_load_boot_elf(
        path, &loaded_elf, &loaded_elf_size,
        serial, sizeof(serial), &elf_hash) < 0);
    assert(loaded_elf == NULL && loaded_elf_size == 0u && elf_hash == 0u);
    write_u32_at(path,
        23L * SECTOR + (long)nested_boot_first_offset + 10L, SECTOR);

    /* Extent totals above the 32 MiB boot-ELF limit are rejected before I/O. */
    write_u32_at(path,
        23L * SECTOR + (long)nested_boot_second_offset + 10L,
        33u * 1024u * 1024u);
    assert(ps3scale_ps2_iso_load_boot_elf(
        path, &loaded_elf, &loaded_elf_size,
        serial, sizeof(serial), &elf_hash) < 0);
    assert(loaded_elf == NULL && loaded_elf_size == 0u && elf_hash == 0u);
    write_u32_at(path,
        23L * SECTOR + (long)nested_boot_second_offset + 10L, SECTOR);

    /* Extent chains longer than the 32-record bound are rejected. */
    {
        unsigned char many_extents[SECTOR];
        size_t many_pos = 0;
        unsigned int extent_index;
        memset(many_extents, 0, sizeof(many_extents));
        many_pos += make_special_record(
            many_extents + many_pos, 23, SECTOR, 0);
        many_pos += make_special_record(
            many_extents + many_pos, 20, SECTOR, 1);
        for (extent_index = 0; extent_index < 33u; ++extent_index) {
            unsigned char flags = extent_index < 32u ? 0x80u : 0u;
            make_record_flags(many_extents + many_pos,
                24u + extent_index, SECTOR, "SLUS_203.12", flags);
            many_pos += many_extents[many_pos];
        }
        assert(many_pos <= sizeof(many_extents));
        write_sector(path, 23, many_extents, sizeof(many_extents));
        assert(ps3scale_ps2_iso_load_boot_elf(
            path, &loaded_elf, &loaded_elf_size,
            serial, sizeof(serial), &elf_hash) < 0);
        assert(loaded_elf == NULL && loaded_elf_size == 0u && elf_hash == 0u);
        assert(serial[0] == '\0');
    }

    /* SYSTEM.CNF's two extents are also joined for serial extraction. */
    assert(ps3scale_ps2_iso_read_serial(path, serial, sizeof(serial)) == 0);
    assert(strcmp(serial, "SLUS_203.12") == 0);

    assert(ps3scale_title_id_from_filename(
        "Grand Theft Auto V [BLUS31156].iso", ps3id, sizeof(ps3id)) == 0);
    assert(strcmp(ps3id, "BLUS31156") == 0);

    assert(ps3scale_ps2_serial_from_filename(
        "Black [SLUS_213.76].iso", ps2id, sizeof(ps2id)) == 0);
    assert(strcmp(ps2id, "SLUS_213.76") == 0);

    puts("PS3 SCALE ISO metadata and ELF tests: PASS");
    return 0;
}
