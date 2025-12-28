#include "services/file_service.h"
#include "database.h"
#include <sqlite3.h>
#include <stdio.h>

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

// Search folder id by its path, return folder id if found, otherwise return 0
// Input: folderPath - path to the folder in logical storage (e.g. /audios)
// - userId: owner of the file
// Returns: folder ID of `audios` folder if found, otherwise 0
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

        sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);
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

// Save new file metadata, return file ID on success, 0 on failure
// Input:
// - ownerId: user ID of the file owner
// - parentFolderId: ID of the parent folder
// - fileName: name of the new file to create (e.g. report.pdf)
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

// Save new folder metadata, return folder ID on success, 0 on failure
// Input:
// - ownerId: user ID of the folder owner
// - parentFolderId: ID of the parent folder
// - newFolderName: path of the new folder to create (e.g. documents)
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

    sqlite3_bind_text(stmt, 1, new_folder_name, -1, SQLITE_STATIC);
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

/**
 * get_folder_info - Lấy thông tin chi tiết của một folder
 *
 * @param folder_id: ID của folder cần truy vấn (phải > 0)
 *
 * @return con trỏ `cJSON*` chứa object với cấu trúc:
 ```json
 {
    "folder_id": int,
    "parent_id": int,
    "folder_name": string,
    "owner_id": int,
    "user_root": bool,
    "items": [ { "id": int, "name": string, "type": "folder" }, 
               { "id": int, "name": string, "type": "file", "size": int64 } ... ]
  }
 ```
 * Hoặc `cJSON` null nếu folder không tồn tại hoặc input không hợp lệ.
 *
 */
cJSON* get_folder_info(int folder_id) {
    if (!db_global || folder_id <= 0) {
        return cJSON_CreateNull();
    }

    sqlite3_stmt* stmt = NULL;
    
    const char* sql_info =
        "SELECT name, parent_id, owner_id, user_root FROM folders WHERE id = ?";
    if (sqlite3_prepare_v2(db_global, sql_info, -1, &stmt, NULL) != SQLITE_OK) {
        return cJSON_CreateNull();
    }

    sqlite3_bind_int(stmt, 1, folder_id);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return cJSON_CreateNull();
    }

    const unsigned char* name = sqlite3_column_text(stmt, 0);
    int parent_id = sqlite3_column_type(stmt, 1) != SQLITE_NULL
                        ? sqlite3_column_int(stmt, 1)
                        : 0;
    int owner_id = sqlite3_column_int(stmt, 2);
    int user_root = sqlite3_column_int(stmt, 3);

    sqlite3_finalize(stmt);

    cJSON* info = cJSON_CreateObject();
    cJSON_AddNumberToObject(info, "folder_id", folder_id);
    cJSON_AddNumberToObject(info, "parent_id", parent_id);
    cJSON_AddStringToObject(info, "folder_name", name ? (const char*)name : "");
    cJSON_AddNumberToObject(info, "owner_id", owner_id);
    cJSON_AddBoolToObject(info, "user_root", user_root != 0);

    cJSON* items = cJSON_CreateArray();
    cJSON_AddItemToObject(info, "items", items);

    const char* sql_folders =
        "SELECT id, name FROM folders WHERE parent_id = ? ORDER BY name";
    if (sqlite3_prepare_v2(db_global, sql_folders, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, folder_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char* child_name = sqlite3_column_text(stmt, 1);

            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", id);
            cJSON_AddStringToObject(item, "name", child_name ? (const char*)child_name : "");
            cJSON_AddStringToObject(item, "type", "folder");
            cJSON_AddItemToArray(items, item);
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    const char* sql_files =
        "SELECT id, name, size FROM files WHERE folder_id = ? ORDER BY name";
    if (sqlite3_prepare_v2(db_global, sql_files, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, folder_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char* file_name = sqlite3_column_text(stmt, 1);
            sqlite3_int64 size = sqlite3_column_int64(stmt, 2);

            cJSON* item = cJSON_CreateObject();
            cJSON_AddNumberToObject(item, "id", id);
            cJSON_AddStringToObject(item, "name", file_name ? (const char*)file_name : "");
            cJSON_AddStringToObject(item, "type", "file");
            cJSON_AddNumberToObject(item, "size", (double)size);
            cJSON_AddItemToArray(items, item);
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    return info;
}

int delete_folder(int folder_id) {
    if (!db_global || folder_id <= 0) {
        return 0; // failure
    }

    sqlite3_stmt* stmt = NULL;
    int rc;

    // Bắt đầu transaction để đảm bảo atomic
    rc = sqlite3_exec(db_global, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) return 0;

    // Xóa files trong folder
    const char* sql_delete_files = "DELETE FROM files WHERE folder_id = ?";
    rc = sqlite3_prepare_v2(db_global, sql_delete_files, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, folder_id);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    stmt = NULL;

    // Xóa folder (các subfolders sẽ tự cascade nếu schema đúng)
    const char* sql_delete_folder = "DELETE FROM folders WHERE id = ?";
    rc = sqlite3_prepare_v2(db_global, sql_delete_folder, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, folder_id);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);

    // Kết thúc transaction
    rc = sqlite3_exec(db_global, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db_global, "ROLLBACK;", NULL, NULL, NULL);
        return 0;
    }

    return 1; // success
}

int delete_file(int file_id) {
    if (!db_global || file_id <= 0) {
        return 0; // failure
    }

    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_exec(db_global, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) return 0;

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

/*
Lấy thông tin file theo file ID → trả về cJSON object chứa metadata file
Output cJSON object mẫu:
```
{
    "file_id": 1,
    "file_name": "report.pdf",
    "folder_id": 2,
    "owner_id": 3,
    "storage_hash": "550e8400-e29b-41d4-a716-446655440000",
    "size": 1048576
}
```
 */ 
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
