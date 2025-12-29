#include "handlers/folder_handler.h"
#include "router.h"
#include "cJSON.h"
#include "database.h"
#include "services/folder_service.h"
#include "services/item_service.h"
#include "services/authorize_service.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void handle_cmd_list_shared_folders(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;
    printf("[CMD:LIST_SHARED_ITEMS][INFO] user_id=%d\n", c->user_id);

    if (!db_global || c->user_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authenticated\"}");
        send_data(c, resp);
        return;
    }

    cJSON *root = list_shared_items(c->user_id);
    if (!root) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }

    char *json_resp = cJSON_PrintUnformatted(root);
    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    send_data(c, resp);

    free(json_resp);
    cJSON_Delete(root);
}

void handle_cmd_list(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;

    if (!db_global || !c->logged_in) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authenticated\"}");
        send_data(c, resp);
        return;
    }

    int folder_id = 0;

    if (f->payload_len > 0) {
        cJSON *root = cJSON_Parse((char *)f->payload);
        if (root) {
            cJSON *folder_item = cJSON_GetObjectItemCaseSensitive(root, "folder_id");
            if (cJSON_IsNumber(folder_item)) {
                folder_id = folder_item->valueint;
            }
            cJSON_Delete(root);
        }
    }

    if (folder_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"folder_is_not_exist\"}");
        send_data(c, resp);
        return;
    }

    if (!authorize_folder_access(c->user_id, folder_id, PERM_READ)) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authorized\"}");
        send_data(c, resp);
        return;
    }

    cJSON *folder_info = get_folder_info(folder_id);
    if (!folder_info || cJSON_IsNull(folder_info)) {
        cJSON_Delete(folder_info);
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"folder_not_found\"}");
        send_data(c, resp);
        return;
    }

    cJSON_AddStringToObject(folder_info, "status", "ok");
    PermissionLevel perm = get_folder_permission(c->user_id, folder_id);
    cJSON_AddNumberToObject(folder_info, "permission", perm);
    char *json_resp = cJSON_PrintUnformatted(folder_info);
    cJSON_Delete(folder_info);

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

    printf("[CMD:LIST][SUCCESS] Sent list for folder_id=%d (fd=%d, user_id=%d, request_id=%d)\n",
           folder_id, c->sockfd, c->user_id, f->header.cmd.request_id);

    free(json_resp);
}

void handle_cmd_mkdir(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;
    printf("[CMD:MKDIR][INFO] Processing MKDIR request (fd=%d, user_id=%d, request_id=%d)\n", 
           c->sockfd, c->user_id, f->header.cmd.request_id);

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

    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON *parent_item = cJSON_GetObjectItemCaseSensitive(root, "parent_id");

    if (!cJSON_IsString(name_item) || name_item->valuestring[0] == '\0') {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_name\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }

    char name_buf[256];
    memset(name_buf, 0, sizeof(name_buf));
    snprintf(name_buf, sizeof(name_buf), "%s", name_item->valuestring);

    int parent_id = 0;
    if (cJSON_IsNumber(parent_item)) {
        parent_id = parent_item->valueint;
    }

    if (parent_id > 0 && !authorize_folder_access(c->user_id, parent_id, PERM_WRITE)) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authorized\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }

    int new_id = 0;
    int create_rc = folder_create(c->user_id, parent_id, name_buf, &new_id);
    cJSON_Delete(root);

    if (create_rc != 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }

    cJSON *resp_root = cJSON_CreateObject();
    cJSON_AddStringToObject(resp_root, "status", "ok");
    cJSON_AddNumberToObject(resp_root, "id", new_id);
    cJSON_AddStringToObject(resp_root, "name", name_buf); 
    cJSON_AddNumberToObject(resp_root, "parent_id", parent_id);

    char *json_resp = cJSON_PrintUnformatted(resp_root);
    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    send_data(c, resp);

    free(json_resp);
    cJSON_Delete(resp_root);
}

void handle_cmd_delete_folder(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;
    printf("[CMD:DELETE_FOLDER][INFO] user_id=%d\n", c->user_id);

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

    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(root, "folder_id");
    if (!cJSON_IsNumber(id_item)) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_folder_id\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }
    int folder_id = id_item->valueint;
    cJSON_Delete(root);

    PermissionLevel perm = get_folder_permission(c->user_id, folder_id);
    if (perm < PERM_WRITE) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authorized\"}");
        send_data(c, resp);
        return;
    }

    sqlite3_stmt *stmt = NULL;
    int owner_id = 0;
    int user_root = 0;
    const char *sql_meta = "SELECT owner_id, user_root FROM folders WHERE id = ?";
    if (sqlite3_prepare_v2(db_global, sql_meta, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, folder_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            owner_id = sqlite3_column_int(stmt, 0);
            user_root = sqlite3_column_int(stmt, 1);
        }
    }
    sqlite3_finalize(stmt);

    if (owner_id == 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"folder_not_found\"}");
        send_data(c, resp);
        return;
    }

    if (user_root == 1) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"cannot_delete_root_folder\"}");
        send_data(c, resp);
        return;
    }

    if (!delete_folder(folder_id, c->user_id)) {
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

void handle_cmd_share_folder(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;
    printf("[CMD:SHARE_FOLDER][INFO] user_id=%d\n", c->user_id);

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

    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(root, "folder_id");
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

    int folder_id = id_item->valueint;
    char username_buf[128];
    memset(username_buf, 0, sizeof(username_buf));
    snprintf(username_buf, sizeof(username_buf), "%s", user_item->valuestring ? user_item->valuestring : "");
    int permission = perm_item->valueint;
    
    int share_rc = folder_share_with_user(c->user_id, folder_id, username_buf, permission);
    
    cJSON_Delete(root);

    if (share_rc == -1) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"folder_not_found_or_not_owner\"}");
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

void handle_cmd_rename_folder(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;
    printf("[CMD:RENAME_FOLDER][INFO] user_id=%d\n", c->user_id);

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

    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(root, "folder_id");
    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(root, "new_name");

    if (!cJSON_IsNumber(id_item) || !cJSON_IsString(name_item) || name_item->valuestring[0] == '\0') {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_fields\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }

    int folder_id = id_item->valueint;
    char new_name_buf[256];
    memset(new_name_buf, 0, sizeof(new_name_buf));
    snprintf(new_name_buf, sizeof(new_name_buf), "%s", name_item->valuestring ? name_item->valuestring : "");
    cJSON_Delete(root);

    int rc = folder_rename(c->user_id, folder_id, new_name_buf);
    if (rc == -1) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"folder_not_found_or_not_owner\"}");
        send_data(c, resp);
        return;
    }
    if (rc == -2) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"cannot_rename_root\"}");
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
