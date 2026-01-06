#include "services/folder_service.h"
#include "database.h"
#include "services/permission_service.h"
#include "cJSON.h"
#include "services/file_service.h"
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
    char **hashes = NULL;
    int hash_count = 0;
    int hash_cap = 0;

    while (count > 0) {
        int current = stack[--count];
        sqlite3_stmt *stmt = NULL;

        const char *sql_files = "SELECT id FROM files WHERE folder_id = ?";
        if (sqlite3_prepare_v2(db_global, sql_files, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, current);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int fid = sqlite3_column_int(stmt, 0);
                // collect storage_hash for GC
                sqlite3_stmt *stmt_hash = NULL;
                const char *sql_hash = "SELECT storage_hash FROM files WHERE id = ?";
                if (sqlite3_prepare_v2(db_global, sql_hash, -1, &stmt_hash, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(stmt_hash, 1, fid);
                    if (sqlite3_step(stmt_hash) == SQLITE_ROW) {
                        const unsigned char *h = sqlite3_column_text(stmt_hash, 0);
                        if (h) {
                            if (hash_count >= hash_cap) {
                                hash_cap = hash_cap == 0 ? 16 : hash_cap * 2;
                                hashes = realloc(hashes, sizeof(char*) * hash_cap);
                            }
                            if (hashes) {
                                hashes[hash_count] = strdup((const char*)h);
                                hash_count++;
                            }
                        }
                    }
                }
                sqlite3_finalize(stmt_hash);
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
    // remove duplicate hashes
    for (int i = 0; i < hash_count; i++) {
        if (!hashes[i]) continue;
        for (int j = i + 1; j < hash_count; j++) {
            if (hashes[j] && strcmp(hashes[i], hashes[j]) == 0) {
                free(hashes[j]);
                hashes[j] = NULL;
            }
        }
    }

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

    // GC storage hashes if no longer referenced
    for (int i = 0; i < hash_count; i++) {
        if (!hashes || !hashes[i]) continue;
        sqlite3_stmt *stmt_gc = NULL;
        const char *sql_count = "SELECT COUNT(1) FROM files WHERE storage_hash = ?";
        int cnt = 1;
        if (sqlite3_prepare_v2(db_global, sql_count, -1, &stmt_gc, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt_gc, 1, hashes[i], -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt_gc) == SQLITE_ROW) {
                cnt = sqlite3_column_int(stmt_gc, 0);
            }
        }
        sqlite3_finalize(stmt_gc);
        if (cnt == 0) {
            char path[512];
            snprintf(path, sizeof(path), "data/storage/%s", hashes[i]);
            unlink(path);
        }
        free(hashes[i]);
    }
    free(hashes);

    return 1; 
}

cJSON* search_folders(int user_id, const char *keyword) {
    if (!db_global || user_id <= 0 || !keyword) return NULL;

    char pattern[256];
    if (snprintf(pattern, sizeof(pattern), "%%%s%%", keyword) >= (int)sizeof(pattern)) {
        return NULL; // keyword quá dài
    }

    cJSON *items = cJSON_CreateArray();
    if (!items) return NULL;

    const char *sql =
        "SELECT f.id, f.name, f.owner_id, u.username "
        "FROM folders f JOIN users u ON f.owner_id = u.id "
        "WHERE lower(f.name) LIKE lower(?) "
        "ORDER BY f.id";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char *name = sqlite3_column_text(stmt, 1);
            int owner_id = sqlite3_column_int(stmt, 2);
            const unsigned char *owner_name = sqlite3_column_text(stmt, 3);

            PermissionLevel perm = get_folder_permission(user_id, id);
            if (perm < PERM_READ) continue;

            cJSON *item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", id);
            cJSON_AddStringToObject(item, "name", name ? (const char *)name : "");
            cJSON_AddNumberToObject(item, "owner_id", owner_id);
            cJSON_AddStringToObject(item, "owner_name", owner_name ? (const char *)owner_name : "");
            cJSON_AddNumberToObject(item, "permission", perm);
            cJSON_AddStringToObject(item, "type", "folder");
            cJSON_AddItemToArray(items, item);
        }
        sqlite3_finalize(stmt);
    }

    return items;
}

