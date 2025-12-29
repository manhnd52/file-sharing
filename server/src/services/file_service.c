#include "services/file_service.h"
#include "database.h"
#include "services/authorize_service.h"
#include <sqlite3.h>
#include <stdio.h>

// Lưu metadata file
int file_save_metadata(int owner_id, int parent_folder_id, const char* file_name, const char* storage_hash, uint64_t size) { 
    if (!db_global || owner_id <= 0 || !file_name || file_name[0] == '\0' ||
        !storage_hash || storage_hash[0] == '\0' || size == 0) {
        return 0;
    }

    printf("[FILE] Saving metadata: owner_id=%d, parent_folder_id=%d, file_name=%s, storage_hash=%s, size=%lu\n",
           owner_id, parent_folder_id, file_name, storage_hash, size);

    const char* sql_insert =
        "INSERT INTO files(name, folder_id, owner_id, storage_hash, size) "
        "VALUES(?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt = NULL;

    if (sqlite3_prepare_v2(db_global, sql_insert, -1, &stmt, NULL) != SQLITE_OK) {
        printf("[FILE] Failed to prepare statement: %s\n", sqlite3_errmsg(db_global));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, file_name, -1, SQLITE_TRANSIENT);
    if (parent_folder_id > 0) {
        sqlite3_bind_int(stmt, 2, parent_folder_id);
    } else {
        sqlite3_bind_null(stmt, 2);
    }
    sqlite3_bind_int(stmt, 3, owner_id);
    sqlite3_bind_text(stmt, 4, storage_hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, (sqlite3_int64)size);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[FILE] Failed to save file metadata: %s\n", sqlite3_errmsg(db_global));
        return 0;
    }

    int file_id = (int)sqlite3_last_insert_rowid(db_global);
    return file_id;
}

// Share File
int file_share_with_user(int owner_id, int file_id, const char* username, int permission) {
    if (!db_global || owner_id <= 0 || file_id <= 0 || !username || username[0] == '\0') {
        return -1;
    }

    // Ensure owner owns file
    sqlite3_stmt* stmt = NULL;
    const char* sql_check =
        "SELECT id FROM files WHERE id = ? AND owner_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_check, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_int(stmt, 1, file_id);
    sqlite3_bind_int(stmt, 2, owner_id);
    int exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    if (!exists) return -1;

    // Lookup target user id
    const char* sql_user = "SELECT id FROM users WHERE lower(username) = lower(?)";
    if (sqlite3_prepare_v2(db_global, sql_user, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
    int target_user_id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        target_user_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    if (target_user_id <= 0) return -2;

    // Try update existing
    const char* sql_update =
        "UPDATE permissions SET permission = ? WHERE target_type = 0 AND target_id = ? AND user_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_update, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_int(stmt, 1, permission);
    sqlite3_bind_int(stmt, 2, file_id);
    sqlite3_bind_int(stmt, 3, target_user_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE && sqlite3_changes(db_global) > 0) {
        return 0;
    }

    const char* sql_insert =
        "INSERT INTO permissions(target_type, target_id, user_id, permission) "
        "VALUES(0, ?, ?, ?)";
    if (sqlite3_prepare_v2(db_global, sql_insert, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_int(stmt, 1, file_id);
    sqlite3_bind_int(stmt, 2, target_user_id);
    sqlite3_bind_int(stmt, 3, permission);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -3;
}

// Lấy thông tin file
cJSON* get_file_info(int file_id) {
    cJSON* file_info = cJSON_CreateNull();
    if (!db_global || file_id <= 0) {
        return file_info;
    }

    sqlite3_stmt* stmt = NULL;
    const char* sql_info =
        "SELECT name, folder_id, owner_id, storage_hash, size FROM files WHERE id = ?";
    if (sqlite3_prepare_v2(db_global, sql_info, -1, &stmt, NULL) != SQLITE_OK) {
        return file_info;
    }

    sqlite3_bind_int(stmt, 1, file_id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return file_info;
    }

    const unsigned char* name = sqlite3_column_text(stmt, 0);
    int folder_id = sqlite3_column_int(stmt, 1);
    int owner_id = sqlite3_column_int(stmt, 2);
    const unsigned char* storage_hash = sqlite3_column_text(stmt, 3);
    sqlite3_int64 size = sqlite3_column_int64(stmt, 4);

    file_info = cJSON_CreateObject();
    cJSON_AddNumberToObject(file_info, "file_id", file_id);
    cJSON_AddStringToObject(file_info, "file_name", name ? (const char*)name : "");
    cJSON_AddNumberToObject(file_info, "folder_id", folder_id);
    cJSON_AddNumberToObject(file_info, "owner_id", owner_id);
    cJSON_AddStringToObject(file_info, "storage_hash", storage_hash ? (const char*)storage_hash : "");
    cJSON_AddNumberToObject(file_info, "file_size", (double)size);

    sqlite3_finalize(stmt);

    return file_info;
}

// Xóa file
int delete_file(int file_id) {
    if (!db_global || file_id <= 0) return 0;

    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_exec(db_global, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) return 0;

    // Xóa permissions của file
    sqlite3_stmt *stmt_perm = NULL;
    const char *sql_del_perm = "DELETE FROM permissions WHERE target_type = 0 AND target_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_del_perm, -1, &stmt_perm, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt_perm, 1, file_id);
        sqlite3_step(stmt_perm);
    }
    sqlite3_finalize(stmt_perm);

    const char* sql_delete_file = "DELETE FROM files WHERE id = ?";
    rc = sqlite3_prepare_v2(db_global, sql_delete_file, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db_global, "ROLLBACK;", NULL, NULL, NULL);
        return 0;
    }

    sqlite3_bind_int(stmt, 1, file_id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        sqlite3_exec(db_global, "ROLLBACK;", NULL, NULL, NULL);
        return 0;
    }

    rc = sqlite3_exec(db_global, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db_global, "ROLLBACK;", NULL, NULL, NULL);
        return 0;
    }

    return 1; 
}

int file_get_owner(int file_id) {
    if (!db_global || file_id <= 0) return 0;
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT owner_id FROM files WHERE id = ?";
    int owner = 0;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, file_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            owner = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return owner;
}

int file_rename(int actor_id, int file_id, const char* new_name) {
    if (!db_global || actor_id <= 0 || file_id <= 0 || !new_name || new_name[0] == '\0') {
        return -1;
    }

    sqlite3_stmt *stmt = NULL;
    int owner_id = 0;
    const char *sql_check = "SELECT owner_id FROM files WHERE id = ?";
    if (sqlite3_prepare_v2(db_global, sql_check, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_int(stmt, 1, file_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        owner_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    if (owner_id == 0) return -1;

    if (owner_id != actor_id) {
        PermissionLevel perm = get_file_permission(actor_id, file_id);
        if (perm < PERM_WRITE) return -1;
    }

    const char *sql_update = "UPDATE files SET name = ? WHERE id = ?";
    if (sqlite3_prepare_v2(db_global, sql_update, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, file_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -3;
}
