#include "handlers/cmd_handler.h"
#include "router.h"
#include "cJSON.h"
#include <sqlite3.h>
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

// Helper: get or create root folder for a user
static int get_or_create_user_root_folder(int user_id) {
    if (!db_global || user_id <= 0)
        return 0;

    sqlite3_stmt *stmt = NULL;
    int folder_id = 0;

    const char *sql_select =
        "SELECT id FROM folders WHERE owner_id = ? AND user_root = 1 "
        "ORDER BY id LIMIT 1";
    if (sqlite3_prepare_v2(db_global, sql_select, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            folder_id = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);

    if (folder_id > 0)
        return folder_id;

    // Not found: create one
    const char *sql_insert =
        "INSERT INTO folders(name, parent_id, owner_id, user_root) "
        "VALUES('root', NULL, ?, 1)";
    if (sqlite3_prepare_v2(db_global, sql_insert, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, user_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE)
        return 0;

    folder_id = (int)sqlite3_last_insert_rowid(db_global);
    return folder_id;
}

void handle_cmd_list(Conn *c, Frame *f, const char *cmd) {
    (void)cmd;

    if (!db_global || c->user_id <= 0) {
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

    sqlite3_stmt *stmt = NULL;
    int current_folder_id = folder_id;
    int parent_id = 0;
    char folder_name[256] = {0};

    // Resolve root folder for user if folder_id == 0
    if (current_folder_id == 0) {
        current_folder_id = get_or_create_user_root_folder(c->user_id);
        if (current_folder_id <= 0) {
            Frame resp;
            build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                                "{\"error\":\"db_error\"}");
            send_data(c, resp);
            return;
        }
        strncpy(folder_name, "root", sizeof(folder_name) - 1);
    }

    // Load folder info (name, parent_id)
    if (current_folder_id > 0 && folder_name[0] == '\0') {
        const char *sql_info =
            "SELECT name, parent_id FROM folders WHERE id = ?";
        if (sqlite3_prepare_v2(db_global, sql_info, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, current_folder_id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char *name = sqlite3_column_text(stmt, 0);
                if (name) {
                    strncpy(folder_name, (const char *)name, sizeof(folder_name) - 1);
                }
                parent_id = sqlite3_column_type(stmt, 1) != SQLITE_NULL
                                ? sqlite3_column_int(stmt, 1)
                                : 0;
            }
        }
        sqlite3_finalize(stmt);
    }

    if (current_folder_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"folder_not_found\"}");
        send_data(c, resp);
        return;
    }

    // Build JSON response
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddNumberToObject(response, "folder_id", current_folder_id);
    cJSON_AddNumberToObject(response, "parent_id", parent_id);
    cJSON_AddStringToObject(response, "folder_name",
                            folder_name[0] ? folder_name : "/");

    cJSON *items = cJSON_CreateArray();
    cJSON_AddItemToObject(response, "items", items);

    // List child folders
    const char *sql_sub =
        "SELECT id, name FROM folders WHERE parent_id = ? ORDER BY name";
    if (sqlite3_prepare_v2(db_global, sql_sub, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, current_folder_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char *name = sqlite3_column_text(stmt, 1);

            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", id);
            cJSON_AddStringToObject(item, "name",
                                    name ? (const char *)name : "");
            cJSON_AddStringToObject(item, "type", "folder");
            cJSON_AddItemToArray(items, item);
        }
    }
    sqlite3_finalize(stmt);

    // List files in folder
    const char *sql_files =
        "SELECT id, name, size FROM files WHERE folder_id = ? ORDER BY name";
    if (sqlite3_prepare_v2(db_global, sql_files, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, current_folder_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char *name = sqlite3_column_text(stmt, 1);
            sqlite3_int64 size = sqlite3_column_int64(stmt, 2);

            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", id);
            cJSON_AddStringToObject(item, "name",
                                    name ? (const char *)name : "");
            cJSON_AddStringToObject(item, "type", "file");
            cJSON_AddNumberToObject(item, "size", (double)size);
            cJSON_AddItemToArray(items, item);
        }
    }
    sqlite3_finalize(stmt);

    char *json_resp = cJSON_PrintUnformatted(response);
    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    send_data(c, resp);

    printf("[CMD:LIST][SUCCESS] Sent list for folder_id=%d (fd=%d, user_id=%d, request_id=%d)\n",
           current_folder_id, c->sockfd, c->user_id, f->header.cmd.request_id);

    free(json_resp);
    cJSON_Delete(response);
}

// Handler for UPLOAD command
void handle_cmd_upload(Conn *c, Frame *f, const char *cmd) {
    printf("[CMD:UPLOAD][INFO] Processing UPLOAD request (fd=%d, user_id=%d, request_id=%d)\n", 
           c->sockfd, c->user_id, ntohl(f->header.cmd.request_id));
    
    // Parse JSON payload
    cJSON *root = cJSON_Parse((char *)f->payload);
    if (!root) {
        printf("[CMD:UPLOAD][ERROR] Failed to parse JSON payload (fd=%d, user_id=%d)\n", c->sockfd, c->user_id);
        return;
    }
    
    cJSON *filename_item = cJSON_GetObjectItem(root, "filename");
    cJSON *size_item = cJSON_GetObjectItem(root, "size");
    
    const char *filename = (filename_item && cJSON_IsString(filename_item)) ? filename_item->valuestring : "unknown";
    int file_size = (size_item && cJSON_IsNumber(size_item)) ? size_item->valueint : 0;
    
    printf("[CMD:UPLOAD][INFO] File metadata: filename='%s', size=%d bytes (fd=%d, user_id=%d)\n", 
           filename, file_size, c->sockfd, c->user_id);
    
    // Build response
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ready");
    cJSON_AddStringToObject(response, "message", "Ready to receive data");
    cJSON_AddNumberToObject(response, "chunk_size", 8192);
    
    char *json_resp = cJSON_PrintUnformatted(response);
    
    Frame resp;
    build_respond_frame(&resp, ntohl(f->header.cmd.request_id), STATUS_OK, json_resp);
    
    pthread_mutex_lock(&c->write_lock);
    send_frame(c->sockfd, &resp);
    pthread_mutex_unlock(&c->write_lock);
    
    free(json_resp);
    cJSON_Delete(response);
    cJSON_Delete(root);
}

// Handler for DOWNLOAD command
void handle_cmd_download(Conn *c, Frame *f, const char *cmd) {
    printf("[CMD:DOWNLOAD][INFO] Processing DOWNLOAD request (fd=%d, user_id=%d, request_id=%d)\n", 
           c->sockfd, c->user_id, f->header.cmd.request_id);
    
    // TODO: Parse JSON to get file path
    // TODO: Check file exists and permissions
    // TODO: Send RESPOND with file metadata
    // TODO: Start sending DATA frames
    
    Frame resp;
    const char *json_resp = "{\"status\":\"ok\",\"file_size\":1024}";
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    
    pthread_mutex_lock(&c->write_lock);
    send_frame(c->sockfd, &resp);
    pthread_mutex_unlock(&c->write_lock);
}

// Handler for PING command
void handle_cmd_ping(Conn *c, Frame *f, const char *cmd) {
    printf("[CMD:PING][INFO] PING received (fd=%d, user_id=%d, request_id=%d)\n", 
           c->sockfd, c->user_id, f->header.cmd.request_id);
    
    // Build response
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "pong");
    
    char *json_resp = cJSON_PrintUnformatted(response);
    
    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    
    pthread_mutex_lock(&c->write_lock);
    send_frame(c->sockfd, &resp);
    pthread_mutex_unlock(&c->write_lock);
    
    free(json_resp);
    cJSON_Delete(response);
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
        parent_id = get_or_create_user_root_folder(c->user_id);
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

    int root_id = get_or_create_user_root_folder(c->user_id);
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
