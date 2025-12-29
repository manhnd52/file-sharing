#include "handlers/folder_handler.h"
#include "router.h"
#include "cJSON.h"
#include "database.h"
#include "services/file_service.h"
#include "services/authorize_service.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper: get or create root folder for a user
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

// Handler for MKDIR command
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

    // --- FIX: Copy name to local buffer to use safely after cJSON_Delete ---
    char name_buf[256];
    memset(name_buf, 0, sizeof(name_buf));
    snprintf(name_buf, sizeof(name_buf), "%s", name_item->valuestring);
    // -----------------------------------------------------------------------

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
    // Use name_buf instead of name_item->valuestring
    int create_rc = folder_create(c->user_id, parent_id, name_buf, &new_id);
    
    // Now it's safe to delete root
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
    cJSON_AddStringToObject(resp_root, "name", name_buf); // Use local buffer
    cJSON_AddNumberToObject(resp_root, "parent_id", parent_id);

    char *json_resp = cJSON_PrintUnformatted(resp_root);
    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    send_data(c, resp);

    free(json_resp);
    cJSON_Delete(resp_root);
}

// List folders owned by current user
void handle_cmd_list_own_folders(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;
    printf("[CMD:LIST_OWN_FOLDERS][INFO] user_id=%d\n", c->user_id);

    if (!db_global || c->user_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authenticated\"}");
        send_data(c, resp);
        return;
    }

    cJSON *root = list_owned_top_folders(c->user_id);
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

// Delete folder (and its subtree via cascade)
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

    // PERM_WRITE: nếu là owner thì xóa theo sở hữu, nếu không phải owner thì xóa thực sự (trừ root)
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

    if (owner_id == c->user_id) {
        int del_rc = folder_delete_owned(c->user_id, folder_id);
        if (del_rc == -2) {
            Frame resp;
            build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                                "{\"error\":\"cannot_delete_root_folder\"}");
            send_data(c, resp);
            return;
        }
        if (del_rc != 0) {
            Frame resp;
            build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                                "{\"error\":\"db_error\"}");
            send_data(c, resp);
            return;
        }
    } else {
        if (user_root == 1) {
            Frame resp;
            build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                                "{\"error\":\"cannot_delete_root_folder\"}");
            send_data(c, resp);
            return;
        }
        if (!delete_folder(folder_id)) {
            Frame resp;
            build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                                "{\"error\":\"db_error\"}");
            send_data(c, resp);
            return;
        }
    }

    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK,
                        "{\"status\":\"ok\"}");
    send_data(c, resp);
}

// Delete file with permission handling
void handle_cmd_delete_file(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;
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
                            "{\"error\":\"not_authorized\"}");
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

// Share folder with another user
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
    // FIX: Copy string to buffer safely
    char username_buf[128];
    memset(username_buf, 0, sizeof(username_buf));
    snprintf(username_buf, sizeof(username_buf), "%s", user_item->valuestring ? user_item->valuestring : "");
    int permission = perm_item->valueint;
    
    // Call logic
    int share_rc = folder_share_with_user(c->user_id, folder_id, username_buf, permission);
    
    // Cleanup
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

// Share file with another user
void handle_cmd_share_file(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;
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
    // FIX: Copy string to buffer safely
    char username_buf[128];
    memset(username_buf, 0, sizeof(username_buf));
    snprintf(username_buf, sizeof(username_buf), "%s", user_item->valuestring ? user_item->valuestring : "");
    int permission = perm_item->valueint;
    
    // Call logic
    int share_rc = file_share_with_user(c->user_id, file_id, username_buf, permission);
    
    // Cleanup
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

// List permissions for target (folder/file)
void handle_cmd_list_permissions(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;
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
    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(root, "target_type");
    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(root, "target_id");
    if (!cJSON_IsNumber(type_item) || !cJSON_IsNumber(id_item)) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_fields\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }
    int target_type = type_item->valueint;
    int target_id = id_item->valueint;
    cJSON_Delete(root);

    cJSON *perms = list_permissions(target_type, target_id);
    if (!perms) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }
    char *json_resp = cJSON_PrintUnformatted(perms);
    cJSON_Delete(perms);
    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    send_data(c, resp);
    free(json_resp);
}

// Update permission for a target
void handle_cmd_update_permission(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;
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
    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(root, "target_type");
    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(root, "target_id");
    cJSON *user_item = cJSON_GetObjectItemCaseSensitive(root, "username");
    cJSON *perm_item = cJSON_GetObjectItemCaseSensitive(root, "permission");
    if (!cJSON_IsNumber(type_item) || !cJSON_IsNumber(id_item) || !cJSON_IsString(user_item) || !cJSON_IsNumber(perm_item)) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_fields\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }
    int target_type = type_item->valueint;
    int target_id = id_item->valueint;
    
    // --- FIX: Copy username ra buffer trước khi xóa root ---
    char username_buf[128];
    memset(username_buf, 0, sizeof(username_buf));
    snprintf(username_buf, sizeof(username_buf), "%s", user_item->valuestring ? user_item->valuestring : "");
    // -----------------------------------------------------

    int perm = perm_item->valueint;
    
    // Bây giờ xóa root an toàn
    cJSON_Delete(root);

    int rc = update_permission(c->user_id, target_type, target_id, username_buf, perm);
    
    if (rc == -1) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_owner_or_not_found\"}");
        send_data(c, resp);
        return;
    }
    if (rc == -2) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"user_not_found_or_invalid\"}");
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

// Rename folder or file
void handle_cmd_rename_item(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;
    printf("[CMD:RENAME_ITEM][INFO] user_id=%d\n", c->user_id);

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

    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(root, "item_id");
    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(root, "item_type");
    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(root, "new_name");

    if (!cJSON_IsNumber(id_item) || !cJSON_IsString(type_item) ||
        !cJSON_IsString(name_item) || name_item->valuestring[0] == '\0') {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_fields\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }

    int item_id = id_item->valueint;
    
    // Normalize type
    char type_buf[16];
    memset(type_buf, 0, sizeof(type_buf));
    snprintf(type_buf, sizeof(type_buf), "%s", type_item->valuestring ? type_item->valuestring : "");
    for (size_t i = 0; i < strlen(type_buf); ++i) {
        if (type_buf[i] >= 'A' && type_buf[i] <= 'Z') {
            type_buf[i] = (char)(type_buf[i] - 'A' + 'a');
        }
    }
    
    // Copy new name
    char new_name_buf[256];
    memset(new_name_buf, 0, sizeof(new_name_buf));
    snprintf(new_name_buf, sizeof(new_name_buf), "%s", name_item->valuestring ? name_item->valuestring : "");

    // --- FIX: Use data logic before deleting, or delete after usage ---
    // Since we copied everything to buffers (type_buf, new_name_buf), we can delete now.
    cJSON_Delete(root);
    
    int rc = item_rename(c->user_id, item_id, type_buf, new_name_buf);

    if (rc == -1) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"item_not_found_or_not_owner\"}");
        send_data(c, resp);
        return;
    }
    if (rc == -2) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"invalid_item_type_or_forbidden\"}");
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

// List folders shared with current user
void handle_cmd_list_shared_folders(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;
    printf("[CMD:LIST_SHARED_FOLDERS][INFO] user_id=%d\n", c->user_id);

    if (!db_global || c->user_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authenticated\"}");
        send_data(c, resp);
        return;
    }

    cJSON *root = list_shared_folders(c->user_id);
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
