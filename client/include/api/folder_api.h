#ifndef CLIENT_FOLDER_API_H
#define CLIENT_FOLDER_API_H

#include "../../../protocol/frame.h"

int list_api(int folder_id, Frame *resp);
int mkdir_api(int parent_id, const char *name, Frame *resp);
int delete_folder_api(int folder_id, Frame *resp);
int share_folder_api(int folder_id, const char *username, int permission, Frame *resp);
int rename_folder_api(int folder_id, const char *new_name, Frame *resp);
int search_api_folder(const char *keyword, Frame *resp);
int copy_folder_api(int folder_id, int dest_folder_id, Frame *resp);

#endif
