#include "cJSON.h"
#include "utils/local_storage_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_entire_file(const char *path, char **out_buffer, size_t *out_len) {
    if (!path || !out_buffer || !out_len)
        return -1;

    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    long length = ftell(fp);
    if (length < 0) {
        fclose(fp);
        return -1;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    char *buffer = (char *)malloc((size_t)length + 1);
    if (!buffer) {
        fclose(fp);
        return -1;
    }

    size_t read_len = fread(buffer, 1, (size_t)length, fp);
    fclose(fp);

    if (read_len != (size_t)length) {
        free(buffer);
        return -1;
    }

    buffer[length] = '\0';
    *out_buffer = buffer;
    *out_len = (size_t)length;
    return 0;
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

int local_storage_load_file(const char *path, LocalStorageData *out) {
    if (!path || !out)
        return -1;

    memset(out, 0, sizeof(*out));
    char *content = NULL;
    size_t content_len = 0;
    if (read_entire_file(path, &content, &content_len) != 0)
        return -1;

    cJSON *json = cJSON_Parse(content);
    free(content);

    if (!json)
        return -1;

    int status = 0;
    if (copy_json_string(cJSON_GetObjectItemCaseSensitive(json, "token"), out->token,
                         LOCAL_STORAGE_TOKEN_MAX) != 0)
        status = -1;

    if (copy_json_string(cJSON_GetObjectItemCaseSensitive(json, "root_folder_id"),
                         out->root_folder_id, LOCAL_STORAGE_FOLDER_ID_MAX) != 0)
        status = -1;

    cJSON *user = cJSON_GetObjectItemCaseSensitive(json, "user");
    if (!cJSON_IsObject(user)) {
        status = -1;
    } else {
        if (copy_json_string(cJSON_GetObjectItemCaseSensitive(user, "id"), out->user.id,
                             LOCAL_STORAGE_USER_ID_MAX) != 0)
            status = -1;

        if (copy_json_string(cJSON_GetObjectItemCaseSensitive(user, "username"), out->user.username,
                             LOCAL_STORAGE_USERNAME_MAX) != 0)
            status = -1;

        if (copy_json_string(cJSON_GetObjectItemCaseSensitive(user, "email"), out->user.email,
                             LOCAL_STORAGE_EMAIL_MAX) != 0)
            status = -1;
    }

    cJSON_Delete(json);
    return status;
}

int local_storage_load_default(LocalStorageData *out) {
    return local_storage_load_file(LOCAL_STORAGE_PATH, out);
}

int local_storage_save_file(const char *path, const LocalStorageData *data) {
    if (!path || !data)
        return -1;

    cJSON *json = cJSON_CreateObject();
    if (!json)
        return -1;

    if (!cJSON_AddStringToObject(json, "token", data->token)) {
        cJSON_Delete(json);
        return -1;
    }

    if (!cJSON_AddStringToObject(json, "root_folder_id", data->root_folder_id)) {
        cJSON_Delete(json);
        return -1;
    }

    cJSON *user_obj = cJSON_CreateObject();
    if (!user_obj) {
        cJSON_Delete(json);
        return -1;
    }

    if (!cJSON_AddStringToObject(user_obj, "id", data->user.id) ||
        !cJSON_AddStringToObject(user_obj, "username", data->user.username) ||
        !cJSON_AddStringToObject(user_obj, "email", data->user.email)) {
        cJSON_Delete(user_obj);
        cJSON_Delete(json);
        return -1;
    }

    cJSON_AddItemToObject(json, "user", user_obj);

    char *payload = cJSON_Print(json);
    if (!payload) {
        cJSON_Delete(json);
        return -1;
    }

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(payload);
        cJSON_Delete(json);
        return -1;
    }

    size_t payload_len = strlen(payload);
    if (fwrite(payload, 1, payload_len, fp) != payload_len) {
        fclose(fp);
        free(payload);
        cJSON_Delete(json);
        return -1;
    }

    fclose(fp);
    free(payload);
    cJSON_Delete(json);
    return 0;
}

int local_storage_save_default(const LocalStorageData *data) {
    return local_storage_save_file(LOCAL_STORAGE_PATH, data);
}
