// File này chứa các hàm kiểm tra cấp quyền truy cập thư mục, file cho user
#include "services/permission_service.h"
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

    int current_id = folder_id;
    PermissionLevel effective = PERM_NONE;

    while (current_id > 0) {
        sqlite3_stmt *stmt = NULL;
        const char *sql_meta = "SELECT owner_id, parent_id FROM folders WHERE id = ?";
        if (sqlite3_prepare_v2(db_global, sql_meta, -1, &stmt, NULL) != SQLITE_OK) {
            break;
        }
        sqlite3_bind_int(stmt, 1, current_id);

        int owner_id = 0;
        int parent_id = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            owner_id = sqlite3_column_int(stmt, 0);
            parent_id = sqlite3_column_type(stmt, 1) != SQLITE_NULL ? sqlite3_column_int(stmt, 1) : 0;
        } else {
            sqlite3_finalize(stmt);
            break;
        }
        sqlite3_finalize(stmt);

        if (owner_id == user_id) return PERM_WRITE; // owner luôn full quyền

        PermissionLevel direct = query_permission(user_id, 1, current_id);
        if (direct > PERM_NONE) {
            if (effective == PERM_NONE) {
                effective = direct;
            } else if (direct < effective) {
                effective = direct; // không cho cấp con cao hơn cha
            }
        }

        current_id = parent_id;
    }

    return effective;
}

PermissionLevel get_file_permission(int user_id, int file_id) {
    if (!db_global || user_id <= 0 || file_id <= 0) return PERM_NONE;

    sqlite3_stmt *stmt = NULL;
    const char *sql_meta = "SELECT owner_id, folder_id FROM files WHERE id = ?";
    if (sqlite3_prepare_v2(db_global, sql_meta, -1, &stmt, NULL) != SQLITE_OK) {
        return PERM_NONE;
    }
    sqlite3_bind_int(stmt, 1, file_id);

    int owner_id = 0;
    int folder_id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        owner_id = sqlite3_column_int(stmt, 0);
        folder_id = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);

    if (owner_id == 0) return PERM_NONE;
    if (owner_id == user_id) return PERM_WRITE;

    PermissionLevel file_direct = query_permission(user_id, 0, file_id);

    PermissionLevel inherited = get_folder_permission(user_id, folder_id);
    PermissionLevel effective = file_direct;
    if (effective == PERM_NONE) effective = inherited;
    else if (inherited > PERM_NONE && inherited < effective) effective = inherited; // giới hạn bởi quyền cha

    return effective;
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
