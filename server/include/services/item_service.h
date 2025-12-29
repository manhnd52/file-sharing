#ifndef ITEM_SERVICE_H
#define ITEM_SERVICE_H

#include <cJSON.h>

cJSON* list_permissions(int target_type, int target_id);
int update_permission(int owner_id, int target_type, int target_id, const char* username, int permission); // 0 ok, -1 not owner/not found, -2 user not found, -3 db
cJSON* list_shared_items(int user_id);

#endif // ITEM_SERVICE_H
