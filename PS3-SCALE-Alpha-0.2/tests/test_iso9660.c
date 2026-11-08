#include "ps3scale/iso9660.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR 2048

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
    p[28] = 1; /* volume sequence number, little endian */
    p[32] = (unsigned char)n;
    memcpy(p + 33, name, n);
}

int main(void)
{
    const char *path = "build-host/sample_ps2.iso";
    FILE *fp;
    unsigned char *image;
    unsigned char *pvd;
    unsigned char *root;
    const char cnf[] = "BOOT2 = cdrom0:\\\\SLUS_203.12;1\\r\\n";
    char serial[16];
    char ps3id[16];
    char ps2id[16];
    size_t image_size = 22 * SECTOR;
    size_t rec_len;

    image = (unsigned char *)calloc(1, image_size);
    assert(image != NULL);

    pvd = image + 16 * SECTOR;
    pvd[0] = 1;
    memcpy(pvd + 1, "CD001", 5);
    pvd[6] = 1;
    make_record(pvd + 156, 20, SECTOR, "\\0", 1);

    image[17 * SECTOR] = 255;
    memcpy(image + 17 * SECTOR + 1, "CD001", 5);
    image[17 * SECTOR + 6] = 1;

    root = image + 20 * SECTOR;
    make_record(root, 21, (unsigned int)(sizeof(cnf) - 1),
                "SYSTEM.CNF;1", 0);
    rec_len = root[0];
    memcpy(image + 21 * SECTOR, cnf, sizeof(cnf) - 1);
    (void)rec_len;

    fp = fopen(path, "wb");
    assert(fp != NULL);
    assert(fwrite(image, 1, image_size, fp) == image_size);
    fclose(fp);
    free(image);

    assert(ps3scale_ps2_iso_read_serial(path, serial, sizeof(serial)) == 0);
    assert(strcmp(serial, "SLUS_203.12") == 0);

    assert(ps3scale_title_id_from_filename(
        "Grand Theft Auto V [BLUS31156].iso", ps3id, sizeof(ps3id)) == 0);
    assert(strcmp(ps3id, "BLUS31156") == 0);

    assert(ps3scale_ps2_serial_from_filename(
        "Black [SLUS_213.76].iso", ps2id, sizeof(ps2id)) == 0);
    assert(strcmp(ps2id, "SLUS_213.76") == 0);

    puts("PS3 SCALE ISO metadata tests: PASS");
    return 0;
}
