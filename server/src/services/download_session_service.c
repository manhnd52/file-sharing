#include "services/download_session_service.h"
#include "database.h"
#include <sqlite3.h>
#include <string.h>

static int bind_session_id(sqlite3_stmt *stmt, int index, const uint8_t session_id[BYTE_UUID_SIZE]) {
    return sqlite3_bind_blob(stmt, index, session_id, BYTE_UUID_SIZE, SQLITE_STATIC);
}

int ds_create(
    const uint8_t session_id[16],
    int chunk_size,
    uint64_t total_file_size,
    int file_id,
    const char *file_hashcode
) {
    if (!db_global || !session_id || chunk_size <= 0 ||
        file_id <= 0 || !file_hashcode || file_hashcode[0] == '\0') {
        return 0;
    }

    const char *sql =
        "INSERT INTO download_sessions(session_id, state, chunk_size, total_file_size, file_id, file_hashcode) "
        "VALUES(?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    if (bind_session_id(stmt, 1, session_id) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 2, DOWNLOAD_INIT) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 3, chunk_size) != SQLITE_OK ||
        sqlite3_bind_int64(stmt, 4, (sqlite3_int64)total_file_size) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 5, file_id) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 6, file_hashcode, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return 0;
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 1 : 0;
}

int ds_get(
    const uint8_t session_id[16],
    DownloadSession *out
) {
    if (!db_global || !session_id || !out) {
        return 0;
    }

    const char *sql =
        "SELECT last_requested_chunk, chunk_size, total_file_size, file_id, file_hashcode, state "
        "FROM download_sessions WHERE session_id = ?";
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

    out->last_requested_chunk = (uint32_t)sqlite3_column_int(stmt, 0);
    out->chunk_size = sqlite3_column_int(stmt, 1);
    out->total_file_size = (uint64_t)sqlite3_column_int64(stmt, 2);
    out->file_id = sqlite3_column_int(stmt, 3);
    const unsigned char *hash = sqlite3_column_text(stmt, 4);
    out->state = (DownloadState)sqlite3_column_int(stmt, 5);
    if (hash) {
        strncpy(out->file_hashcode, (const char *)hash, sizeof(out->file_hashcode) - 1);
    } else {
        out->file_hashcode[0] = '\0';
    }
    out->file_hashcode[sizeof(out->file_hashcode) - 1] = '\0';
    memcpy(out->session_id, session_id, BYTE_UUID_SIZE);

    sqlite3_finalize(stmt);
    return 1;
}

int ds_update_state(
    const uint8_t session_id[16],
    DownloadState new_state
) {
    if (!db_global || !session_id) {
        return 0;
    }

    const char *sql =
        "UPDATE download_sessions SET state = ? WHERE session_id = ?";
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
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE && sqlite3_changes(db_global) > 0) ? 1 : 0;
}

int ds_update_progress(
    const uint8_t session_id[16],
    uint32_t last_requested_chunk
) {
    if (!db_global || !session_id) {
        return 0;
    }

    const char *sql =
        "UPDATE download_sessions SET last_requested_chunk = ? WHERE session_id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    if (sqlite3_bind_int(stmt, 1, (int)last_requested_chunk) != SQLITE_OK ||
        bind_session_id(stmt, 2, session_id) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return 0;
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE && sqlite3_changes(db_global) > 0) ? 1 : 0;
}

int ds_delete(
    const uint8_t session_id[16]
) {
    if (!db_global || !session_id) {
        return 0;
    }

    const char *sql =
        "DELETE FROM download_sessions WHERE session_id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }

    if (bind_session_id(stmt, 1, session_id) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return 0;
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE && sqlite3_changes(db_global) > 0) ? 1 : 0;
}
