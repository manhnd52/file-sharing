#include "handlers/download_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "services/file_service.h"
#include "utils/uuid.h"
#include "cJSON.h"

DownloadSession dss[MAX_SESSION];

static void respond_download_finish_error(Conn *c, Frame *f, const char *payload) {
    if (!c || !f || !payload) return;
    Frame err_frame;
    build_respond_frame(&err_frame, f->header.cmd.request_id, STATUS_NOT_OK, payload);
    send_frame(c->sockfd, &err_frame);
}

static int find_session(const uint8_t sid[SESSIONID_SIZE], DownloadSession **out) {
    // find existing
    for (int i = 0; i < MAX_SESSION; ++i) {
        if (memcmp(dss[i].session_id, sid, SESSIONID_SIZE) == 0 && dss[i].file_hashcode[0] != '\0') {
            *out = &dss[i];
            return 0;
        }
    }
    return -1;
}

/*
Handler for received DOWNLOAD_CHUNK command frames
`DOWNLOAD_CHUNK` frame payload mẫu:
```
{
    "session_id": "<UUID string>",
    "chunk_index": <number>
}
```
Respond với DATA frame chứa chunk dữ liệu tương ứng của file.
*/
void download_chunk_handler(Conn *c, Frame *req) {
    if (!c || !req) return;

    cJSON *root = cJSON_Parse((const char *)req->payload);
    if (!root) {
        respond_download_finish_error(c, req, "{\"error\": \"invalid_json\"}");
        return;
    }

    cJSON *session_id_json = cJSON_GetObjectItemCaseSensitive(root, "session_id");
    cJSON *chunk_index_json = cJSON_GetObjectItemCaseSensitive(root, "chunk_index");

    if (!session_id_json || !cJSON_IsString(session_id_json) ||
        !chunk_index_json || !cJSON_IsNumber(chunk_index_json)) {
        respond_download_finish_error(c, req, "{\"error\": \"missing_parameters\"}");
        cJSON_Delete(root);
        return;
    }

    const char *session_id_str = session_id_json->valuestring;
    uint8_t session_id[BYTE_UUID_SIZE];

    if (uuid_string_to_bytes(session_id_str, session_id) != 0) {
        respond_download_finish_error(c, req, "{\"error\": \"invalid_session_id\"}");
        cJSON_Delete(root);
        return;
    }

    DownloadSession *ds = NULL;
    if (find_session(session_id, &ds) != 0 || !ds) {
        respond_download_finish_error(c, req, "{\"error\": \"session_not_found\"}");
        cJSON_Delete(root);
        return;
    }

    uint32_t chunk_index = (uint32_t)chunk_index_json->valueint;
    if (chunk_index != ds->last_requested_chunk + 1) {
        respond_download_finish_error(c, req, "{\"error\": \"invalid_chunk_index\"}");
        cJSON_Delete(root);
        return;
    }
    // Đọc chunk dữ liệu từ file storage
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "data/storage/%s", ds->file_hashcode);
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        respond_download_finish_error(c, req, "{\"error\": \"failed_to_open_file\"}");
        cJSON_Delete(root);
        return;
    }
    uint64_t offset = (uint64_t)(chunk_index - 1) * (uint64_t)ds->chunk_size;
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        close(fd);
        respond_download_finish_error(c, req, "{\"error\": \"failed_to_seek_file\"}");
        cJSON_Delete(root);
        return;
    }

    uint8_t *buffer = (uint8_t *)malloc(ds->chunk_size);
    if (!buffer) {
        close(fd);
        respond_download_finish_error(c, req, "{\"error\": \"memory_allocation_failed\"}");
        cJSON_Delete(root);
        return;
    }
    ssize_t r = read(fd, buffer, ds->chunk_size);
    if (r < 0) {
        free(buffer);
        close(fd);
        respond_download_finish_error(c, req, "{\"error\": \"failed_to_read_file\"}");
        cJSON_Delete(root);
        return;
    }
    free(buffer);
    close(fd);
    // Gửi DATA frame chứa chunk dữ liệu
    Frame data_frame;
    build_data_frame(&data_frame, req->header.cmd.request_id, session_id,
                        chunk_index, (uint32_t)r, buffer);
    send_frame(c->sockfd, &data_frame);
    ds->last_requested_chunk = chunk_index;
    cJSON_Delete(root);
}

