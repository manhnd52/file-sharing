#include "services/upload_session_service.h"
#include "database.h"
#include <sqlite3.h>
#include <string.h>

static int bind_session_id(sqlite3_stmt *stmt, int index, const uint8_t session_id[BYTE_UUID_SIZE]) {
    return sqlite3_bind_blob(stmt, index, session_id, BYTE_UUID_SIZE, SQLITE_STATIC);
}

int us_create(
    const uint8_t session_id[16],
    const char *uuid_str,
    uint32_t chunk_size,
    uint64_t expected_file_size,
    int parent_folder_id,
    const char *file_name
) {
    if (!db_global || !session_id || !uuid_str || uuid_str[0] == '\0' ||
        chunk_size == 0 || !file_name || file_name[0] == '\0') {
        return 0;
    }

    const char *sql =
        "INSERT INTO upload_sessions(session_id, uuid_str, chunk_size, expected_file_size, parent_folder_id, file_name) "
        "VALUES(?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    if (bind_session_id(stmt, 1, session_id) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, uuid_str, -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 3, (int)chunk_size) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 4, (sqlite3_int64)expected_file_size) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 5, parent_folder_id) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 6, file_name, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return 0;
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 1 : 0;
}

int us_get(
    const uint8_t session_id[16],
    UploadSession *out
) {
    if (!db_global || !session_id || !out) {
        return 0;
    }

    const char *sql =
        "SELECT uuid_str, last_received_chunk, chunk_size, total_received_size, expected_file_size, parent_folder_id, file_name "
        "FROM upload_sessions WHERE session_id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    if (bind_session_id(stmt, 1, session_id) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return 0;
    }

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return 0;
    }

    const unsigned char *uuid = sqlite3_column_text(stmt, 0);
    if (uuid) {
        strncpy(out->uuid_str, (const char *)uuid, sizeof(out->uuid_str) - 1);
    } else {
        out->uuid_str[0] = '\0';
    }
    out->uuid_str[sizeof(out->uuid_str) - 1] = '\0';

    out->last_received_chunk = (uint32_t)sqlite3_column_int(stmt, 1);
    out->chunk_size = (uint32_t)sqlite3_column_int(stmt, 2);
    out->total_received_size = (uint64_t)sqlite3_column_int64(stmt, 3);
    out->expected_file_size = (uint64_t)sqlite3_column_int64(stmt, 4);
    out->parent_folder_id = sqlite3_column_int(stmt, 5);

    const unsigned char *name = sqlite3_column_text(stmt, 6);
    if (name) {
        strncpy(out->file_name, (const char *)name, sizeof(out->file_name) - 1);
    } else {
        out->file_name[0] = '\0';
    }
    out->file_name[sizeof(out->file_name) - 1] = '\0';

    memcpy(out->session_id, session_id, BYTE_UUID_SIZE);
    sqlite3_finalize(stmt);
    return 1;
}

int us_update_state(
    const uint8_t session_id[16],
    UploadState new_state
) {
    if (!db_global || !session_id) {
        return 0;
    }

    const char *sql =
        "UPDATE upload_sessions SET state = ?, updated_at = (unixepoch()) WHERE session_id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    if (sqlite3_bind_int(stmt, 1, new_state) != SQLITE_OK ||
        bind_session_id(stmt, 2, session_id) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return 0;
    }

    int rc = sqlite3_step(stmt);
    int changed = sqlite3_changes(db_global);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE && changed > 0) ? 1 : 0;
}

int us_update_progress(
    const uint8_t session_id[16],
    uint32_t last_received_chunk,
    uint64_t total_received_size
) {
    if (!db_global || !session_id) {
        return 0;
    }

    const char *sql =
        "UPDATE upload_sessions SET last_received_chunk = ?, total_received_size = ?, updated_at = (unixepoch()) "
        "WHERE session_id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    if (sqlite3_bind_int(stmt, 1, (int)last_received_chunk) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)total_received_size) != SQLITE_OK ||
        bind_session_id(stmt, 3, session_id) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return 0;
    }

    int rc = sqlite3_step(stmt);
    int changed = sqlite3_changes(db_global);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE && changed > 0) ? 1 : 0;
}

int us_delete(
    const uint8_t session_id[16]
) {
    if (!db_global || !session_id) {
        return 0;
    }

    const char *sql = "DELETE FROM upload_sessions WHERE session_id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    if (bind_session_id(stmt, 1, session_id) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return 0;
    }

    int rc = sqlite3_step(stmt);
    int changed = sqlite3_changes(db_global);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE && changed > 0) ? 1 : 0;
}
