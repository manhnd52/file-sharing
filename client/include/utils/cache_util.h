#ifndef CLIENT_UTILS_CACHE_UTIL_H
#define CLIENT_UTILS_CACHE_UTIL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define CACHE_STATE_PATH "./cache.json"
#define CACHE_SESSION_ID_MAX 64
#define CACHE_FILE_NAME_MAX 256
#define CACHE_PATH_MAX 1024
#define CACHE_CREATED_AT_MAX 64

typedef enum {
    CACHE_TRANSFER_IDLE = 0,
    CACHE_TRANSFER_ACTIVE = 1,
    CACHE_TRANSFER_DISCONNECTED = 2,
    CACHE_TRANSFER_CANCELLED = 3
} CacheTransferState;

typedef struct {
    char session_id[CACHE_SESSION_ID_MAX];
    char file_name[CACHE_FILE_NAME_MAX];
    char storage_path[CACHE_PATH_MAX];
    char created_at[CACHE_CREATED_AT_MAX];
    CacheTransferState state;
    uint64_t total_size;
    uint32_t chunk_size;
    uint32_t last_received_chunk;
} CacheDownloadingState;

typedef struct {
    char session_id[CACHE_SESSION_ID_MAX];
    uint32_t parent_folder_id;
    char file_path[CACHE_PATH_MAX];
    char created_at[CACHE_CREATED_AT_MAX];
    CacheTransferState state;
    uint64_t total_size;
    uint32_t chunk_size;
    uint32_t last_sent_chunk;
} CacheUploadingState;

typedef struct {
    CacheDownloadingState downloading;
    CacheUploadingState uploading;
} CacheState;

int cache_load_file(const char *path, CacheState *out);
int cache_load_default(CacheState *out);
int cache_save_file(const char *path, const CacheState *data);
int cache_save_default(const CacheState *data);
int cache_update_downloading(const CacheDownloadingState *state);
int cache_update_uploading(const CacheUploadingState *state);
int cache_reset_downloading(void);
int cache_reset_uploading(void);
int cache_set_downloading_transfer_state(CacheTransferState state);
int cache_set_uploading_transfer_state(CacheTransferState state);

void cache_init_downloading_state(const char *session_id,
                                  const char *file_name,
                                  const char *storage_path,
                                  uint64_t total_size,
                                  uint32_t chunk_size);
void cache_init_uploading_state(const char *session_id,
                                const int parent_folder_id,
                                const char *file_path,
                                uint64_t total_size,
                                uint32_t chunk_size);
int cache_update_uploading_last_sent_chunk(int chunk_index);
int cache_update_downloading_last_received_chunk(int chunk_index);

CacheTransferState cache_get_downloading_transfer_state();
CacheTransferState cache_get_uploading_transfer_state();

#endif /* CLIENT_UTILS_CACHE_UTIL_H */
