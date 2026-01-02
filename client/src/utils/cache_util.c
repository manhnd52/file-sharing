#include "cJSON.h"
#include "utils/cache_util.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>

static int read_entire_file(const char *path, char **out_buffer, size_t *out_len)
{
    *out_buffer = NULL;
    *out_len = 0;

    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;

    int fd = fileno(fp);
    if (flock(fd, LOCK_SH) != 0) goto error;

    if (fseek(fp, 0, SEEK_END) != 0) goto error;
    long length = ftell(fp);
    if (length < 0) goto error;
    if (fseek(fp, 0, SEEK_SET) != 0) goto error;

    char *buffer = malloc((size_t)length + 1);
    if (!buffer) goto error;

    size_t n = fread(buffer, 1, (size_t)length, fp);
    if (n != (size_t)length) {
        free(buffer);
        goto error;
    }

    buffer[length] = '\0';
    *out_buffer = buffer;
    *out_len = (size_t)length;

    flock(fd, LOCK_UN);
    fclose(fp);
    return 0;

error:
    flock(fd, LOCK_UN);
    fclose(fp);
    return -1;
}


static int copy_json_string(cJSON *item, char *dest, size_t dest_size) {
    if (!item || !dest || dest_size == 0)
        return -1;

    if (!cJSON_IsString(item) || !item->valuestring)
        return -1;

    strncpy(dest, item->valuestring, dest_size - 1);
    dest[dest_size - 1] = '\0';
    return 0;
}

static uint64_t read_json_uint64(cJSON *item) {
    if (!cJSON_IsNumber(item) || item->valuedouble < 0)
        return 0;
    return (uint64_t)item->valuedouble;
}

static uint32_t read_json_uint32(cJSON *item) {
    if (!cJSON_IsNumber(item) || item->valuedouble < 0)
        return 0;

    double value = item->valuedouble;
    if (value > UINT32_MAX)
        value = UINT32_MAX;
    return (uint32_t)value;
}

static void fill_downloading_state(CacheDownloadingState *out, cJSON *downloading_json) {
    if (!out || !downloading_json)
        return;

    copy_json_string(cJSON_GetObjectItemCaseSensitive(downloading_json, "session_id"),
                     out->session_id, CACHE_SESSION_ID_MAX);
    copy_json_string(cJSON_GetObjectItemCaseSensitive(downloading_json, "file_name"),
                     out->file_name, CACHE_FILE_NAME_MAX);
    copy_json_string(cJSON_GetObjectItemCaseSensitive(downloading_json, "storage_path"),
                     out->storage_path, CACHE_PATH_MAX);
    copy_json_string(cJSON_GetObjectItemCaseSensitive(downloading_json, "created_at"),
                     out->created_at, CACHE_CREATED_AT_MAX);

    out->total_size =
        read_json_uint64(cJSON_GetObjectItemCaseSensitive(downloading_json, "total_size"));
    out->chunk_size =
        read_json_uint32(cJSON_GetObjectItemCaseSensitive(downloading_json, "chunk_size"));
    out->last_received_chunk =
        read_json_uint32(cJSON_GetObjectItemCaseSensitive(downloading_json, "last_received_chunk"));
}

static void fill_uploading_state(CacheUploadingState *out, cJSON *uploading_json) {
    if (!out || !uploading_json)
        return;

    copy_json_string(cJSON_GetObjectItemCaseSensitive(uploading_json, "session_id"),
                     out->session_id, CACHE_SESSION_ID_MAX);
    out->parent_folder_id =
        read_json_uint32(cJSON_GetObjectItemCaseSensitive(uploading_json, "parent_folder_id"));
    copy_json_string(cJSON_GetObjectItemCaseSensitive(uploading_json, "file_path"),
                     out->file_path, CACHE_PATH_MAX);
    copy_json_string(cJSON_GetObjectItemCaseSensitive(uploading_json, "created_at"),
                     out->created_at, CACHE_CREATED_AT_MAX);

    out->total_size =
        read_json_uint64(cJSON_GetObjectItemCaseSensitive(uploading_json, "total_size"));
    out->chunk_size =
        read_json_uint32(cJSON_GetObjectItemCaseSensitive(uploading_json, "chunk_size"));
    out->last_sent_chunk =
        read_json_uint32(cJSON_GetObjectItemCaseSensitive(uploading_json, "last_sent_chunk"));
}

