#ifndef CLIENT_PERMISSION_API_H
#define CLIENT_PERMISSION_API_H

#include "../../../protocol/frame.h"

int list_folder_permissions_api(int folder_id, Frame *resp);
int list_file_permissions_api(int file_id, Frame *resp);
int update_folder_permission_api(int folder_id, const char *username, int permission, Frame *resp);
int update_file_permission_api(int file_id, const char *username, int permission, Frame *resp);

#endif
