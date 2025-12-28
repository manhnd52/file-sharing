#ifndef CLIENT_FILE_API_H
#define CLIENT_FILE_API_H

#include "../../../protocol/frame.h"

int list_api(int folder_id, Frame *resp);
int ping_api(Frame *resp);
int download_file_api(const char* storage_path, int folder_id, Frame* res);

#endif