int cache_load_file(const char *path, CacheState *out) {
    if (!path || !out)
        return -1;

    printf("Debug: cache_load_file\n");
    memset(out, 0, sizeof(*out));

    char *content = NULL;
    size_t content_len = 0;
    if (read_entire_file(path, &content, &content_len) != 0) {
        printf("Không đọc được file cache\n");
        return -1;
    }
        

    cJSON *json = cJSON_Parse(content);
    free(content);

    if (!json) {
        printf("Không giaiả mã được Json\n");
        return -1;
    }

    int status = 0;
    cJSON *downloading_json = cJSON_GetObjectItemCaseSensitive(json, "downloading");
    if (cJSON_IsObject(downloading_json)) {
        fill_downloading_state(&out->downloading, downloading_json);
    } else {
        printf("Không tao duoc download json\n");
        status = -1;
    }

    cJSON *uploading_json = cJSON_GetObjectItemCaseSensitive(json, "uploading");
    if (cJSON_IsObject(uploading_json)) {
        fill_uploading_state(&out->uploading, uploading_json);
    } else {
        status = -1;
        printf("Không tao duoc upload json\n");
    }

    char* payload = cJSON_Print(json);
    printf("\n READ JSON: %s\n", payload);

    cJSON_Delete(json);
    return status;
}

int cache_load_default(CacheState *out) {
    return cache_load_file(CACHE_STATE_PATH, out);
}

static cJSON *build_downloading_object(const CacheDownloadingState *state) {
    cJSON *downloading = cJSON_CreateObject();
    if (!downloading)
        return NULL;

    cJSON_AddStringToObject(downloading, "session_id",
                            state->session_id[0] ? state->session_id : "");
    cJSON_AddStringToObject(downloading, "file_name",
                            state->file_name[0] ? state->file_name : "");
    cJSON_AddStringToObject(downloading, "storage_path",
                            state->storage_path[0] ? state->storage_path : "");
    cJSON_AddStringToObject(downloading, "created_at",
                            state->created_at[0] ? state->created_at : "");
    cJSON_AddNumberToObject(downloading, "total_size", (double)state->total_size);
    cJSON_AddNumberToObject(downloading, "chunk_size", (double)state->chunk_size);
    cJSON_AddNumberToObject(downloading, "last_received_chunk",
                            (double)state->last_received_chunk);
    return downloading;
}

static cJSON *build_uploading_object(const CacheUploadingState *state) {
    cJSON *uploading = cJSON_CreateObject();
    if (!uploading)
        return NULL;

    cJSON_AddStringToObject(uploading, "session_id",
                            state->session_id[0] ? state->session_id : "");
    cJSON_AddNumberToObject(uploading, "parent_folder_id",
                            (double)state->parent_folder_id);
    cJSON_AddStringToObject(uploading, "file_path",
                            state->file_path[0] ? state->file_path : "");
    cJSON_AddStringToObject(uploading, "created_at",
                            state->created_at[0] ? state->created_at : "");
    cJSON_AddNumberToObject(uploading, "total_size", (double)state->total_size);
    cJSON_AddNumberToObject(uploading, "chunk_size", (double)state->chunk_size);
    cJSON_AddNumberToObject(uploading, "last_sent_chunk", (double)state->last_sent_chunk);
    return uploading;
}

int cache_save_file(const char *path, const CacheState *data) {
    if (!path || !data)
        return -1;

    cJSON *json = cJSON_CreateObject();
    if (!json)
        return -1;

    cJSON *downloading = build_downloading_object(&data->downloading);
    cJSON *uploading = build_uploading_object(&data->uploading);

    if (!downloading || !uploading) {
        cJSON_Delete(downloading);
        cJSON_Delete(uploading);
        cJSON_Delete(json);
        return -1;
    }

    cJSON_AddItemToObject(json, "downloading", downloading);
    cJSON_AddItemToObject(json, "uploading", uploading);

    char *payload = cJSON_Print(json);
    printf("Payload: %s\n", payload);
    if (!payload) {
        cJSON_Delete(json);
        return -1;
    }

    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        free(payload);
        cJSON_Delete(json);
        return -1;
    }

    if (flock(fd, LOCK_EX) != 0) {
        close(fd);
        free(payload);
        cJSON_Delete(json);
        return -1;
    }

    if (ftruncate(fd, 0) != 0) {
        flock(fd, LOCK_UN);
        close(fd);
        free(payload);
        cJSON_Delete(json);
        return -1;
    }

    FILE *fp = fdopen(fd, "wb");
    if (!fp) {
        flock(fd, LOCK_UN);
        close(fd);
        free(payload);
        cJSON_Delete(json);
        return -1;
    }

    size_t payload_len = strlen(payload);
    size_t written = fwrite(payload, 1, payload_len, fp);
    fclose(fp);
    free(payload);
    cJSON_Delete(json);

    return written == payload_len ? 0 : -1;
}

