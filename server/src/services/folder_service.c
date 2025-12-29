#include "services/folder_service.h"
#include "database.h"
#include "services/authorize_service.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int folder_get_or_create_user_root(int user_id) {
    if (!db_global || user_id <= 0) {
        return 0;
    }

    sqlite3_stmt* stmt = NULL;
    int folder_id = 0;

    const char* sql_select =
        "SELECT id FROM folders WHERE owner_id = ? AND user_root = 1 "
        "ORDER BY id LIMIT 1";

    if (sqlite3_prepare_v2(db_global, sql_select, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            folder_id = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);

    if (folder_id > 0) {
        return folder_id;
    }

    const char* sql_insert =
        "INSERT INTO folders(name, parent_id, owner_id, user_root) "
        "VALUES('root', NULL, ?, 1)";
    if (sqlite3_prepare_v2(db_global, sql_insert, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    sqlite3_bind_int(stmt, 1, user_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[FOLDER] Failed to create root folder: %s\n", sqlite3_errmsg(db_global));
        return 0;
    }

    folder_id = (int)sqlite3_last_insert_rowid(db_global);
    return folder_id;
}

int folder_save_metadata(int owner_id, int parent_folder_id, const char* new_folder_name) {
    if (!db_global || owner_id <= 0 || !new_folder_name || new_folder_name[0] == '\0') {
        return 0;
    }

    const char* sql_insert =
        "INSERT INTO folders(name, parent_id, owner_id, user_root) "
        "VALUES(?, ?, ?, 0)";
    sqlite3_stmt* stmt = NULL;

    if (sqlite3_prepare_v2(db_global, sql_insert, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, new_folder_name, -1, SQLITE_TRANSIENT);
    if (parent_folder_id > 0) {
        sqlite3_bind_int(stmt, 2, parent_folder_id);
    } else {
        sqlite3_bind_null(stmt, 2);
    }
    sqlite3_bind_int(stmt, 3, owner_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[FOLDER] Failed to save folder metadata: %s\n", sqlite3_errmsg(db_global));
        return 0;
    }

    int folder_id = (int)sqlite3_last_insert_rowid(db_global);
    return folder_id;
}

int folder_create(int owner_id, int parent_id, const char* name, int* out_id) {
    if (!db_global || owner_id <= 0 || !name || name[0] == '\0') {
        return -1;
    }

    if (parent_id == 0) {
        parent_id = folder_get_or_create_user_root(owner_id);
        if (parent_id <= 0) return -3;
    }

    const char* sql_insert =
        "INSERT INTO folders(name, parent_id, owner_id, user_root) "
        "VALUES(?, ?, ?, 0)";
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql_insert, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, parent_id);
    sqlite3_bind_int(stmt, 3, owner_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        return -3;
    }

    if (out_id) {
        *out_id = (int)sqlite3_last_insert_rowid(db_global);
    }
    return 0;
}

int folder_rename(int actor_id, int folder_id, const char* new_name) {
    if (!db_global || actor_id <= 0 || folder_id <= 0 || !new_name || new_name[0] == '\0') {
        return -1;
    }

    sqlite3_stmt *stmt = NULL;
    int owner_id = 0;
    int user_root = 0;
    const char *sql_check =
        "SELECT owner_id, user_root FROM folders WHERE id = ?";
    if (sqlite3_prepare_v2(db_global, sql_check, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_int(stmt, 1, folder_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        owner_id = sqlite3_column_int(stmt, 0);
        user_root = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);

    if (owner_id == 0) return -1;
    if (user_root == 1) return -2;

    if (owner_id != actor_id) {
        PermissionLevel perm = get_folder_permission(actor_id, folder_id);
        if (perm < PERM_WRITE) return -1;
    }

    const char *sql_update = "UPDATE folders SET name = ? WHERE id = ?";
    if (sqlite3_prepare_v2(db_global, sql_update, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, folder_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -3;
}

int delete_folder(int folder_id, int actor_id) {
    if (!db_global || folder_id <= 0) return 0;

    int capacity = 64;
    int count = 0;
    int *stack = (int *)malloc(sizeof(int) * capacity);
    if (!stack) return 0;
    stack[count++] = folder_id;

    while (count > 0) {
        int current = stack[--count];
        sqlite3_stmt *stmt = NULL;

        const char *sql_files = "SELECT id FROM files WHERE folder_id = ?";
        if (sqlite3_prepare_v2(db_global, sql_files, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, current);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int fid = sqlite3_column_int(stmt, 0);
                sqlite3_stmt *del = NULL;
                const char *sql_del_file_perm = "DELETE FROM permissions WHERE target_type = 0 AND target_id = ?";
                if (sqlite3_prepare_v2(db_global, sql_del_file_perm, -1, &del, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(del, 1, fid);
                    sqlite3_step(del);
                }
                sqlite3_finalize(del);
            }
        }
        sqlite3_finalize(stmt);

        const char *sql_children = "SELECT id FROM folders WHERE parent_id = ?";
        if (sqlite3_prepare_v2(db_global, sql_children, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, current);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int child_id = sqlite3_column_int(stmt, 0);
                if (count >= capacity) {
                    capacity *= 2;
                    int *tmp = realloc(stack, sizeof(int) * capacity);
                    if (!tmp) { free(stack); return 0; }
                    stack = tmp;
                }
                stack[count++] = child_id;
            }
        }
        sqlite3_finalize(stmt);

        sqlite3_stmt *del_folder_perm = NULL;
        const char *sql_del_folder_perm = "DELETE FROM permissions WHERE target_type = 1 AND target_id = ?";
        if (sqlite3_prepare_v2(db_global, sql_del_folder_perm, -1, &del_folder_perm, NULL) == SQLITE_OK) {
            sqlite3_bind_int(del_folder_perm, 1, current);
            sqlite3_step(del_folder_perm);
        }
        sqlite3_finalize(del_folder_perm);
    }
    free(stack);

    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_exec(db_global, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) return 0;

    const char* sql_file_ids = "SELECT id FROM files WHERE folder_id = ?";
    rc = sqlite3_prepare_v2(db_global, sql_file_ids, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, folder_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int fid = sqlite3_column_int(stmt, 0);
            sqlite3_stmt *stmt_del = NULL;
            const char *sql_del_perm = "DELETE FROM permissions WHERE target_type = 0 AND target_id = ?";
            if (sqlite3_prepare_v2(db_global, sql_del_perm, -1, &stmt_del, NULL) == SQLITE_OK) {
                sqlite3_bind_int(stmt_del, 1, fid);
                sqlite3_step(stmt_del);
            }
            sqlite3_finalize(stmt_del);
        }
    }
    sqlite3_finalize(stmt);

    sqlite3_stmt *stmt_perm = NULL;
    const char *sql_del_folder_perm = "DELETE FROM permissions WHERE target_type = 1 AND target_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_del_folder_perm, -1, &stmt_perm, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt_perm, 1, folder_id);
        sqlite3_step(stmt_perm);
    }
    sqlite3_finalize(stmt_perm);

    const char* sql_delete_files = "DELETE FROM files WHERE folder_id = ?";
    rc = sqlite3_prepare_v2(db_global, sql_delete_files, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, folder_id);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);

    const char* sql_delete_folder = "DELETE FROM folders WHERE id = ?";
    rc = sqlite3_prepare_v2(db_global, sql_delete_folder, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, folder_id);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);

    rc = sqlite3_exec(db_global, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db_global, "ROLLBACK;", NULL, NULL, NULL);
        return 0;
    }

    return 1; 
}

