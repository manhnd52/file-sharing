#ifndef CLIENT_FILE_API_H
#define CLIENT_FILE_API_H

#include "../../../protocol/frame.h"
#include <stdint.h>
#include <stddef.h>

int list_api(int folder_id, Frame *resp);
int ping_api(Frame *resp);
int mkdir_api(int parent_id, const char *name, Frame *resp);
int delete_folder_api(int folder_id, Frame *resp);
int delete_file_api(int file_id, Frame *resp);
int share_folder_api(int folder_id, const char *username, int permission, Frame *resp);
int share_file_api(int file_id, const char *username, int permission, Frame *resp);
int rename_item_api(int item_id, const char *item_type, const char *new_name, Frame *resp);
int list_shared_folders_api(Frame *resp);
int list_permissions_api(int target_type, int target_id, Frame *resp);
int update_permission_api(int target_type, int target_id, const char *username, int permission, Frame *resp);

#endif
