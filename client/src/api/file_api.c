#include "api/file_api.h"
#include "client.h"
#include "cJSON.h"

static int send_json_cmd(cJSON *json, Frame *resp) {
    if (!json || !resp) return -1;
    int rc = send_cmd(json, resp);
    cJSON_Delete(json);
    return rc;
}

int delete_file_api(int file_id, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "DELETE_FILE");
    cJSON_AddNumberToObject(json, "file_id", file_id);
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

int rename_file_api(int file_id, const char *new_name, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "RENAME_FILE");
    cJSON_AddNumberToObject(json, "file_id", file_id);
    cJSON_AddStringToObject(json, "new_name", new_name ? new_name : "");
    return send_json_cmd(json, resp);
}

int list_shared_items_api(Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;
    cJSON_AddStringToObject(json, "cmd", "LIST_SHARED_ITEMS");
    return send_json_cmd(json, resp);
}

