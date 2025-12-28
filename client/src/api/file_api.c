#include "api/file_api.h"

#include "client.h"
#include "cJSON.h"

<<<<<<< HEAD
#include <stdbool.h>
=======
>>>>>>> 4ef72e0 (apply new git ignore)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int send_list_request(int folder_id, Frame *resp) {
    if (!resp) {
        return -1;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return -1;
    }

    cJSON_AddStringToObject(json, "cmd", "LIST");
    if (folder_id > 0) {
        cJSON_AddNumberToObject(json, "folder_id", folder_id);
    }

    int rc = send_cmd(json, resp);
    cJSON_Delete(json);
    return rc;
}

// LIST: get folder info and its item
int list_api(int folder_id, Frame *resp) {
    return send_list_request(folder_id, resp);
}

int ping_api(Frame *resp) {
    return send_simple_cmd("PING", resp);
}

int download_file_api(const char* storage_path, int folder_id, Frame* res) {
    if (!storage_path || storage_path[0] == '\0' || folder_id <= 0 || !res) {
        return -1;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return -1;
    }

    cJSON_AddStringToObject(json, "cmd", "DOWNLOAD_INIT");
    cJSON_AddNumberToObject(json, "file_id", folder_id);
    cJSON_AddNumberToObject(json, "chunk_size", MAX_PAYLOAD);

    int rc = send_cmd(json, res);
    cJSON_Delete(json);

    if (rc != 0) {
        return rc;
    }

    if (res->msg_type != MSG_RESPOND || res->header.resp.status != STATUS_OK ||
        res->payload_len == 0) {
        return -1;
    }

    char *payload_copy = (char *)malloc(res->payload_len + 1);
    if (!payload_copy) {
        return -1;
    }

    memcpy(payload_copy, res->payload, res->payload_len);
    payload_copy[res->payload_len] = '\0';

    cJSON *payload_json = cJSON_Parse(payload_copy);
    free(payload_copy);
    payload_copy = NULL;

    if (!payload_json) {
        return -1;
    }

    cJSON *session_item = cJSON_GetObjectItemCaseSensitive(payload_json, "sessionId");
    cJSON *chunk_size_item = cJSON_GetObjectItemCaseSensitive(payload_json, "chunk_size");
    cJSON *file_size_item = cJSON_GetObjectItemCaseSensitive(payload_json, "file_size");

    if (!cJSON_IsString(session_item) || session_item->valuestring[0] == '\0' ||
        !cJSON_IsNumber(chunk_size_item) ||
        !cJSON_IsNumber(file_size_item)) {
        cJSON_Delete(payload_json);
        return -1;
    }

    uint32_t chunk_size = (uint32_t)chunk_size_item->valuedouble;
    double file_size_dbl = file_size_item->valuedouble;

    if (chunk_size == 0 || file_size_dbl < 0) {
        cJSON_Delete(payload_json);
        return -1;
    }

    uint64_t file_size = (uint64_t)file_size_dbl;
    char session_id[64] = {0};
    strncpy(session_id, session_item->valuestring, sizeof(session_id) - 1);

    cJSON_Delete(payload_json);
    payload_json = NULL;

    uint32_t total_chunks = 0;
    if (file_size > 0) {
        total_chunks = (uint32_t)((file_size + chunk_size - 1) / chunk_size);
    }

    FILE *fp = fopen(storage_path, "wb");
    if (!fp) {
        return -1;
    }

    cJSON *finish_json = NULL;

    for (uint32_t chunk_index = 1; chunk_index <= total_chunks; ++chunk_index) {
        cJSON *chunk_json = cJSON_CreateObject();
        if (!chunk_json) {
            rc = -1;
            goto cleanup;
        }

        cJSON_AddStringToObject(chunk_json, "cmd", "DOWNLOAD_CHUNK");
        cJSON_AddStringToObject(chunk_json, "session_id", session_id);
        cJSON_AddNumberToObject(chunk_json, "chunk_index", chunk_index);

        Frame chunk_resp = {0};
        rc = send_cmd(chunk_json, &chunk_resp);
        cJSON_Delete(chunk_json);
        chunk_json = NULL;

        if (rc != 0) {
            goto cleanup;
        }

        if (chunk_resp.msg_type != MSG_DATA || chunk_resp.payload_len == 0) {
            rc = -1;
            if (chunk_resp.msg_type == MSG_RESPOND) {
                *res = chunk_resp;
            }
            goto cleanup;
        }

        size_t written = fwrite(chunk_resp.payload, 1, chunk_resp.payload_len, fp);
        if (written != chunk_resp.payload_len) {
            rc = -1;
            goto cleanup;
        }
    }

    finish_json = cJSON_CreateObject();
    if (!finish_json) {
        rc = -1;
        goto cleanup;
    }

    cJSON_AddStringToObject(finish_json, "cmd", "DOWNLOAD_FINISH");
    cJSON_AddStringToObject(finish_json, "session_id", session_id);

    rc = send_cmd(finish_json, res);
    cJSON_Delete(finish_json);
    finish_json = NULL;

    if (rc != 0) {
        goto cleanup;
    }

    if (res->msg_type != MSG_RESPOND || res->header.resp.status != STATUS_OK) {
        rc = -1;
        goto cleanup;
    }

    rc = 0;

cleanup:
    if (fp) {
        fclose(fp);
        if (rc != 0) {
            remove(storage_path);
        }
    }
    if (finish_json) {
        cJSON_Delete(finish_json);
    }
    return rc;
}
