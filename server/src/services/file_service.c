#include "services/file_service.h"
#include "database.h"
#include "services/authorize_service.h"
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Helper: Lấy hoặc tạo folder root cho user
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

// Tạo folder mới
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

    // FIX: Dùng SQLITE_TRANSIENT để copy chuỗi an toàn
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

// Parse đường dẫn folder
int parseFolderPath(const int userId, const char* folderPath) {
    if (!db_global || userId <= 0 || !folderPath || folderPath[0] == '\0') {
        return 0;
    }

    char pathCopy[256];
    strncpy(pathCopy, folderPath, sizeof(pathCopy) - 1);
    pathCopy[sizeof(pathCopy) - 1] = '\0';

    char* token;
    char* rest = pathCopy;

    int parentId = folder_get_or_create_user_root(userId);
    if (parentId <= 0) {
        return 0;
    }

    token = strtok_r(rest, "/", &rest);

    while (token != NULL) {
        sqlite3_stmt* stmt = NULL;
        const char* sql_select =
            "SELECT id FROM folders WHERE name = ? AND parent_id = ? AND owner_id = ? "
            "ORDER BY id LIMIT 1";

        if (sqlite3_prepare_v2(db_global, sql_select, -1, &stmt, NULL) != SQLITE_OK) {
            return 0;
        }

        // FIX: Dùng SQLITE_TRANSIENT vì token nằm trên stack
        sqlite3_bind_text(stmt, 1, token, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, parentId);
        sqlite3_bind_int(stmt, 3, userId);

        int folderId = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            folderId = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);

        if (folderId == 0) {
            return 0; 
        }

        parentId = folderId;
        token = strtok_r(NULL, "/", &rest);
    }

    return parentId;
}

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

    // FIX: Dùng SQLITE_TRANSIENT
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

// Lưu metadata folder
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

    // FIX: Dùng SQLITE_TRANSIENT (tránh lỗi bộ nhớ khi dùng json string)
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

// Lấy thông tin folder
cJSON* get_folder_info(int folder_id) {
    if (!db_global || folder_id <= 0) {
        return cJSON_CreateNull();
    }

    sqlite3_stmt* stmt = NULL;
    
    // Join lấy tên owner
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

    // FIX: Tạo JSON xong mới finalize stmt
    cJSON* info = cJSON_CreateObject();
    cJSON_AddNumberToObject(info, "folder_id", folder_id);
    cJSON_AddNumberToObject(info, "parent_id", parent_id);
    cJSON_AddStringToObject(info, "folder_name", name ? (const char*)name : "");
    cJSON_AddNumberToObject(info, "owner_id", owner_id);
    cJSON_AddStringToObject(info, "owner_name", owner_name ? (const char*)owner_name : "");
    cJSON_AddBoolToObject(info, "user_root", user_root != 0);

    sqlite3_finalize(stmt); // <--- Giờ mới an toàn để giải phóng

    cJSON* items = cJSON_CreateArray();
    cJSON_AddItemToObject(info, "items", items);

    // Get sub-folders
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

    // Get files
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

// Xóa permissions cho toàn bộ subtree (folder + files)
void purge_permissions_for_folder_subtree(int folder_id) {
    if (!db_global || folder_id <= 0) return;

    sqlite3_stmt *stmt = NULL;

    // Xóa permissions của các file trong folder hiện tại
    const char *sql_files = "SELECT id FROM files WHERE folder_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_files, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, folder_id);
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

    // Đệ quy xuống các folder con
    const char *sql_children = "SELECT id FROM folders WHERE parent_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_children, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, folder_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int child_id = sqlite3_column_int(stmt, 0);
            purge_permissions_for_folder_subtree(child_id);
        }
    }
    sqlite3_finalize(stmt);

    // Xóa permissions của folder hiện tại
    sqlite3_stmt *del_folder_perm = NULL;
    const char *sql_del_folder_perm = "DELETE FROM permissions WHERE target_type = 1 AND target_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_del_folder_perm, -1, &del_folder_perm, NULL) == SQLITE_OK) {
        sqlite3_bind_int(del_folder_perm, 1, folder_id);
        sqlite3_step(del_folder_perm);
    }
    sqlite3_finalize(del_folder_perm);
}

// Xóa folder (Giả định DB có Cascade hoặc user tự xử lý đệ quy)
int delete_folder(int folder_id) {
    if (!db_global || folder_id <= 0) return 0;

    // Dọn permissions cho toàn bộ subtree trước khi xóa dữ liệu
    purge_permissions_for_folder_subtree(folder_id);

    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_exec(db_global, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) return 0;

    // Xóa permissions của files trong folder
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

    // Xóa permissions của folder
    sqlite3_stmt *stmt_perm = NULL;
    const char *sql_del_folder_perm = "DELETE FROM permissions WHERE target_type = 1 AND target_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_del_folder_perm, -1, &stmt_perm, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt_perm, 1, folder_id);
        sqlite3_step(stmt_perm);
    }
    sqlite3_finalize(stmt_perm);

    // Xóa files
    const char* sql_delete_files = "DELETE FROM files WHERE folder_id = ?";
    rc = sqlite3_prepare_v2(db_global, sql_delete_files, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, folder_id);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);

    // Xóa folder (subfolders sẽ không tự cascade nếu foreign_keys off)
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

