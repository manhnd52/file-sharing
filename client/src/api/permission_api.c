#include "api/permission_api.h"
#include "client.h"
#include "cJSON.h"

static int send_json_cmd(cJSON *json, Frame *resp) {
    if (!json || !resp) return -1;
    int rc = send_cmd(json, resp);
    cJSON_Delete(json);
    return rc;
}

int list_folder_permissions_api(int folder_id, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "LIST_PERMISSIONS");
    cJSON_AddNumberToObject(json, "target_type", 1);
    cJSON_AddNumberToObject(json, "target_id", folder_id);
    return send_json_cmd(json, resp);
}

int list_file_permissions_api(int file_id, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "LIST_PERMISSIONS");
    cJSON_AddNumberToObject(json, "target_type", 0);
    cJSON_AddNumberToObject(json, "target_id", file_id);
    return send_json_cmd(json, resp);
}

int update_folder_permission_api(int folder_id, const char *username, int permission, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "UPDATE_PERMISSION");
    cJSON_AddNumberToObject(json, "target_type", 1);
    cJSON_AddNumberToObject(json, "target_id", folder_id);
    cJSON_AddStringToObject(json, "username", username ? username : "");
    cJSON_AddNumberToObject(json, "permission", permission);
    return send_json_cmd(json, resp);
}

int update_file_permission_api(int file_id, const char *username, int permission, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "UPDATE_PERMISSION");
    cJSON_AddNumberToObject(json, "target_type", 0);
    cJSON_AddNumberToObject(json, "target_id", file_id);
    cJSON_AddStringToObject(json, "username", username ? username : "");
    cJSON_AddNumberToObject(json, "permission", permission);
    return send_json_cmd(json, resp);
}
