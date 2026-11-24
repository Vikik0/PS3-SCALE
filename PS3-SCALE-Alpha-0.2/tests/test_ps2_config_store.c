#include "ps3scale/ps2_config_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define LIM 64u
static void put(const char*p,const char*s){FILE*f=fopen(p,"wb");size_t n=strlen(s);assert(f);assert(fwrite(s,1,n,f)==n);assert(fclose(f)==0);}
static void eq(const char*p,const char*s){char b[128];FILE*f=fopen(p,"rb");size_t n=strlen(s);assert(f&&n<sizeof(b));assert(fread(b,1,sizeof(b),f)==n);assert(memcmp(b,s,n)==0);assert(fclose(f)==0);}
int main(void){
 char root[128],t[192],b[192],bt[192],r[192],rt[192],d[192],t2[192],b2[192],r2[192],d2[192],bigp[192],bigb[192],bigt[192],big[LIM+2];int n;
 n=snprintf(root,sizeof(root),"/tmp/ps3scale_ps2_store_%ld",(long)getpid());assert(n>0&&(size_t)n<sizeof(root));assert(mkdir(root,0700)==0);
#define P(v,s) do{n=snprintf(v,sizeof(v),"%s/%s",root,s);assert(n>0&&(size_t)n<sizeof(v));}while(0)
 P(t,"game.iso.CONFIG");P(b,"game.iso.CONFIG.bak");P(bt,"game.iso.CONFIG.backup.tmp");P(r,"game.iso.CONFIG.rollback");P(rt,"game.iso.CONFIG.restore.tmp");P(d,"game.iso.CONFIG.download");
 P(t2,"new.iso.CONFIG");P(b2,"new.iso.CONFIG.bak");P(r2,"new.iso.CONFIG.rollback");P(d2,"new.iso.CONFIG.download");P(bigp,"large.CONFIG");P(bigb,"large.CONFIG.bak");P(bigt,"large.CONFIG.tmp");
 put(t,"original-config");assert(ps3scale_ps2_config_store_probe(t,LIM,NULL)==1);assert(ps3scale_ps2_config_store_backup(t,b,bt,LIM)==0);eq(b,"original-config");
 assert(ps3scale_ps2_config_store_can_replace(t,b,LIM)==1);assert(ps3scale_ps2_config_store_backup(t,b,bt,LIM)==-2);
 put(t,"changed-config");assert(ps3scale_ps2_config_store_can_replace(t,b,LIM)==-1);put(d,"downloaded-config");
 assert(ps3scale_ps2_config_store_install(d,t,b,r,LIM)==-2);eq(t,"changed-config");eq(d,"downloaded-config");
 assert(ps3scale_ps2_config_store_restore(t,b,r,rt,LIM)==0);eq(t,"original-config");eq(r,"changed-config");
 assert(ps3scale_ps2_config_store_restore(t,b,r,rt,LIM)==1);assert(unlink(r)==0);
 assert(ps3scale_ps2_config_store_can_replace(t,b,LIM)==1);put(d,"downloaded-config");
 assert(ps3scale_ps2_config_store_install(d,t,b,r,LIM)==0);eq(t,"downloaded-config");eq(r,"original-config");
 assert(ps3scale_ps2_config_store_restore(t,b,r,rt,LIM)==-3);assert(unlink(r)==0);
 assert(ps3scale_ps2_config_store_restore(t,b,r,rt,LIM)==0);eq(t,"original-config");eq(r,"downloaded-config");
 put(d2,"new-title-config");assert(ps3scale_ps2_config_store_install(d2,t2,b2,r2,LIM)==0);eq(t2,"new-title-config");
 memset(big,'X',sizeof(big));{FILE*f=fopen(bigp,"wb");assert(f);assert(fwrite(big,1,sizeof(big),f)==sizeof(big));assert(fclose(f)==0);}
 assert(ps3scale_ps2_config_store_probe(bigp,LIM,NULL)==-1);assert(ps3scale_ps2_config_store_backup(bigp,bigb,bigt,LIM)==-4);
 assert(unlink(t)==0);assert(unlink(b)==0);assert(unlink(r)==0);assert(unlink(t2)==0);assert(unlink(bigp)==0);assert(rmdir(root)==0);
 puts("PS3 SCALE PS2 CONFIG backup/restore tests: PASS");return 0;
}