// Logic xóa folder của chính mình
int folder_delete_owned(int owner_id, int folder_id) {
    if (!db_global || owner_id <= 0 || folder_id <= 0) return -1;

    sqlite3_stmt* stmt = NULL;
    const char* sql_check =
        "SELECT user_root FROM folders WHERE id = ? AND owner_id = ?";
    if (sqlite3_prepare_v2(db_global, sql_check, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    sqlite3_bind_int(stmt, 1, folder_id);
    sqlite3_bind_int(stmt, 2, owner_id);

    int user_root = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_root = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (user_root < 0) return -1; 
    if (user_root == 1) return -2; // Không được xóa root

    return delete_folder(folder_id) ? 0 : -3;
}

// Share Folder
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
    // FIX: Dùng SQLITE_TRANSIENT
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

    // Insert new
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
    // FIX: Dùng SQLITE_TRANSIENT
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

    // FIX: Tạo JSON trước khi finalize
    file_info = cJSON_CreateObject();
    cJSON_AddNumberToObject(file_info, "file_id", file_id);
    cJSON_AddStringToObject(file_info, "file_name", name ? (const char*)name : "");
    cJSON_AddNumberToObject(file_info, "folder_id", folder_id);
    cJSON_AddNumberToObject(file_info, "owner_id", owner_id);
    cJSON_AddStringToObject(file_info, "storage_hash", storage_hash ? (const char*)storage_hash : "");
    cJSON_AddNumberToObject(file_info, "file_size", (double)size);

    sqlite3_finalize(stmt); // <--- Giờ mới an toàn

    return file_info;
}

// List root folders
cJSON* list_owned_top_folders(int owner_id) {
    if (!db_global || owner_id <= 0) return NULL;

    int root_id = folder_get_or_create_user_root(owner_id);
    if (root_id <= 0) return NULL;

    const char* sql =
        "SELECT id, name FROM folders "
        "WHERE owner_id = ? AND user_root = 0 AND parent_id = ? "
        "ORDER BY id";
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, owner_id);
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
    return root;
}

// List shared folders
cJSON* list_shared_folders(int user_id) {
    if (!db_global || user_id <= 0) return NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    cJSON *folders_only = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "items", items);
    cJSON_AddItemToObject(root, "folders", folders_only); // giữ tương thích cũ

    // Folders shared trực tiếp
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

    // Files shared trực tiếp
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

// Đổi tên
int item_rename(int actor_id, int item_id, const char* item_type, const char* new_name) {
    if (!db_global || actor_id <= 0 || item_id <= 0 || !item_type || !new_name || new_name[0] == '\0') {
        return -1;
    }

    const char *sql_update = NULL;
    sqlite3_stmt *stmt = NULL;
    int owner_id = 0;

    if (strcmp(item_type, "folder") == 0) {
        const char *sql_check =
            "SELECT owner_id, user_root FROM folders WHERE id = ?";
        if (sqlite3_prepare_v2(db_global, sql_check, -1, &stmt, NULL) != SQLITE_OK) {
            return -3;
        }
        sqlite3_bind_int(stmt, 1, item_id);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            owner_id = sqlite3_column_int(stmt, 0);
            // user_root đã bị bỏ qua vì không cần thiết check ở đây
        }
        sqlite3_finalize(stmt);

        if (owner_id == 0) return -1;

        if (owner_id != actor_id) {
            PermissionLevel perm = get_folder_permission(actor_id, item_id);
            if (perm < PERM_WRITE) return -1;
        }

        sql_update = "UPDATE folders SET name = ? WHERE id = ?";
    } else if (strcmp(item_type, "file") == 0) {
        const char *sql_check = "SELECT owner_id FROM files WHERE id = ?";
        if (sqlite3_prepare_v2(db_global, sql_check, -1, &stmt, NULL) != SQLITE_OK) {
            return -3;
        }
        sqlite3_bind_int(stmt, 1, item_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            owner_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        if (owner_id == 0) return -1;

        if (owner_id != actor_id) {
            PermissionLevel perm = get_file_permission(actor_id, item_id);
            if (perm < PERM_WRITE) return -1;
        }

        sql_update = "UPDATE files SET name = ? WHERE id = ?";
    } else {
        return -2;
    }

    if (sqlite3_prepare_v2(db_global, sql_update, -1, &stmt, NULL) != SQLITE_OK) {
        return -3;
    }
    // FIX: Dùng SQLITE_TRANSIENT
    sqlite3_bind_text(stmt, 1, new_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, item_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        return -3;
    }
    return 0;
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
    // FIX: SQLITE_TRANSIENT
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
