#ifndef UPLOAD_API_H
#define UPLOAD_API_H

#include "frame.h"

int upload_file_api(const char* file_path, int parent_folder_id, Frame* res);

#endif
