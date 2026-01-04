#include "cJSON.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"

#include "api/download_api.h"
#include "utils/file_system_util.h"
#include "utils/cache_util.h"

static int sent_download_init_cmd(int folder_id, Frame* res) {
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

    return 0;
}

static int sent_download_chunk_cmd(const char* session_id, uint32_t chunk_index,
                                   Frame* resp) {
    if (!session_id || session_id[0] == '\0' || !resp) {
        return -1;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return -1;
    }

    cJSON_AddStringToObject(json, "cmd", "DOWNLOAD_CHUNK");
    cJSON_AddStringToObject(json, "session_id", session_id);
    cJSON_AddNumberToObject(json, "chunk_index", chunk_index);

    int rc = send_cmd(json, resp);
    printf("DEBUG: after send_cmd rc = %d\n", rc);
    cJSON_Delete(json);
    return rc;
}

static int sent_download_finish_cmd(const char* session_id, Frame* res) {
    if (!session_id || session_id[0] == '\0' || !res) {
        return -1;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return -1;
    }

    cJSON_AddStringToObject(json, "cmd", "DOWNLOAD_FINISH");
    cJSON_AddStringToObject(json, "session_id", session_id);

    int rc = send_cmd(json, res);
    cJSON_Delete(json);
    return rc;
}

int download_file_api(const char* storage_path, int file_id, Frame* res) {
    if (!storage_path || storage_path[0] == '\0' || file_id <= 0 || !res) {
        return -1;
    }

    int rc = sent_download_init_cmd(file_id, res);
    if (rc != 0) {
        return rc;
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
    cJSON *file_name_item = cJSON_GetObjectItemCaseSensitive(payload_json, "file_name");

    if (!cJSON_IsString(session_item) || session_item->valuestring[0] == '\0' ||
        !cJSON_IsNumber(chunk_size_item) ||
        !cJSON_IsNumber(file_size_item) ||
        !cJSON_IsString(file_name_item) || file_name_item->valuestring[0] == '\0') {
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
    char file_name[256] = {0};
    strncpy(file_name, file_name_item->valuestring, sizeof(file_name) - 1);
    file_name[sizeof(file_name) - 1] = '\0';

    cJSON_Delete(payload_json);
    payload_json = NULL;

    uint32_t total_chunks = 0;
    if (file_size > 0) {
        total_chunks = (uint32_t)((file_size + chunk_size - 1) / chunk_size);
    }

    char target_path[1024] = {0};
    size_t folder_len = strlen(storage_path);
    if (storage_path[folder_len - 1] == '/' || storage_path[folder_len - 1] == '\\') {
        snprintf(target_path, sizeof(target_path), "%s%s", storage_path, file_name);
    } else {
        snprintf(target_path, sizeof(target_path), "%s/%s", storage_path, file_name);
    }

    char *tmp = create_unique_filepath(target_path);
    if (tmp) {
        strncpy(target_path, tmp, sizeof(target_path)-1);
        target_path[sizeof(target_path)-1] = '\0';
        free(tmp);
    }

    FILE* fp = fopen(target_path, "wb");

    if (!fp) {
        return -1;
    }

    cache_init_downloading_state(session_id, file_name, target_path, file_size, chunk_size);

    for (uint32_t chunk_index = 1; chunk_index <= total_chunks; ++chunk_index) {
        Frame chunk_resp = {0};
        if (cache_get_downloading_transfer_state() == CACHE_TRANSFER_CANCELLED) {
            rc = REQ_OK;
            res->msg_type = MSG_RESPOND;
            snprintf((char*)res->payload, MAX_PAYLOAD, "{\"message\":\"Cancelled\"}");
            res->payload_len = strlen((char*)res->payload);
            goto cleanup;
        }

        rc = sent_download_chunk_cmd(session_id, chunk_index, &chunk_resp);
        if (rc != 0) {
            if (rc == REQ_NO_RESP) {
                printf("DEBUG: set disconnected\n");
                cache_set_downloading_transfer_state(CACHE_TRANSFER_DISCONNECTED);
            }
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
            printf("WRITE ERRR\n");
            rc = -1;
            goto cleanup;
        }
        cache_update_downloading_last_received_chunk(chunk_index);
    }

    rc = sent_download_finish_cmd(session_id, res);

    if (rc != 0) {
        goto cleanup;
    }

    if (res->msg_type != MSG_RESPOND || res->header.resp.status != STATUS_OK) {
        rc = -1;
        goto cleanup;
    }

    cache_reset_downloading();
    rc = 0;

cleanup:
    if (fp) {
        fclose(fp);
        if (rc != 0) {
            remove(target_path);
        }
    }
    return rc;
}

int download_resume_api(Frame* res) {
    if (!res) {
        return REQ_ERROR;
    }

    CacheState cache;
    int rc = cache_load_default(&cache);
    if (rc != 0) {
        return rc;
    }
    CacheDownloadingState* downloading = &cache.downloading;
    if (downloading->session_id[0] == '\0' ||
        downloading->file_name[0] == '\0' ||
        downloading->storage_path[0] == '\0' ||
        downloading->total_size == 0 ||
        downloading->chunk_size == 0) {
        return REQ_ERROR;
    }

    if (downloading->state != CACHE_TRANSFER_DISCONNECTED) {
        return REQ_ERROR;
    }
    
    uint32_t total_chunks = 0;
    uint32_t last_received_chunk = downloading->last_received_chunk;
    uint32_t total_size = downloading->total_size;
    uint32_t chunk_size = downloading->chunk_size;
    
    total_chunks = (uint32_t)((total_size + chunk_size - 1) / chunk_size);
    if (last_received_chunk >= total_chunks) {
        return REQ_ERROR;
    }

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "cmd", "DOWNLOAD_RESUME");
    cJSON_AddStringToObject(json, "session_id", downloading->session_id);
    cJSON_AddNumberToObject(json, "last_received_chunk", last_received_chunk);

    rc = send_cmd(json, res);

    if (rc != 0) {
        goto cleanup;
    }

    FILE* fp = fopen(downloading->storage_path, "ab");
    if (!fp) {
        rc = REQ_ERROR;
        goto cleanup;
    }

    for (uint32_t chunk_index = last_received_chunk + 1; chunk_index <= total_chunks; ++chunk_index) {
        rc = sent_download_chunk_cmd(downloading->session_id, chunk_index, res);
        if (rc != 0) {
            fclose(fp);
            goto cleanup;
        }

        size_t written = fwrite(res->payload, 1, res->payload_len, fp);

        if (written != res->payload_len) {
            rc = REQ_ERROR;
            fclose(fp);
            goto cleanup;
        }

        cache_update_downloading_last_received_chunk(chunk_index);
    }

    rc = sent_download_finish_cmd(downloading->session_id, res);
    fclose(fp);
    if (rc != 0) {
        goto cleanup;
    }
    rc = 0;

cleanup:
    cJSON_Delete(json);
    return rc;
}