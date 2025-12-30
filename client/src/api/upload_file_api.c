#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client.h"

#include "utils/file_system_util.h"
#include "utils/uuid_util.h"

static int sent_upload_init_cmd(const char *file_name, int parent_folder_id,
                                uint64_t file_size, Frame *res) {
    if (!file_name || file_name[0] == '\0' || parent_folder_id <= 0 || !res) {
        return -1;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return -1;
    }

    cJSON_AddStringToObject(json, "cmd", "UPLOAD_INIT");
    cJSON_AddNumberToObject(json, "parent_folder_id", parent_folder_id);
    cJSON_AddStringToObject(json, "file_name", file_name);
    cJSON_AddNumberToObject(json, "file_size", (double)file_size);
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

static int sent_upload_chunk_cmd(const uint8_t session_id[SESSIONID_SIZE],
                                 uint32_t chunk_index, uint32_t chunk_length,
                                 const uint8_t *data, Frame *resp) {
    if (!session_id || chunk_length == 0 || !data || !resp) {
        return -1;
    }
    Frame frame = {0};
    if (build_data_frame(&frame, 0, session_id, chunk_index, chunk_length, data) != 0) {
        return -1;
    }
    printf("SENT DATA: \n");
    int rc = connect_send_request(g_conn, &frame, resp);
    printf("RCV ACK DATA: \n");

    
    if (resp->msg_type != MSG_RESPOND || resp->header.resp.status != STATUS_OK) {
        rc = -1;
    }

    return rc;
}

static int sent_upload_finish_cmd(const char *session_id_str, Frame *res) {
    if (!session_id_str || session_id_str[0] == '\0' || !res) {
        return -1;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return -1;
    }

    cJSON_AddStringToObject(json, "cmd", "UPLOAD_FINISH");
    cJSON_AddStringToObject(json, "session_id", session_id_str);
    int rc = send_cmd(json, res);

    if (res->msg_type != MSG_RESPOND || res->header.resp.status != STATUS_OK) {
        rc = -1;
    }

    cJSON_Delete(json);

    return rc;
}

int upload_file_api(const char* file_path, int parent_folder_id, Frame* res) {
    if (!file_path || file_path[0] == '\0' || parent_folder_id <= 0 || !res) {
        return -1;
    }
    int rc = -1;
    char file_name[256] = {0};
    int file_size = -1;
    Frame init_resp = {0};
    char *payload_copy = NULL;
    cJSON *payload_json = NULL;
    
    // Tìm file
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        printf("[UPLOAD] Không tìm thấy file");
        return -1;
    }

    // Lấy size của file
    file_size= get_file_size(file_path);
    if (file_size == -1) goto cleanup;

    // Lấy file name
    extract_file_name(file_path, file_name, sizeof(file_name));


    rc = sent_upload_init_cmd(file_name, parent_folder_id, file_size, &init_resp);
    
    if (rc != 0) {
        goto cleanup;
    }

    payload_copy = (char *)malloc(init_resp.payload_len + 1);
    if (!payload_copy) {
        goto cleanup;
    }

    memcpy(payload_copy, init_resp.payload, init_resp.payload_len);
    payload_copy[init_resp.payload_len] = '\0';

    payload_json = cJSON_Parse(payload_copy);
    free(payload_copy);
    payload_copy = NULL;

    if (!payload_json) {
        goto cleanup;
    }

    cJSON *session_item = cJSON_GetObjectItemCaseSensitive(payload_json, "sessionId");
    if (!cJSON_IsString(session_item) || session_item->valuestring[0] == '\0') {
        cJSON_Delete(payload_json);
        payload_json = NULL;
        goto cleanup;
    }

    char session_id_str[37] = {0};
    strncpy(session_id_str, session_item->valuestring, sizeof(session_id_str) - 1);
    cJSON_Delete(payload_json);
    payload_json = NULL;

    uint8_t session_id[SESSIONID_SIZE] = {0};
    if (uuid_string_to_bytes(session_id_str, session_id) != 0) {
        goto cleanup;
    }

    uint8_t buffer[MAX_PAYLOAD];
    uint32_t chunk_index = 1;
    size_t chunk_len = 0;

    printf("***A\n");
    while ((chunk_len = fread(buffer, 1, MAX_PAYLOAD, fp)) > 0) {
        Frame chunk_resp = {0};
        rc = sent_upload_chunk_cmd(session_id, chunk_index, (uint32_t)chunk_len, buffer, &chunk_resp);
        if (rc != 0) {
            goto cleanup;
        }
        ++chunk_index;
    }

    if (ferror(fp)) {
        rc = -1;
        goto cleanup;
    }

    rc = sent_upload_finish_cmd(session_id_str, res);
    if (rc != 0) {
        goto cleanup;
    }

    rc = 0;

cleanup:
    if (fp) {
        fclose(fp);
    }
    if (payload_copy) {
        free(payload_copy);
    }
    if (payload_json) {
        cJSON_Delete(payload_json);
    }
    return rc;
}
