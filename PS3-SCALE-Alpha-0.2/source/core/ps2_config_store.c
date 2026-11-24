#include "ps3scale/ps2_config_store.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define COPY_CHUNK 4096u
int ps3scale_ps2_config_store_probe(const char *path,size_t max_bytes,size_t *file_size){
 struct stat st;if(file_size)*file_size=0;if(!path||!path[0]||!max_bytes)return -1;
 if(stat(path,&st)!=0)return errno==ENOENT?0:-1;
 if(!S_ISREG(st.st_mode)||st.st_size<=0||(unsigned long long)st.st_size>(unsigned long long)max_bytes)return -1;
 if(file_size)*file_size=(size_t)st.st_size;return 1;
}
static int files_equal(const char *a_path,const char *b_path,size_t max_bytes){
 FILE *a=NULL,*b=NULL;unsigned char ab[COPY_CHUNK],bb[COPY_CHUNK];size_t as=0,bs=0,total=0;int result=-1;
 if(ps3scale_ps2_config_store_probe(a_path,max_bytes,&as)!=1||ps3scale_ps2_config_store_probe(b_path,max_bytes,&bs)!=1)return -1;
 if(as!=bs)return 0;a=fopen(a_path,"rb");b=fopen(b_path,"rb");if(!a||!b)goto done;
 for(;;){size_t na=fread(ab,1,sizeof(ab),a),nb=fread(bb,1,sizeof(bb),b);
  if(na!=nb||(na&&memcmp(ab,bb,na)!=0)){result=0;goto done;}total+=na;if(total>max_bytes)goto done;
  if(na<sizeof(ab)){if(ferror(a)||ferror(b))goto done;result=total==as?1:-1;goto done;}}
done:if(a)fclose(a);if(b)fclose(b);return result;
}
static int copy_bounded(const char *src_path,const char *dst_path,size_t max_bytes){
 FILE *src=NULL,*dst=NULL;unsigned char buf[COPY_CHUNK];size_t expected=0,total=0;int ok=0;
 if(ps3scale_ps2_config_store_probe(src_path,max_bytes,&expected)!=1||ps3scale_ps2_config_store_probe(dst_path,max_bytes,NULL)!=0)return -1;
 src=fopen(src_path,"rb");if(!src)goto done;dst=fopen(dst_path,"wb");if(!dst)goto done;
 for(;;){size_t n=fread(buf,1,sizeof(buf),src);if(n){if(n>max_bytes||total>max_bytes-n||fwrite(buf,1,n,dst)!=n)goto done;total+=n;}
  if(n<sizeof(buf)){if(ferror(src))goto done;break;}}
 if(total!=expected||fflush(dst)!=0)goto done;ok=1;
done:if(src&&fclose(src)!=0)ok=0;if(dst&&fclose(dst)!=0)ok=0;if(!ok)unlink(dst_path);return ok?0:-1;
}
int ps3scale_ps2_config_store_can_replace(const char *target,const char *backup,size_t max_bytes){
 int ts=ps3scale_ps2_config_store_probe(target,max_bytes,NULL);if(ts==0)return 0;if(ts!=1)return -1;
 if(ps3scale_ps2_config_store_probe(backup,max_bytes,NULL)!=1)return -1;
 return files_equal(target,backup,max_bytes)==1?1:-1;
}
int ps3scale_ps2_config_store_backup(const char *target,const char *backup,const char *tmp,size_t max_bytes){
 int state;if(!target||!backup||!tmp||!target[0]||!backup[0]||!tmp[0]||!strcmp(target,backup)||!strcmp(target,tmp)||!strcmp(backup,tmp))return -1;
 if(ps3scale_ps2_config_store_probe(target,max_bytes,NULL)!=1)return -4;
 state=ps3scale_ps2_config_store_probe(backup,max_bytes,NULL);if(state==1)return -2;
 if(state!=0||ps3scale_ps2_config_store_probe(tmp,max_bytes,NULL)!=0)return -3;
 if(copy_bounded(target,tmp,max_bytes)!=0)return -5;
 if(files_equal(target,tmp,max_bytes)!=1){unlink(tmp);return -5;}
 if(ps3scale_ps2_config_store_probe(backup,max_bytes,NULL)!=0||rename(tmp,backup)!=0){unlink(tmp);return -5;}
 return 0;
}
int ps3scale_ps2_config_store_restore(const char *target,const char *backup,const char *rollback,const char *tmp,size_t max_bytes){
 int ts;if(!target||!backup||!rollback||!tmp||!target[0]||!backup[0]||!rollback[0]||!tmp[0]||
 !strcmp(target,backup)||!strcmp(target,rollback)||!strcmp(target,tmp)||!strcmp(backup,rollback)||!strcmp(backup,tmp)||!strcmp(rollback,tmp))return -1;
 if(ps3scale_ps2_config_store_probe(backup,max_bytes,NULL)!=1)return -2;
 ts=ps3scale_ps2_config_store_probe(target,max_bytes,NULL);if(ts<0)return -4;
 if(ts==1&&files_equal(target,backup,max_bytes)==1)return 1;
 if(ps3scale_ps2_config_store_probe(tmp,max_bytes,NULL)!=0)return -3;
 if(ts==1&&ps3scale_ps2_config_store_probe(rollback,max_bytes,NULL)!=0)return -3;
 if(copy_bounded(backup,tmp,max_bytes)!=0||files_equal(backup,tmp,max_bytes)!=1){unlink(tmp);return -5;}
 if(ts==1&&rename(target,rollback)!=0){unlink(tmp);return -5;}
 if(rename(tmp,target)!=0){if(ts==1)rename(rollback,target);unlink(tmp);return -5;}return 0;
}
int ps3scale_ps2_config_store_install(const char *tmp,const char *target,const char *backup,const char *rollback,size_t max_bytes){
 int ts;if(!tmp||!target||!backup||!rollback||!tmp[0]||!target[0]||!backup[0]||!rollback[0]||
 !strcmp(tmp,target)||!strcmp(tmp,backup)||!strcmp(tmp,rollback)||!strcmp(target,backup)||!strcmp(target,rollback)||!strcmp(backup,rollback))return -1;
 if(ps3scale_ps2_config_store_probe(tmp,max_bytes,NULL)!=1)return -4;
 ts=ps3scale_ps2_config_store_probe(target,max_bytes,NULL);if(ts<0)return -4;
 if(ts==1){if(ps3scale_ps2_config_store_can_replace(target,backup,max_bytes)!=1)return -2;
  if(ps3scale_ps2_config_store_probe(rollback,max_bytes,NULL)!=0)return -3;
  if(rename(target,rollback)!=0)return -4;if(rename(tmp,target)!=0){rename(rollback,target);return -4;}return 0;}
 return rename(tmp,target)==0?0:-4;
}
