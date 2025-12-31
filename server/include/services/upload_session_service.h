#include <stdint.h>
#include "handlers/upload_handler.h"

typedef enum {
    UPLOAD_INIT = 0,
    UPLOAD_UPLOADING = 1,
    UPLOAD_PAUSED = 2,
    UPLOAD_COMPLETED = 3,
    UPLOAD_FAILED = 4,
    UPLOAD_CANCELED = 5
} UploadState;

int us_create(
    const uint8_t session_id[16],
    const char *uuid_str,
    uint32_t chunk_size,
    uint64_t expected_file_size,
    int parent_folder_id,
    const char *file_name
);

int us_get(
    const uint8_t session_id[16],
    UploadSession *out
);

int us_update_state(
    const uint8_t session_id[16],
    UploadState new_state
);

int us_update_progress(
    const uint8_t session_id[16],
    uint32_t last_received_chunk,
    uint64_t total_received_size
);

int us_delete(
    const uint8_t session_id[16]
);