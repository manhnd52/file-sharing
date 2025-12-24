#ifndef UPLOAD_HANDLER_H
#define UPLOAD_HANDLER_H
#include "server.h"
#include "frame.h"
#include "cJSON.h"
#include <stdint.h>
#include "frame.h"
#include "utils/uuid.h"
#include "services/file_service.h"

#define MAX_SESSION 1024

typedef struct {
  uint8_t session_id[BYTE_UUID_SIZE];
  uint32_t last_received_chunk;
  uint32_t chunk_length;
  uint64_t total_received_size;
  uint64_t expected_file_size;
  int parent_folder_id;   // folder ID in which the file will be stored
  char file_name[256];    // original file name
  char uuid_str[37]; // UUID string representation, be transform from session_id
} UploadSession;

extern UploadSession ss[MAX_SESSION];

void upload_handler(Conn *c, Frame *data);
void upload_init_handler(Conn *c, Frame *f);
void upload_finish_handler(Conn *c, Frame *f);

#endif
