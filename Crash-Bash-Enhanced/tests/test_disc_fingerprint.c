#define _POSIX_C_SOURCE 200809L
#include "disc_fingerprint.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void write_file(const char *path,const char *data)
{
    FILE *f=fopen(path,"wb"); assert(f); assert(fwrite(data,1,strlen(data),f)==strlen(data)); assert(fclose(f)==0);
}
static void write_cue(const char *path,const char *track)
{
    FILE *f=fopen(path,"wb"); assert(f);
    assert(fprintf(f,"REM TEST\r\nFILE \"%s\" BINARY\r\n TRACK 01 MODE2/2352\r\n INDEX 01 00:00:00\r\n",track)>0);
    assert(fclose(f)==0);
}
int main(void)
{
    char tmpl[]="/tmp/cb-disc-fingerprint-XXXXXX", *root=mkdtemp(tmpl);
    char a[512],b[512],c[512],ca[512],cb[512],cc[512],cm[512];
    char ha[17],hb[17],hc[17];
    assert(root);
    snprintf(a,sizeof(a),"%s/a.bin",root); snprintf(b,sizeof(b),"%s/b.bin",root);
    snprintf(c,sizeof(c),"%s/c.bin",root); snprintf(ca,sizeof(ca),"%s/a.cue",root);
    snprintf(cb,sizeof(cb),"%s/b.cue",root); snprintf(cc,sizeof(cc),"%s/c.cue",root);
    snprintf(cm,sizeof(cm),"%s/missing.cue",root);
    write_file(a,"same Crash Bash track bytes"); write_file(b,"same Crash Bash track bytes");
    write_file(c,"different region or revision");
    write_cue(ca,"a.bin"); write_cue(cb,"b.bin"); write_cue(cc,"c.bin"); write_cue(cm,"no.bin");
    assert(cb_disc_fingerprint(ca,ha,sizeof(ha))==0);
    assert(cb_disc_fingerprint(cb,hb,sizeof(hb))==0);
    assert(cb_disc_fingerprint(cc,hc,sizeof(hc))==0);
    assert(strlen(ha)==16 && strcmp(ha,hb)==0 && strcmp(ha,hc)!=0);
    assert(cb_disc_fingerprint(cm,hc,sizeof(hc))==-1);
    assert(unlink(a)==0 && unlink(b)==0 && unlink(c)==0);
    assert(unlink(ca)==0 && unlink(cb)==0 && unlink(cc)==0 && unlink(cm)==0);
    assert(rmdir(root)==0);
    puts("Disc fingerprint tests passed.");
    return 0;
}
