#include "handlers/file_handler.h"
#include "cJSON.h"
#include "database.h"
#include "services/file_service.h"
#include "services/permission_service.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void handle_cmd_delete_file(Conn *c, Frame *f) {
    printf("[CMD:DELETE_FILE][INFO] user_id=%d\n", c->user_id);

    if (!db_global || c->user_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authenticated\"}");
        send_data(c, resp);
        return;
    }

    if (f->payload_len == 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_payload\"}");
        send_data(c, resp);
        return;
    }

    cJSON *root = cJSON_Parse((char *)f->payload);
    if (!root) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"invalid_json\"}");
        send_data(c, resp);
        return;
    }

    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(root, "file_id");
    if (!cJSON_IsNumber(id_item)) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_file_id\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }
    int file_id = id_item->valueint;
    cJSON_Delete(root);

    PermissionLevel perm = get_file_permission(c->user_id, file_id);
    
    if (perm < PERM_WRITE) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"forbidden\"}");
        send_data(c, resp);
        return;
    }

    int owner_id = file_get_owner(file_id);
    if (owner_id == 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"file_not_found\"}");
        send_data(c, resp);
        return;
    }

    if (owner_id == c->user_id || perm == PERM_WRITE) {
        if (!delete_file(file_id)) {
            Frame resp;
            build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                                "{\"error\":\"db_error\"}");
            send_data(c, resp);
            return;
        }
    } else {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authorized\"}");
        send_data(c, resp);
        return;
    }

    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK,
                        "{\"status\":\"ok\"}");
    send_data(c, resp);
}

void handle_cmd_copy_file(Conn *c, Frame *f) {
    if (!db_global || c->user_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authenticated\"}");
        send_data(c, resp);
        return;
    }
    cJSON *root = cJSON_Parse((char *)f->payload);
    if (!root) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"invalid_json\"}");
        send_data(c, resp);
        return;
    }
    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(root, "file_id");
    cJSON *dest_item = cJSON_GetObjectItemCaseSensitive(root, "dest_folder_id");
    if (!cJSON_IsNumber(id_item) || !cJSON_IsNumber(dest_item)) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_fields\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }
    int file_id = id_item->valueint;
    int dest_folder_id = dest_item->valueint;
    cJSON_Delete(root);

    int new_id = 0;
    int rc = copy_file(c->user_id, file_id, dest_folder_id, &new_id);
    if (rc != 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"copy_failed\"}");
        send_data(c, resp);
        return;
    }
    cJSON *resp_root = cJSON_CreateObject();
    cJSON_AddStringToObject(resp_root, "status", "ok");
    cJSON_AddNumberToObject(resp_root, "id", new_id);
    char *json_resp = cJSON_PrintUnformatted(resp_root);
    cJSON_Delete(resp_root);
    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    send_data(c, resp);
    free(json_resp);
}

static int parse_keyword(Frame *f, char *out, size_t out_size) {
    if (!f || f->payload_len == 0 || !out || out_size == 0) return 0;
    cJSON *root = cJSON_Parse((char *)f->payload);
    if (!root) return 0;
    cJSON *kw_item = cJSON_GetObjectItemCaseSensitive(root, "keyword");
    int ok = 0;
    if (cJSON_IsString(kw_item) && kw_item->valuestring && kw_item->valuestring[0] != '\0') {
        snprintf(out, out_size, "%s", kw_item->valuestring);
        ok = 1;
    }
    cJSON_Delete(root);
    return ok;
}

static void respond_items(Conn *c, Frame *f, cJSON *items) {
    if (!items) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"server_error\"}");
        send_data(c, resp);
        return;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "items", items);
    cJSON_AddStringToObject(root, "status", "ok");
    char *json_resp = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json_resp) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"server_error\"}");
        send_data(c, resp);
        return;
    }
    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    send_data(c, resp);
    free(json_resp);
}

void handle_cmd_search_files(Conn *c, Frame *f) {
    if (!db_global || c->user_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authenticated\"}");
        send_data(c, resp);
        return;
    }
    char kw[256] = {0};
    if (!parse_keyword(f, kw, sizeof(kw))) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_keyword\"}");
        send_data(c, resp);
        return;
    }
    cJSON *items = search_files(c->user_id, kw);
    respond_items(c, f, items);
}

void handle_cmd_share_file(Conn *c, Frame *f) {
    printf("[CMD:SHARE_FILE][INFO] user_id=%d\n", c->user_id);

    if (!db_global || c->user_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authenticated\"}");
        send_data(c, resp);
        return;
    }

    if (f->payload_len == 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_payload\"}");
        send_data(c, resp);
        return;
    }

    cJSON *root = cJSON_Parse((char *)f->payload);
    if (!root) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"invalid_json\"}");
        send_data(c, resp);
        return;
    }

    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(root, "file_id");
    cJSON *user_item = cJSON_GetObjectItemCaseSensitive(root, "username");
    cJSON *perm_item = cJSON_GetObjectItemCaseSensitive(root, "permission");

    if (!cJSON_IsNumber(id_item) || !cJSON_IsString(user_item) ||
        !cJSON_IsNumber(perm_item)) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_fields\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }

    int file_id = id_item->valueint;
    char username_buf[128];
    memset(username_buf, 0, sizeof(username_buf));
    snprintf(username_buf, sizeof(username_buf), "%s", user_item->valuestring ? user_item->valuestring : "");
    int permission = perm_item->valueint;
    
    int share_rc = file_share_with_user(c->user_id, file_id, username_buf, permission);
    
    cJSON_Delete(root);

    if (share_rc == -1) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"file_not_found_or_not_owner\"}");
        send_data(c, resp);
        return;
    }
    if (share_rc == -2) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"user_not_found\"}");
        send_data(c, resp);
        return;
    }
    if (share_rc != 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }

    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK,
                        "{\"status\":\"ok\"}");
    send_data(c, resp);
}

void handle_cmd_rename_file(Conn *c, Frame *f) {
    printf("[CMD:RENAME_FILE][INFO] user_id=%d\n", c->user_id);

    if (!db_global || c->user_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authenticated\"}");
        send_data(c, resp);
        return;
    }

    if (f->payload_len == 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_payload\"}");
        send_data(c, resp);
        return;
    }

    cJSON *root = cJSON_Parse((char *)f->payload);
    if (!root) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"invalid_json\"}");
        send_data(c, resp);
        return;
    }

    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(root, "file_id");
    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(root, "new_name");

    if (!cJSON_IsNumber(id_item) || !cJSON_IsString(name_item) || name_item->valuestring[0] == '\0') {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_fields\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }

    int file_id = id_item->valueint;
    char new_name_buf[256];
    memset(new_name_buf, 0, sizeof(new_name_buf));
    snprintf(new_name_buf, sizeof(new_name_buf), "%s", name_item->valuestring ? name_item->valuestring : "");
    cJSON_Delete(root);

    int rc = file_rename(c->user_id, file_id, new_name_buf);
    if (rc == -1) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"file_not_found_or_not_owner\"}");
        send_data(c, resp);
        return;
    }
    if (rc != 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }

    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK,
                        "{\"status\":\"ok\"}");
    send_data(c, resp);
}
