#ifndef FILE_SERVICE_H
#define FILE_SERVICE_H

#include <stdint.h>
#include <cJSON.h>

// Inserts a new row into the files table and returns the new record ID on success.
// Returns 0 on failure.
int folder_get_or_create_user_root(int user_id);
int parseFolderPath(const int userId, const char* folderPath);
int file_save_metadata(int owner_id, int parent_folder_id, const char* file_name, const char* storage_hash, uint64_t size);
int folder_save_metadata(int owner_id, int parent_folder_id, const char* new_folder_name);

// Higher-level folder/file operations (with ownership checks)
int folder_create(int owner_id, int parent_id, const char* name, int* out_id);
int folder_delete_owned(int owner_id, int folder_id); // 0 ok, -1 not found/not owner, -2 cannot delete root, -3 db error
int folder_share_with_user(int owner_id, int folder_id, const char* username, int permission); // 0 ok, -1 not owner/not found, -2 user not found, -3 db
int file_share_with_user(int owner_id, int file_id, const char* username, int permission); // 0 ok, -1 not owner/not found, -2 user not found, -3 db
int item_rename(int owner_id, int item_id, const char* item_type, const char* new_name); // 0 ok, -1 not found/not owner, -2 cannot rename root/invalid type, -3 db
int file_get_owner(int file_id);
cJSON* list_permissions(int target_type, int target_id);
int update_permission(int owner_id, int target_type, int target_id, const char* username, int permission); // 0 ok, -1 not owner/not found, -2 user not found, -3 db
void purge_permissions_for_folder_subtree(int folder_id);

// crud folder, file metadata
cJSON* get_folder_info(int folder_id);
cJSON* get_file_info(int file_id);
cJSON* list_owned_top_folders(int owner_id);
cJSON* list_shared_folders(int user_id);

int delete_folder(int folder_id);
int delete_file(int file_id);

#endif // FILE_SERVICE_H
