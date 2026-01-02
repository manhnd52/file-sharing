#ifndef DOWNLOAD_HANDLER_H
#define DOWNLOAD_HANDLER_H

#include "server.h"
#include "frame.h"
#include "cJSON.h"
#include <stdint.h>
#include "frame.h"
#include "utils/uuid.h"
#include "services/file_service.h"

typedef enum {
    DOWNLOAD_INIT = 0,
    DOWNLOAD_DOWNLOADING = 1,
    DOWNLOAD_PAUSED = 2,
    DOWNLOAD_COMPLETED = 3,
    DOWNLOAD_FAILED = 4,
    DOWNLOAD_CANCELED = 5
} DownloadState;

typedef struct {
    uint8_t session_id[BYTE_UUID_SIZE];     // session ID để theo dõi phiên download
    uint32_t last_requested_chunk;          // chunk cuối cùng đã request/nhận
    uint32_t chunk_size;                    // kích thước mỗi chunk
    DownloadState state;                    // trạng thái hiện tại của session
    uint64_t total_file_size;               // kích thước file dự kiến
    int file_id;                            // downloading file id
    char file_hashcode[37];                 // UUID string lấy nguồn file trong storage/data
} DownloadSession;

void download_chunk_handler(Conn *c, Frame *req);
void download_init_handler(Conn *c, Frame *f);
void download_finish_handler(Conn *c, Frame *f);
void download_cancel_handler(Conn *c, Frame *req);
void download_resume_handler(Conn *c, Frame *req);
#endif
