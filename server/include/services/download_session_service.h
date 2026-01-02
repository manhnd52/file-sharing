#include <stdint.h>
#include "handlers/download_handler.h"

int ds_create(
    const uint8_t session_id[16],
    int chunk_size,
    uint64_t total_file_size,
    int file_id,
    const char *file_hashcode
);

int ds_get(
    const uint8_t session_id[16],
    DownloadSession *out
);

int ds_update_state(
    const uint8_t session_id[16],
    DownloadState new_state
);

int ds_update_progress(
    const uint8_t session_id[16],
    uint32_t last_requested_chunk
);

int ds_delete(
    const uint8_t session_id[16]
);