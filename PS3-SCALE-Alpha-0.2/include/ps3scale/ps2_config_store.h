#ifndef PS3SCALE_PS2_CONFIG_STORE_H
#define PS3SCALE_PS2_CONFIG_STORE_H
#include <stddef.h>
int ps3scale_ps2_config_store_probe(const char *path,size_t max_bytes,size_t *file_size);
/* Create a new, exclusive, bounded temporary file from memory. */
int ps3scale_ps2_config_store_stage(const char *path,const void *data,size_t length,size_t max_bytes);
/* Returns 1 when the file matches the local CONFIG command subset, 0 when it does not, -1 on I/O error. This is syntax detection, not proof of provenance. */
int ps3scale_ps2_config_store_is_generated(const char *path,size_t max_bytes);
/* Read an existing bounded CONFIG file without changing it. */
int ps3scale_ps2_config_store_read(const char *path,void *buffer,
                                   size_t capacity,size_t max_bytes,
                                   size_t *file_size);
int ps3scale_ps2_config_store_can_replace(const char *target,const char *backup,size_t max_bytes);
int ps3scale_ps2_config_store_backup(const char *target,const char *backup,const char *temporary,size_t max_bytes);
int ps3scale_ps2_config_store_restore(const char *target,const char *backup,const char *rollback_path,const char *temporary,size_t max_bytes);
int ps3scale_ps2_config_store_install(const char *temporary,const char *target,const char *backup,const char *rollback_path,size_t max_bytes);
#endif
