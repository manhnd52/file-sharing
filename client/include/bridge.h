#ifndef FS_CLIENT_BRIDGE_H
#define FS_CLIENT_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include "../../protocol/frame.h"

int fs_connect(const char *host, uint16_t port, int timeout_seconds);

void fs_disconnect(void);

int fs_login_json(const char *username, const char *password,
                  char *out_buf, size_t out_len);

int fs_register_json(const char *username, const char *password,
                     char *out_buf, size_t out_len);
int fs_auth_json(const char *token, char *out_buf, size_t out_len);
int fs_logout_json(char *out_buf, size_t out_len);

int fs_list_json(int folder_id, char *out_buf, size_t out_len);
int fs_list_shared_items_json(char *out_buf, size_t out_len);

int fs_mkdir_json(int parent_id, const char *name, char *out_buf, size_t out_len);

int fs_delete_folder_json(int folder_id, char *out_buf, size_t out_len);
int fs_delete_file_json(int file_id, char *out_buf, size_t out_len);

int fs_share_folder_json(int folder_id, const char *username, int permission,
                         char *out_buf, size_t out_len);
int fs_share_file_json(int file_id, const char *username, int permission,
                       char *out_buf, size_t out_len);
int fs_rename_folder_json(int folder_id, const char *new_name,
                        char *out_buf, size_t out_len);
int fs_rename_file_json(int file_id, const char *new_name,
                        char *out_buf, size_t out_len);
int fs_list_folder_permissions_json(int folder_id, char *out_buf, size_t out_len);
int fs_list_file_permissions_json(int file_id, char *out_buf, size_t out_len);
int fs_update_folder_permission_json(int folder_id, const char *username, int permission,
                              char *out_buf, size_t out_len);
int fs_update_file_permission_json(int file_id, const char *username, int permission,
                              char *out_buf, size_t out_len);
int fs_upload_file_json(const char *file_path, int parent_folder_id, char *out_buf, size_t out_len);
int fs_download_file_json(const char *dest_dir, int file_id, char *out_buf, size_t out_len);
int fs_download_folder_json(const char *dest_dir, int folder_id, char *out_buf, size_t out_len);
int fs_cancel_download_json(const char *session_id, char *out_buf, size_t out_len);
int fs_cancel_upload_json(const char *session_id, char *out_buf, size_t out_len);

#endif