int cache_save_default(const CacheState *data) {
    return cache_save_file(CACHE_STATE_PATH, data);
}

static int cache_load_or_zero(CacheState *state) {
    if (!state)
        return -1;

    int rc = cache_load_default(state);
    printf("Debug: Cache load or 0: %d\n", rc);
    if (rc != 0) {
        memset(state, 0, sizeof(*state));
    }
    return rc;
}

int cache_update_downloading(const CacheDownloadingState *state) {
    if (!state)
        return -1;

    CacheState current;
    cache_load_or_zero(&current);
    memcpy(&current.downloading, state, sizeof(current.downloading));
    return cache_save_default(&current);
}

int cache_update_uploading(const CacheUploadingState *state) {
    if (!state)
        return -1;

    CacheState current;
    cache_load_or_zero(&current);
    memcpy(&current.uploading, state, sizeof(current.uploading));
    return cache_save_default(&current);
}

int cache_update_uploading_last_sent_chunk(int chunk_index) {
    if (chunk_index <= 0)
        return -1;

    CacheState current;
    cache_load_or_zero(&current);
    current.uploading.last_sent_chunk = (uint32_t)chunk_index;
    return cache_save_default(&current);
}

int cache_update_downloading_last_received_chunk(int chunk_index) {
    if (chunk_index <= 0)
        return -1;

    CacheState current;
    cache_load_or_zero(&current);
    current.downloading.last_received_chunk = (uint32_t)chunk_index;
    return cache_save_default(&current);
}

int cache_reset_downloading(void) {
    CacheState current;
    cache_load_or_zero(&current);
    memset(&current.downloading, 0, sizeof(current.downloading));
    return cache_save_default(&current);
}

int cache_reset_uploading(void) {
    CacheState current;
    cache_load_or_zero(&current);
    memset(&current.uploading, 0, sizeof(current.uploading));
    return cache_save_default(&current);
}

static void format_current_timestamp(char *out, size_t size) {
    if (!out || size == 0)
        return;

    time_t now = time(NULL);
    struct tm tm_info;
    if (localtime_r(&now, &tm_info) != NULL) {
        strftime(out, size, "%Y-%m-%dT%H:%M:%SZ", &tm_info);
    } else {
        out[0] = '\0';
    }
}

void cache_init_downloading_state(const char *session_id,
                                  const char *file_name,
                                  const char *storage_path,
                                  uint64_t total_size,
                                  uint32_t chunk_size) {
    CacheDownloadingState state;
    memset(&state, 0, sizeof(state));

    if (session_id) {
        strncpy(state.session_id, session_id, CACHE_SESSION_ID_MAX - 1);
    }

    if (file_name) {
        strncpy(state.file_name, file_name, CACHE_FILE_NAME_MAX - 1);
    }

    if (storage_path) {
        strncpy(state.storage_path, storage_path, CACHE_PATH_MAX - 1);
    }

    format_current_timestamp(state.created_at, CACHE_CREATED_AT_MAX);
    state.total_size = total_size;
    state.chunk_size = chunk_size;
    state.last_received_chunk = 0;

    CacheState current;
    cache_load_or_zero(&current);
    memcpy(&current.downloading, &state, sizeof(current.downloading));
    memset(&current.uploading, 0, sizeof(current.uploading));
    cache_save_default(&current);
}

void cache_init_uploading_state(const char *session_id,
                                const int parent_folder_id,
                                const char *file_path,
                                uint64_t total_size,
                                uint32_t chunk_size) {
    CacheUploadingState state;
    memset(&state, 0, sizeof(state));

    if (session_id) {
        strncpy(state.session_id, session_id, CACHE_SESSION_ID_MAX - 1);
    }

    if (parent_folder_id > 0) {
        state.parent_folder_id = (uint32_t)parent_folder_id;
    }

    if (file_path) {
        strncpy(state.file_path, file_path, CACHE_PATH_MAX - 1);
    }

    format_current_timestamp(state.created_at, CACHE_CREATED_AT_MAX);
    state.total_size = total_size;
    state.chunk_size = chunk_size;
    state.last_sent_chunk = 0;

    CacheState current;
    cache_load_or_zero(&current);
    memcpy(&current.uploading, &state, sizeof(current.uploading));
    memset(&current.downloading, 0, sizeof(current.downloading));
    cache_save_default(&current);
}
