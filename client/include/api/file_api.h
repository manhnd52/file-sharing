#ifndef FILE_API_H
#define FILE_API_H

#include "frame.h"

int delete_file_api(int file_id, Frame *resp);
int share_file_api(int file_id, const char *username, int permission, Frame *resp);
int rename_file_api(int file_id, const char *new_name, Frame *resp);
int list_shared_items_api(Frame *resp);
int search_file_api(const char *keyword, Frame *resp);
int copy_file_api(int file_id, int dest_folder_id, Frame *resp);

#endif
