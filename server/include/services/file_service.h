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


// crud folder, file metadata
cJSON* get_folder_info(int folder_id);
cJSON* get_file_info(int file_id);

int delete_folder(int folder_id);
int delete_file(int file_id);

#endif // FILE_SERVICE_H
