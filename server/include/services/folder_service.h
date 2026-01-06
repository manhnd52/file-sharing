#ifndef FOLDER_SERVICE_H
#define FOLDER_SERVICE_H

#include <stdint.h>
#include <cJSON.h>

int folder_get_or_create_user_root(int user_id);
int folder_save_metadata(int owner_id, int parent_folder_id, const char* new_folder_name);
int folder_create(int owner_id, int parent_id, const char* name, int* out_id);
int folder_share_with_user(int owner_id, int folder_id, const char* username, int permission); 
int folder_rename(int owner_id, int folder_id, const char* new_name); // 0 ok, -1 not found/not owner, -2 cannot rename root/invalid, -3 db
int delete_folder(int folder_id, int actor_id); 

cJSON* get_folder_info(int folder_id);
cJSON* search_folders(int user_id, const char *keyword);
int copy_folder(int actor_id, int src_folder_id, int dest_folder_id, int *out_new_id);

#endif 
