#include "ps3scale/ps2_config_db.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char *path = "/tmp/ps3scale_ps2_config_index_test.tsv";
    ps3scale_ps2_config_db_t db = {0};
    const ps3scale_ps2_config_entry_t *found;
    char normalized[PS3SCALE_PS2_CONFIG_ID_MAX];
    char url[512];
    FILE *fp = fopen(path, "wb");

    assert(fp != NULL);
    fputs("# type\ttitle_id\ttitle\n", fp);
    fputs("CUSTOM\tSLUS_209.09\tCrash Bandicoot: Twinsanity\n", fp);
    fputs("NET\tSLUS_209.09\tCrash Bandicoot: Twinsanity (NETEMU)\n", fp);
    fputs("CUSTOM\tSLES_525.68\tCrash Twinsanity\n", fp);
    fputs("ENC\tNPUD21770\tExample PS2 Classic\n", fp);
    fclose(fp);

    assert(ps3scale_ps2_config_normalize_id(
        "slus-20909", normalized, sizeof(normalized)) == 0);
    assert(strcmp(normalized, "SLUS_209.09") == 0);
    assert(ps3scale_ps2_config_normalize_id(
        "NPUD21770", normalized, sizeof(normalized)) == 0);
    assert(strcmp(normalized, "NPUD21770") == 0);

    assert(ps3scale_ps2_config_db_load(path, &db) == 4);
    found = ps3scale_ps2_config_db_find(&db, "SLUS-20909", "NET");
    assert(found != NULL);
    assert(strcmp(found->type, "CUSTOM") == 0);
    /* webMAN checks CUSTOM before NET for a PS2 ISO. */
    assert(strcmp(found->title, "Crash Bandicoot: Twinsanity") == 0);

    found = ps3scale_ps2_config_db_find(&db, "NPUD21770", "ENC");
    assert(found != NULL && strcmp(found->type, "ENC") == 0);

    found = ps3scale_ps2_config_db_find(&db, "SLES52568", "NET");
    assert(found != NULL && strcmp(found->type, "CUSTOM") == 0);
    assert(ps3scale_ps2_config_build_url(found, url, sizeof(url)) == 0);
    assert(strstr(url, "/CUSTOM/SLES_525.68.CONFIG") != NULL);

    ps3scale_ps2_config_db_free(&db);
    remove(path);
    puts("PS3 SCALE PS2 CONFIG database tests: PASS");
    return 0;
}