int folder_share_with_user(int owner_id, int folder_id, const char* username, int permission) {
    if (!db_global || owner_id <= 0 || folder_id <= 0 || !username || username[0] == '\0') {
        return -1;
    }

    sqlite3_stmt* stmt = NULL;
    const char* sql_check =
        "SELECT id FROM folders WHERE id = ? AND owner_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_check, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_int(stmt, 1, folder_id);
    sqlite3_bind_int(stmt, 2, owner_id);
    int exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    if (!exists) return -1;

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
        "UPDATE permissions SET permission = ? WHERE target_type = 1 AND target_id = ? AND user_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_update, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_int(stmt, 1, permission);
    sqlite3_bind_int(stmt, 2, folder_id);
    sqlite3_bind_int(stmt, 3, target_user_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE && sqlite3_changes(db_global) > 0) {
        return 0;
    }

    const char* sql_insert =
        "INSERT INTO permissions(target_type, target_id, user_id, permission) "
        "VALUES(1, ?, ?, ?)";
    if (sqlite3_prepare_v2(db_global, sql_insert, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_int(stmt, 1, folder_id);
    sqlite3_bind_int(stmt, 2, target_user_id);
    sqlite3_bind_int(stmt, 3, permission);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -3;
}

cJSON* get_folder_info(int folder_id) {
    if (!db_global || folder_id <= 0) {
        return cJSON_CreateNull();
    }

    sqlite3_stmt* stmt = NULL;
    
    const char* sql_info =
        "SELECT f.name, f.parent_id, f.owner_id, f.user_root, u.username "
        "FROM folders f JOIN users u ON f.owner_id = u.id "
        "WHERE f.id = ?";
    if (sqlite3_prepare_v2(db_global, sql_info, -1, &stmt, NULL) != SQLITE_OK) {
        return cJSON_CreateNull();
    }

    sqlite3_bind_int(stmt, 1, folder_id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return cJSON_CreateNull();
    }

    const unsigned char* name = sqlite3_column_text(stmt, 0);
    int parent_id = sqlite3_column_type(stmt, 1) != SQLITE_NULL ? sqlite3_column_int(stmt, 1) : 0;
    int owner_id = sqlite3_column_int(stmt, 2);
    int user_root = sqlite3_column_int(stmt, 3);
    const unsigned char* owner_name = sqlite3_column_text(stmt, 4);

    cJSON* info = cJSON_CreateObject();
    cJSON_AddNumberToObject(info, "folder_id", folder_id);
    cJSON_AddNumberToObject(info, "parent_id", parent_id);
    cJSON_AddStringToObject(info, "folder_name", name ? (const char*)name : "");
    cJSON_AddNumberToObject(info, "owner_id", owner_id);
    cJSON_AddStringToObject(info, "owner_name", owner_name ? (const char*)owner_name : "");
    cJSON_AddBoolToObject(info, "user_root", user_root != 0);

    sqlite3_finalize(stmt);

    cJSON* items = cJSON_CreateArray();
    cJSON_AddItemToObject(info, "items", items);

    const char* sql_folders =
        "SELECT f.id, f.name, u.username FROM folders f JOIN users u ON f.owner_id = u.id WHERE f.parent_id = ? ORDER BY f.name";
    if (sqlite3_prepare_v2(db_global, sql_folders, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, folder_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char* child_name = sqlite3_column_text(stmt, 1);
            const unsigned char* child_owner = sqlite3_column_text(stmt, 2);

            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", id);
            cJSON_AddStringToObject(item, "name", child_name ? (const char*)child_name : "");
            cJSON_AddStringToObject(item, "type", "folder");
            cJSON_AddStringToObject(item, "owner_name", child_owner ? (const char*)child_owner : "");
            cJSON_AddItemToArray(items, item);
        }
        sqlite3_finalize(stmt);
    }

    const char* sql_files =
        "SELECT f.id, f.name, f.size, u.username FROM files f JOIN users u ON f.owner_id = u.id WHERE f.folder_id = ? ORDER BY f.name";
    if (sqlite3_prepare_v2(db_global, sql_files, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, folder_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char* file_name = sqlite3_column_text(stmt, 1);
            sqlite3_int64 size = sqlite3_column_int64(stmt, 2);
            const unsigned char* file_owner = sqlite3_column_text(stmt, 3);

            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", id);
            cJSON_AddStringToObject(item, "name", file_name ? (const char*)file_name : "");
            cJSON_AddStringToObject(item, "type", "file");
            cJSON_AddNumberToObject(item, "size", (double)size);
            cJSON_AddStringToObject(item, "owner_name", file_owner ? (const char*)file_owner : "");
            cJSON_AddItemToArray(items, item);
        }
        sqlite3_finalize(stmt);
    }

    return info;
}
