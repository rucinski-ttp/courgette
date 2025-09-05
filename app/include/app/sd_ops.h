#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int sd_ops_init(void);

/* Mount point for the SD card */
extern const char* SD_MOUNT_POINT;

/* Synchronous wrappers executing on an internal worker thread. */
int sd_format(void);
int sd_list(const char* rel_path, uint8_t* out, uint32_t* inout_len);
int sd_read(const char* rel_path, uint32_t offset, uint8_t* out, uint32_t* inout_len);
int sd_write(const char* rel_path, uint32_t offset, const uint8_t* data, uint32_t len);
int sd_rename(const char* old_rel_path, const char* new_rel_path);
int sd_delete(const char* rel_path);
int sd_mkdir(const char* rel_path);
int sd_stat_size(const char* rel_path, uint32_t* out_size, int* out_is_dir);
int sd_checksum_crc32(const char* rel_path, uint32_t* out_crc);
int sd_status(void);

#ifdef __cplusplus
}
#endif
