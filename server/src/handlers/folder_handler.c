#include "handlers/folder_handler.h"
#include "router.h"
#include "cJSON.h"
#include <sqlite3.h>
#include "database.h"
#include "services/file_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

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

    int parent_id = 0;
    if (cJSON_IsNumber(parent_item)) {
        parent_id = parent_item->valueint;
    }

    // If parent_id == 0, use (or create) user's root folder as parent
    if (parent_id == 0) {
        parent_id = folder_get_or_create_user_root(c->user_id);
    }

    if (parent_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"parent_not_found\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }

    // Insert new folder
    sqlite3_stmt *stmt = NULL;
    const char *sql_insert =
        "INSERT INTO folders(name, parent_id, owner_id, user_root) "
        "VALUES(?, ?, ?, 0)";
    if (sqlite3_prepare_v2(db_global, sql_insert, -1, &stmt, NULL) != SQLITE_OK) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }

    sqlite3_bind_text(stmt, 1, name_item->valuestring, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, parent_id);
    sqlite3_bind_int(stmt, 3, c->user_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    cJSON_Delete(root);

    if (rc != SQLITE_DONE) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }

    int new_id = (int)sqlite3_last_insert_rowid(db_global);

    cJSON *resp_root = cJSON_CreateObject();
    cJSON_AddStringToObject(resp_root, "status", "ok");
    cJSON_AddNumberToObject(resp_root, "id", new_id);
    cJSON_AddStringToObject(resp_root, "name", name_item->valuestring);
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

    int root_id = folder_get_or_create_user_root(c->user_id);
    if (root_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }

    // Only list direct children of root (top-level folders of the user)
    const char *sql =
        "SELECT id, name FROM folders "
        "WHERE owner_id = ? AND user_root = 0 AND parent_id = ? "
        "ORDER BY id";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }

    sqlite3_bind_int(stmt, 1, c->user_id);
    sqlite3_bind_int(stmt, 2, root_id);

    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "folders", items);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *name = sqlite3_column_text(stmt, 1);

        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", id);
        cJSON_AddStringToObject(item, "name", (const char *)name);
        cJSON_AddItemToArray(items, item);
    }

    sqlite3_finalize(stmt);

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

    // Ensure folder belongs to user and is not root folder
    sqlite3_stmt *stmt = NULL;
    const char *sql_check =
        "SELECT user_root FROM folders WHERE id = ? AND owner_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_check, -1, &stmt, NULL) != SQLITE_OK) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }
    sqlite3_bind_int(stmt, 1, folder_id);
    sqlite3_bind_int(stmt, 2, c->user_id);

    int user_root = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_root = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (user_root < 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"folder_not_found_or_not_owner\"}");
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

    const char *sql_delete = "DELETE FROM folders WHERE id = ? AND owner_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_delete, -1, &stmt, NULL) != SQLITE_OK) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }
    sqlite3_bind_int(stmt, 1, folder_id);
    sqlite3_bind_int(stmt, 2, c->user_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
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
    const char *username = user_item->valuestring;
    int permission = perm_item->valueint;
    cJSON_Delete(root);

    // Ensure current user owns the folder
    sqlite3_stmt *stmt = NULL;
    const char *sql_check =
        "SELECT id FROM folders WHERE id = ? AND owner_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_check, -1, &stmt, NULL) != SQLITE_OK) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }
    sqlite3_bind_int(stmt, 1, folder_id);
    sqlite3_bind_int(stmt, 2, c->user_id);

    int exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    if (!exists) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"folder_not_found_or_not_owner\"}");
        send_data(c, resp);
        return;
    }

    // Lookup target user id
    const char *sql_user =
        "SELECT id FROM users WHERE username = ?";
    if (sqlite3_prepare_v2(db_global, sql_user, -1, &stmt, NULL) != SQLITE_OK) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    int target_user_id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        target_user_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (target_user_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"user_not_found\"}");
        send_data(c, resp);
        return;
    }

    const char *sql_upsert =
        "INSERT INTO permissions(target_type, target_id, user_id, permission) "
        "VALUES(1, ?, ?, ?) "
        "ON CONFLICT(target_type, target_id, user_id) "
        "DO UPDATE SET permission=excluded.permission";
    if (sqlite3_prepare_v2(db_global, sql_upsert, -1, &stmt, NULL) != SQLITE_OK) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }
    sqlite3_bind_int(stmt, 1, folder_id);
    sqlite3_bind_int(stmt, 2, target_user_id);
    sqlite3_bind_int(stmt, 3, permission);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
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
    const char *type = type_item->valuestring;
    const char *new_name = name_item->valuestring;
    cJSON_Delete(root);

    const char *sql_update = NULL;

    if (strcmp(type, "folder") == 0) {
        // Cannot rename root folder
        sqlite3_stmt *stmt = NULL;
        const char *sql_check =
            "SELECT user_root FROM folders WHERE id = ? AND owner_id = ?";
        if (sqlite3_prepare_v2(db_global, sql_check, -1, &stmt, NULL) != SQLITE_OK) {
            Frame resp;
            build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                                "{\"error\":\"db_error\"}");
            send_data(c, resp);
            return;
        }
        sqlite3_bind_int(stmt, 1, item_id);
        sqlite3_bind_int(stmt, 2, c->user_id);

        int user_root = -1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user_root = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        if (user_root < 0) {
            Frame resp;
            build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                                "{\"error\":\"folder_not_found_or_not_owner\"}");
            send_data(c, resp);
            return;
        }
        if (user_root == 1) {
            Frame resp;
            build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                                "{\"error\":\"cannot_rename_root_folder\"}");
            send_data(c, resp);
            return;
        }

        sql_update =
            "UPDATE folders SET name = ? WHERE id = ? AND owner_id = ?";
    } else if (strcmp(type, "file") == 0) {
        sql_update =
            "UPDATE files SET name = ? WHERE id = ? AND owner_id = ?";
    } else {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"invalid_item_type\"}");
        send_data(c, resp);
        return;
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql_update, -1, &stmt, NULL) != SQLITE_OK) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }
    sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, item_id);
    sqlite3_bind_int(stmt, 3, c->user_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
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

    const char *sql =
        "SELECT f.id, f.name, f.owner_id, p.permission "
        "FROM permissions p "
        "JOIN folders f ON p.target_type = 1 AND p.target_id = f.id "
        "WHERE p.user_id = ? "
        "ORDER BY f.id";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }

    sqlite3_bind_int(stmt, 1, c->user_id);

    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "folders", items);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char *name = sqlite3_column_text(stmt, 1);
        int owner_id = sqlite3_column_int(stmt, 2);
        int perm = sqlite3_column_int(stmt, 3);

        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id", id);
        cJSON_AddStringToObject(item, "name", (const char *)name);
        cJSON_AddNumberToObject(item, "owner_id", owner_id);
        cJSON_AddNumberToObject(item, "permission", perm);
        cJSON_AddItemToArray(items, item);
    }

    sqlite3_finalize(stmt);

    char *json_resp = cJSON_PrintUnformatted(root);
    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    send_data(c, resp);

    free(json_resp);
    cJSON_Delete(root);
}
