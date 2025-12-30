#include "services/item_service.h"
#include "services/permission_service.h"
#include "database.h"
#include <sqlite3.h>
#include <string.h>

cJSON* list_permissions(int target_type, int target_id) {
    if (!db_global || target_id <= 0 || (target_type != 0 && target_type != 1)) return NULL;
    const char *sql =
        "SELECT p.user_id, u.username, p.permission "
        "FROM permissions p JOIN users u ON p.user_id = u.id "
        "WHERE p.target_type = ? AND p.target_id = ? "
        "ORDER BY u.username";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return NULL;
    }
    sqlite3_bind_int(stmt, 1, target_type);
    sqlite3_bind_int(stmt, 2, target_id);

    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "permissions", items);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int uid = sqlite3_column_int(stmt, 0);
        const unsigned char *uname = sqlite3_column_text(stmt, 1);
        int perm = sqlite3_column_int(stmt, 2);
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "user_id", uid);
        cJSON_AddStringToObject(item, "username", uname ? (const char*)uname : "");
        cJSON_AddNumberToObject(item, "permission", perm);
        cJSON_AddItemToArray(items, item);
    }
    sqlite3_finalize(stmt);
    return root;
}

int update_permission(int owner_id, int target_type, int target_id, const char* username, int permission) {
    if (!db_global || owner_id <= 0 || target_id <= 0 || !username || username[0] == '\0') return -1;
    if (target_type != 0 && target_type != 1) return -1;

    // Check owner of target
    int target_owner = 0;
    sqlite3_stmt *stmt = NULL;
    const char *sql_owner_folder = "SELECT owner_id FROM folders WHERE id = ?";
    const char *sql_owner_file = "SELECT owner_id FROM files WHERE id = ?";
    const char *sql_owner = target_type == 1 ? sql_owner_folder : sql_owner_file;
    if (sqlite3_prepare_v2(db_global, sql_owner, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_int(stmt, 1, target_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        target_owner = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    if (target_owner == 0) return -1;
    if (target_owner != owner_id) return -1;

    // Lookup target user
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

    // Owner cannot be downgraded
    if (target_user_id == owner_id) return -2;

    // Update existing
    const char* sql_update =
        "UPDATE permissions SET permission = ? WHERE target_type = ? AND target_id = ? AND user_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_update, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_int(stmt, 1, permission);
    sqlite3_bind_int(stmt, 2, target_type);
    sqlite3_bind_int(stmt, 3, target_id);
    sqlite3_bind_int(stmt, 4, target_user_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE && sqlite3_changes(db_global) > 0) return 0;

    // Insert new
    const char* sql_insert =
        "INSERT INTO permissions(target_type, target_id, user_id, permission) VALUES(?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db_global, sql_insert, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_int(stmt, 1, target_type);
    sqlite3_bind_int(stmt, 2, target_id);
    sqlite3_bind_int(stmt, 3, target_user_id);
    sqlite3_bind_int(stmt, 4, permission);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -3;
}

cJSON* list_shared_items(int user_id) {
    if (!db_global || user_id <= 0) return NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    cJSON *folders_only = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "items", items);
    cJSON_AddItemToObject(root, "folders", folders_only); 

    const char *sql_folders =
        "SELECT f.id, f.name, f.owner_id, u.username, p.permission "
        "FROM permissions p "
        "JOIN folders f ON p.target_type = 1 AND p.target_id = f.id "
        "JOIN users u ON f.owner_id = u.id "
        "WHERE p.user_id = ? "
        "ORDER BY f.id";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql_folders, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char *name = sqlite3_column_text(stmt, 1);
            int owner_id = sqlite3_column_int(stmt, 2);
            const unsigned char *owner_name = sqlite3_column_text(stmt, 3);
            int perm = sqlite3_column_int(stmt, 4);

            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", id);
            cJSON_AddStringToObject(item, "name", (const char *)name);
            cJSON_AddNumberToObject(item, "owner_id", owner_id);
            cJSON_AddStringToObject(item, "owner_name", owner_name ? (const char *)owner_name : "");
            cJSON_AddNumberToObject(item, "permission", perm);
            cJSON_AddNumberToObject(item, "target_type", 1);
            cJSON_AddStringToObject(item, "type", "folder");
            cJSON_AddItemToArray(items, item);

            cJSON *folder_item = cJSON_CreateObject();
            cJSON_AddNumberToObject(folder_item, "id", id);
            cJSON_AddStringToObject(folder_item, "name", (const char *)name);
            cJSON_AddNumberToObject(folder_item, "owner_id", owner_id);
            cJSON_AddStringToObject(folder_item, "owner_name", owner_name ? (const char *)owner_name : "");
            cJSON_AddNumberToObject(folder_item, "permission", perm);
            cJSON_AddItemToArray(folders_only, folder_item);
        }
        sqlite3_finalize(stmt);
    }

    const char *sql_files =
        "SELECT fi.id, fi.name, fi.owner_id, u.username, fi.size, p.permission "
        "FROM permissions p "
        "JOIN files fi ON p.target_type = 0 AND p.target_id = fi.id "
        "JOIN users u ON fi.owner_id = u.id "
        "WHERE p.user_id = ? "
        "ORDER BY fi.id";

    if (sqlite3_prepare_v2(db_global, sql_files, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char *name = sqlite3_column_text(stmt, 1);
            int owner_id = sqlite3_column_int(stmt, 2);
            const unsigned char *owner_name = sqlite3_column_text(stmt, 3);
            sqlite3_int64 size = sqlite3_column_int64(stmt, 4);
            int perm = sqlite3_column_int(stmt, 5);

            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", id);
            cJSON_AddStringToObject(item, "name", (const char *)name);
            cJSON_AddNumberToObject(item, "owner_id", owner_id);
            cJSON_AddStringToObject(item, "owner_name", owner_name ? (const char *)owner_name : "");
            cJSON_AddNumberToObject(item, "permission", perm);
            cJSON_AddNumberToObject(item, "size", (double)size);
            cJSON_AddNumberToObject(item, "target_type", 0);
            cJSON_AddStringToObject(item, "type", "file");
            cJSON_AddItemToArray(items, item);
        }
        sqlite3_finalize(stmt);
    }

    return root;
}