static int folder_name_exists(const char *name, int parent_id) {
    sqlite3_stmt *stmt = NULL;
    int exists = 0;
    const char *sql = "SELECT 1 FROM folders WHERE parent_id = ? AND name = ? LIMIT 1";
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (parent_id > 0) sqlite3_bind_int(stmt, 1, parent_id);
        else sqlite3_bind_null(stmt, 1);
        sqlite3_bind_text(stmt, 2, name, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) exists = 1;
    }
    sqlite3_finalize(stmt);
    return exists;
}

static void make_unique_folder_name(char *name_buf, size_t buf_size, int parent_id) {
    if (!name_buf || buf_size == 0) return;
    if (!folder_name_exists(name_buf, parent_id)) return;
    int counter = 1;
    char base[256];
    snprintf(base, sizeof(base), "%s", name_buf);
    do {
        snprintf(name_buf, buf_size, "%s_copy%d", base, counter++);
    } while (folder_name_exists(name_buf, parent_id) && counter < 1000);
}

static int is_descendant(int folder_id, int potential_parent) {
    if (folder_id == potential_parent) return 1;
    int current = folder_id;
    sqlite3_stmt *stmt = NULL;
    while (current > 0) {
        const char *sql = "SELECT parent_id FROM folders WHERE id = ?";
        if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) break;
        sqlite3_bind_int(stmt, 1, current);
        int parent = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            parent = sqlite3_column_type(stmt, 0) != SQLITE_NULL ? sqlite3_column_int(stmt, 0) : 0;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
        if (parent == 0) break;
        if (parent == potential_parent) return 1;
        current = parent;
    }
    return 0;
}

static int copy_folder_recursive(int actor_id, int src_folder_id, int dest_folder_id, int *out_new_id) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT name FROM folders WHERE id = ?";
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) return -3;
    sqlite3_bind_int(stmt, 1, src_folder_id);
    char name[256] = {0};
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *n = sqlite3_column_text(stmt, 0);
        if (n) snprintf(name, sizeof(name), "%s", n);
    } else {
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);

    make_unique_folder_name(name, sizeof(name), dest_folder_id);

    const char *sql_insert =
        "INSERT INTO folders(name, parent_id, owner_id, user_root) VALUES(?, ?, ?, 0)";
    if (sqlite3_prepare_v2(db_global, sql_insert, -1, &stmt, NULL) != SQLITE_OK) return -3;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    if (dest_folder_id > 0) sqlite3_bind_int(stmt, 2, dest_folder_id);
    else sqlite3_bind_null(stmt, 2);
    sqlite3_bind_int(stmt, 3, actor_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) return -3;
    int new_folder_id = (int)sqlite3_last_insert_rowid(db_global);
    if (out_new_id) *out_new_id = new_folder_id;

    // copy files in this folder
    const char *sql_files = "SELECT id FROM files WHERE folder_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_files, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, src_folder_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int fid = sqlite3_column_int(stmt, 0);
            copy_file(actor_id, fid, new_folder_id, NULL);
        }
    }
    sqlite3_finalize(stmt);

    // copy subfolders
    const char *sql_sub = "SELECT id FROM folders WHERE parent_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_sub, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, src_folder_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int child_id = sqlite3_column_int(stmt, 0);
            copy_folder_recursive(actor_id, child_id, new_folder_id, NULL);
        }
    }
    sqlite3_finalize(stmt);
    return 0;
}

int copy_folder(int actor_id, int src_folder_id, int dest_folder_id, int *out_new_id) {
    if (!db_global || actor_id <= 0 || src_folder_id <= 0 || dest_folder_id <= 0) return -1;
    if (!authorize_folder_access(actor_id, src_folder_id, PERM_WRITE)) return -1;
    if (!authorize_folder_access(actor_id, dest_folder_id, PERM_WRITE)) return -2;
    if (is_descendant(dest_folder_id, src_folder_id)) return -2; // cannot copy into itself
    return copy_folder_recursive(actor_id, src_folder_id, dest_folder_id, out_new_id);
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
