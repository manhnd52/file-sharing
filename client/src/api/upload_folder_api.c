#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "api/upload_api.h"
#include "api/folder_api.h"
#include "cJSON.h"
#include "utils/file_system_util.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int extract_folder_id_from_respond(const Frame *frame) {
    char *payload_copy = (char *)malloc(frame->payload_len + 1);

    memcpy(payload_copy, frame->payload, frame->payload_len);

    payload_copy[frame->payload_len] = '\0';

    cJSON *payload_json = cJSON_Parse(payload_copy);

    free(payload_copy);

    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(payload_json, "id");

    int folder_id = -1;

    if (cJSON_IsNumber(id_item)) {
        folder_id = id_item->valueint;
    }

    cJSON_Delete(payload_json);
    return folder_id;
}

static int upload_folder_recursive(const char *folder_path, int parent_folder_id,
                                   Frame *resp) {
    if (!folder_path || folder_path[0] == '\0' || parent_folder_id <= 0 || !resp) {
        return -1;
    }

    char folder_name[256] = {0};
    if (!extract_folder_name(folder_path, folder_name, sizeof(folder_name))) {
        return -1;
    }

    Frame mkdir_resp = {0};
    int rc = mkdir_api(parent_folder_id, folder_name, &mkdir_resp);
    if (resp) {
        *resp = mkdir_resp;
    }

    if (rc != 0) {
        return rc;
    }

    if (mkdir_resp.msg_type != MSG_RESPOND || mkdir_resp.header.resp.status != STATUS_OK ||
        mkdir_resp.payload_len == 0) {
        return -1;
    }

    int remote_folder_id = extract_folder_id_from_respond(&mkdir_resp);
    
    if (remote_folder_id <= 0) {
        return -1;
    }

    DIR *dir = opendir(folder_path);

    if (!dir) {
        return -1;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (is_dot_or_dotdot(entry->d_name)) {
            continue;
        }

        char child_path[PATH_MAX];
        if (!join_path(child_path, sizeof(child_path), folder_path, entry->d_name)) {
            rc = -1;
            break;
        }

        struct stat st = {0};
        if (stat(child_path, &st) != 0) {
            rc = -1;
            break;
        }

        if (S_ISDIR(st.st_mode)) {
            rc = upload_folder_recursive(child_path, remote_folder_id, resp);
        } else if (S_ISREG(st.st_mode)) {
            rc = upload_file_api(child_path, remote_folder_id, resp);
        } else {
            continue;
        }

        if (rc != 0) {
            break;
        }
    }

    closedir(dir);
    return rc;
}

int upload_folder_api(const char *folder_path, int parent_folder_id, Frame *res) {
    if (!folder_path || folder_path[0] == '\0' || parent_folder_id <= 0) {
        return -1;
    }

    struct stat st = {0};
    if (stat(folder_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return -1;
    }

    Frame temp_resp = {0};
    Frame *resp = res ? res : &temp_resp;
    return upload_folder_recursive(folder_path, parent_folder_id, resp);
}
