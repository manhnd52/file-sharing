#include "api/file_api.h"

#include "client.h"
#include "cJSON.h"
#include <string.h>

static int send_json_cmd(cJSON *json, Frame *resp) {
    if (!json || !resp) return -1;
    int rc = send_cmd(json, resp);
    cJSON_Delete(json);
    return rc;
}

int list_api(int folder_id, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "LIST");
    if (folder_id > 0) {
        cJSON_AddNumberToObject(json, "folder_id", folder_id);
    }
    return send_json_cmd(json, resp);
}

int ping_api(Frame *resp) {
    return send_simple_cmd("PING", resp);
}

int mkdir_api(int parent_id, const char *name, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "MKDIR");
    cJSON_AddNumberToObject(json, "parent_id", parent_id);
    cJSON_AddStringToObject(json, "name", name ? name : "");
    return send_json_cmd(json, resp);
}

int delete_folder_api(int folder_id, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "DELETE_FOLDER");
    cJSON_AddNumberToObject(json, "folder_id", folder_id);
    return send_json_cmd(json, resp);
}

int delete_file_api(int file_id, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "DELETE_FILE");
    cJSON_AddNumberToObject(json, "file_id", file_id);
    return send_json_cmd(json, resp);
}

int share_folder_api(int folder_id, const char *username, int permission, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "SHARE_FOLDER");
    cJSON_AddNumberToObject(json, "folder_id", folder_id);
    cJSON_AddStringToObject(json, "username", username ? username : "");
    cJSON_AddNumberToObject(json, "permission", permission);
    return send_json_cmd(json, resp);
}

int share_file_api(int file_id, const char *username, int permission, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "SHARE_FILE");
    cJSON_AddNumberToObject(json, "file_id", file_id);
    cJSON_AddStringToObject(json, "username", username ? username : "");
    cJSON_AddNumberToObject(json, "permission", permission);
    return send_json_cmd(json, resp);
}

int rename_item_api(int item_id, const char *item_type, const char *new_name, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "RENAME_ITEM");
    cJSON_AddNumberToObject(json, "item_id", item_id);
    cJSON_AddStringToObject(json, "item_type", item_type ? item_type : "");
    cJSON_AddStringToObject(json, "new_name", new_name ? new_name : "");
    return send_json_cmd(json, resp);
}

int list_shared_folders_api(Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "LIST_SHARED_FOLDERS");
    return send_json_cmd(json, resp);
}

int list_permissions_api(int target_type, int target_id, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "LIST_PERMISSIONS");
    cJSON_AddNumberToObject(json, "target_type", target_type);
    cJSON_AddNumberToObject(json, "target_id", target_id);
    return send_json_cmd(json, resp);
}

int update_permission_api(int target_type, int target_id, const char *username, int permission, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "UPDATE_PERMISSION");
    cJSON_AddNumberToObject(json, "target_type", target_type);
    cJSON_AddNumberToObject(json, "target_id", target_id);
    cJSON_AddStringToObject(json, "username", username ? username : "");
    cJSON_AddNumberToObject(json, "permission", permission);
    return send_json_cmd(json, resp);
}
