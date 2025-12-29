#ifndef CLIENT_FILE_API_H
#define CLIENT_FILE_API_H

#include "../../../protocol/frame.h"

int delete_file_api(int file_id, Frame *resp);
int share_file_api(int file_id, const char *username, int permission, Frame *resp);
int rename_file_api(int file_id, const char *new_name, Frame *resp);
int list_shared_items_api(Frame *resp);

#endif
