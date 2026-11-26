#ifndef PS3SCALE_PS2_CONFIG_STORE_H
#define PS3SCALE_PS2_CONFIG_STORE_H
#include <stddef.h>
int ps3scale_ps2_config_store_probe(const char *path,size_t max_bytes,size_t *file_size);
/* Create a new, exclusive, bounded temporary file from memory. */
int ps3scale_ps2_config_store_stage(const char *path,const void *data,size_t length,size_t max_bytes);
int ps3scale_ps2_config_store_can_replace(const char *target,const char *backup,size_t max_bytes);
int ps3scale_ps2_config_store_backup(const char *target,const char *backup,const char *temporary,size_t max_bytes);
int ps3scale_ps2_config_store_restore(const char *target,const char *backup,const char *rollback_path,const char *temporary,size_t max_bytes);
int ps3scale_ps2_config_store_install(const char *temporary,const char *target,const char *backup,const char *rollback_path,size_t max_bytes);
#endif
