#ifndef DOWNLOAD_API_H
#define DOWNLOAD_API_H

#include "frame.h"

int download_file_api(const char* storage_path, int file_id, Frame* res);
int download_folder_api(const char* storage_path, int folder_id, Frame* res);
int download_cancel_api(const char* session_id, Frame* res);

#endif
