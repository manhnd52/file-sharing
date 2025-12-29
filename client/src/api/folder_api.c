#include "api/folder_api.h"
#include "client.h"
#include "cJSON.h"

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

int share_folder_api(int folder_id, const char *username, int permission, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "SHARE_FOLDER");
    cJSON_AddNumberToObject(json, "folder_id", folder_id);
    cJSON_AddStringToObject(json, "username", username ? username : "");
    cJSON_AddNumberToObject(json, "permission", permission);
    return send_json_cmd(json, resp);
}

int rename_folder_api(int folder_id, const char *new_name, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "RENAME_FOLDER");
    cJSON_AddNumberToObject(json, "folder_id", folder_id);
    cJSON_AddStringToObject(json, "new_name", new_name ? new_name : "");
    return send_json_cmd(json, resp);
}
