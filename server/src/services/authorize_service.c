// File này chứa các hàm kiểm tra cấp quyền truy cập thư mục, file cho user
#include "services/authorize_service.h"
#include "database.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdbool.h>

static PermissionLevel query_permission(int user_id, int target_type, int target_id) {
    if (!db_global || user_id <= 0 || target_id <= 0) return PERM_NONE;

    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT permission FROM permissions "
                      "WHERE target_type = ? AND target_id = ? AND user_id = ? "
                      "ORDER BY permission DESC LIMIT 1";
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERM_NONE;
    }
    sqlite3_bind_int(stmt, 1, target_type);
    sqlite3_bind_int(stmt, 2, target_id);
    sqlite3_bind_int(stmt, 3, user_id);

    PermissionLevel perm = PERM_NONE;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int p = sqlite3_column_int(stmt, 0);
        if (p >= PERM_WRITE) perm = PERM_WRITE;
        else if (p >= PERM_READ) perm = PERM_READ;
    }
    sqlite3_finalize(stmt);
    return perm;
}

PermissionLevel get_folder_permission(int user_id, int folder_id) {
    if (!db_global || user_id <= 0 || folder_id <= 0) return PERM_NONE;

    // Owner?
    sqlite3_stmt *stmt = NULL;
    const char *sql_owner = "SELECT owner_id FROM folders WHERE id = ?";
    if (sqlite3_prepare_v2(db_global, sql_owner, -1, &stmt, NULL) != SQLITE_OK) {
        return PERM_NONE;
    }
    sqlite3_bind_int(stmt, 1, folder_id);
    PermissionLevel perm = PERM_NONE;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int owner_id = sqlite3_column_int(stmt, 0);
        if (owner_id == user_id) perm = PERM_WRITE;
    }
    sqlite3_finalize(stmt);
    if (perm == PERM_WRITE) return perm;

    return query_permission(user_id, 1, folder_id);
}

PermissionLevel get_file_permission(int user_id, int file_id) {
    if (!db_global || user_id <= 0 || file_id <= 0) return PERM_NONE;

    // Owner?
    sqlite3_stmt *stmt = NULL;
    const char *sql_owner = "SELECT owner_id FROM files WHERE id = ?";
    if (sqlite3_prepare_v2(db_global, sql_owner, -1, &stmt, NULL) != SQLITE_OK) {
        return PERM_NONE;
    }
    sqlite3_bind_int(stmt, 1, file_id);
    PermissionLevel perm = PERM_NONE;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int owner_id = sqlite3_column_int(stmt, 0);
        if (owner_id == user_id) perm = PERM_WRITE;
    }
    sqlite3_finalize(stmt);
    if (perm == PERM_WRITE) return perm;

    return query_permission(user_id, 0, file_id);
}

bool authorize_folder_access(int user_id, int folder_id, PermissionLevel required) {
    PermissionLevel p = get_folder_permission(user_id, folder_id);
    return p >= required;
}

bool authorize_file_access(int user_id, int file_id, PermissionLevel required) {
    PermissionLevel p = get_file_permission(user_id, file_id);
    return p >= required;
}

bool revoke_permission(int user_id, int target_type, int target_id) {
    if (!db_global || user_id <= 0 || target_id <= 0) return false;
    sqlite3_stmt *stmt = NULL;
    const char *sql = "DELETE FROM permissions WHERE target_type = ? AND target_id = ? AND user_id = ?";
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_int(stmt, 1, target_type);
    sqlite3_bind_int(stmt, 2, target_id);
    sqlite3_bind_int(stmt, 3, user_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}
