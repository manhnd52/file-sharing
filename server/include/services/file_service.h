#ifndef FILE_SERVICE_H
#define FILE_SERVICE_H

#include <stdint.h>
#include <cJSON.h>

// File metadata operations
int file_save_metadata(int owner_id, int parent_folder_id, const char* file_name, const char* storage_hash, uint64_t size);
cJSON* get_file_info(int file_id);

// Higher-level file operations
int file_share_with_user(int owner_id, int file_id, const char* username, int permission); // 0 ok, -1 not owner/not found, -2 user not found, -3 db
int delete_file(int file_id);
int file_get_owner(int file_id);
int file_rename(int owner_id, int file_id, const char* new_name);    // 0 ok, -1 not found/not owner, -2 invalid, -3 db

#endif // FILE_SERVICE_H
