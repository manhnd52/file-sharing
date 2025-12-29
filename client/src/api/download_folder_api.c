#include "api/download_api.h"
#include "api/file_api.h"
#include "cJSON.h"
#include "utils/file_system_util.h"
#include "api/folder_api.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int download_folder_api(const char* storage_path, int folder_id, Frame* res) {
    if (!storage_path || storage_path[0] == '\0' || folder_id <= 0 || !res) {
        return -1;
    }

    Frame list_resp = {0};
    int rc = list_api(folder_id, &list_resp);
    if (rc != 0) {
        return rc;
    }

    if (list_resp.msg_type != MSG_RESPOND || list_resp.header.resp.status != STATUS_OK ||
        list_resp.payload_len == 0) {
        *res = list_resp;
        return -1;
    }

    char* payload_copy = (char*)malloc(list_resp.payload_len + 1);
    if (!payload_copy) {
        return -1;
    }

    memcpy(payload_copy, list_resp.payload, list_resp.payload_len);
    payload_copy[list_resp.payload_len] = '\0';

    cJSON* payload_json = cJSON_Parse(payload_copy);
    free(payload_copy);
    if (!payload_json) {
        return -1;
    }

    cJSON* items = cJSON_GetObjectItemCaseSensitive(payload_json, "items");
    cJSON* folder_name_item = cJSON_GetObjectItemCaseSensitive(payload_json, "folder_name");
    if (!folder_name_item || !cJSON_IsString(folder_name_item) ||
        folder_name_item->valuestring[0] == '\0') {
        cJSON_Delete(payload_json);
        return -1;
    }

    // Nối storage path và tên folder
    char folder_path[2048] = {0};

    if (!join_path(folder_path, sizeof(folder_path), storage_path,
                   folder_name_item->valuestring)) {
        cJSON_Delete(payload_json);
        return -1;
    }

    if (!ensure_directory(folder_path)) {
        cJSON_Delete(payload_json);
        return -1;
    }

    if (!items || !cJSON_IsArray(items)) {
        cJSON_Delete(payload_json);
        return 0;
    }

    cJSON* item = NULL;
    cJSON_ArrayForEach(item, items) {
        cJSON* type_item = cJSON_GetObjectItemCaseSensitive(item, "type");
        cJSON* id_item = cJSON_GetObjectItemCaseSensitive(item, "id");
        if (!type_item || !cJSON_IsString(type_item) || type_item->valuestring[0] == '\0' ||
            !id_item || !cJSON_IsNumber(id_item)) {
            continue;
        }

        int child_id = id_item->valueint;
        const char* type_value = type_item->valuestring;
        if (strcmp(type_value, "file") == 0) {
            rc = download_file_api(folder_path, child_id, res);
        } else if (strcmp(type_value, "folder") == 0) {
            rc = download_folder_api(folder_path, child_id, res);
        } else {
            continue;
        }

        if (rc != 0) {
            cJSON_Delete(payload_json);
            return rc;
        }
    }

    cJSON_Delete(payload_json);
    if (res && res->msg_type == 0) {
        // Nếu chưa có phản hồi nào từ server (folder rỗng), tạo frame OK giả
        build_respond_frame(res, 0, STATUS_OK, "{}");
    }
    return 0;
}