void download_init_handler(Conn *c, Frame *f) {
    if (!c || !f) return;

    cJSON *root = cJSON_Parse((const char *)f->payload);
    if (!root) {
        respond_download_finish_error(c, f, "{\"error\": \"invalid_json\"}");
        return;
    }

    cJSON *file_id_json = cJSON_GetObjectItemCaseSensitive(root, "file_id");
    cJSON *chunk_size_json = cJSON_GetObjectItemCaseSensitive(root, "chunk_size");

    if (!file_id_json || !cJSON_IsNumber(file_id_json) ||
        !chunk_size_json || !cJSON_IsNumber(chunk_size_json)) {
        respond_download_finish_error(c, f, "{\"error\": \"missing_parameters\"}");
        cJSON_Delete(root);
        return;
    }

    int file_id = file_id_json->valueint;
    uint32_t chunk_size = (uint32_t)chunk_size_json->valueint;

    // Lấy thông tin file từ file service
    cJSON *fi = get_file_info(file_id);
    if (!fi) {
        respond_download_finish_error(c, f, "{\"error\": \"file_not_found\"}");
        cJSON_Delete(root);
        return;
    } 
    
    uint64_t file_size = (uint64_t)cJSON_GetObjectItemCaseSensitive(fi, "file_size")->valuedouble;
    char storage_hash[37];
    strncpy(storage_hash, cJSON_GetObjectItemCaseSensitive(fi, "storage_hash")->valuestring, sizeof(storage_hash) - 1);
    storage_hash[sizeof(storage_hash) - 1] = '\0';
    char file_name[256];
    strncpy(file_name, cJSON_GetObjectItemCaseSensitive(fi, "file_name")->valuestring, sizeof(file_name) - 1);
    file_name[sizeof(file_name) - 1] = '\0';
    cJSON_Delete(fi);

    // Tạo session download mới
    uint8_t sid[BYTE_UUID_SIZE];
    generate_byte_uuid(sid);

    DownloadSession *ds = NULL;

    for (int i = 0; i < MAX_SESSION; ++i) {
        if (dss[i].file_hashcode[0] == '\0') {
            memcpy(dss[i].session_id, sid, BYTE_UUID_SIZE);
            dss[i].last_requested_chunk = 0;
            dss[i].chunk_size = chunk_size;
            dss[i].total_file_size = file_size;
            dss[i].file_id = file_id;
            strncpy(dss[i].file_hashcode, storage_hash, sizeof(dss[i].file_hashcode) - 1);
            ds = &dss[i];
            break;
        }
    }
    if (!ds) {
        Frame err_frame;
        build_respond_frame(&err_frame, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\": \"No available session slots\"}");
        send_frame(c->sockfd, &err_frame);
        return;
    }

    // Respond with RES including sessionId
    char uuid_str[37];
    bytes_to_uuid_string(sid, uuid_str);
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"sessionId\": \"%s\", \"file_name\": \"%s\", \"file_size\": %llu, \"chunk_size\": %u}", uuid_str, file_name, (unsigned long long)file_size, chunk_size);
    Frame ok;
    build_respond_frame(&ok, f->header.cmd.request_id, STATUS_OK, payload);
    send_frame(c->sockfd, &ok);
    cJSON_Delete(root);
}

void download_finish_handler(Conn *c, Frame *f) {
    if (!c || !f) return;

    cJSON *root = cJSON_Parse((const char *)f->payload);
    cJSON *session_id_json = cJSON_GetObjectItemCaseSensitive(root, "session_id");
    if (!session_id_json || !cJSON_IsString(session_id_json) || session_id_json->valuestring[0] == '\0') {
        respond_download_finish_error(c, f, "{\"error\": \"missing_session_id\"}");
        cJSON_Delete(root);
        return;
    }

    const char *session_id_str = session_id_json->valuestring;
    uint8_t session_id[BYTE_UUID_SIZE];

    if (uuid_string_to_bytes(session_id_str, session_id) != 0) {
        respond_download_finish_error(c, f, "{\"error\": \"invalid_session_id\"}");
        cJSON_Delete(root);
        return;
    }

    DownloadSession *ds = NULL;
    if (find_session(session_id, &ds) != 0 || !ds) {
        respond_download_finish_error(c, f, "{\"error\": \"session_not_found\"}");
        cJSON_Delete(root);
        return;
    }

    // Clear session
    memset(ds, 0, sizeof(DownloadSession));

    // Respond with OK
    Frame ok;
    build_respond_frame(&ok, f->header.cmd.request_id, STATUS_OK, "{\"message\": \"download_session_finished\"}");
    send_frame(c->sockfd, &ok);
    cJSON_Delete(root);
}