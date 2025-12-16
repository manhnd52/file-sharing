// user_service.c
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>

#include "database.h"
#include "services/user_service.h"
#include "models/user.h"

bool prepare_statement(sqlite3_stmt** stmt, const char* sql) {
    if (sqlite3_prepare_v2(db_global, sql, -1, stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Failed to prepare statement: %s\n", sqlite3_errmsg(db_global));
        return false;
    }
    return true;
}

int user_create(const char* username, const char* password) {
    if (!db_global) return 0;

    int user_id = 0;
    sqlite3_stmt* stmt; // Biến này dùng để chuẩn bị và thực thi câu lệnh SQL

    // 1. Thêm user nếu chưa có
    const char* sql_insert = "INSERT OR IGNORE INTO users(username, password) VALUES(?, ?);";
    prepare_statement(&stmt, sql_insert);

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "[DB] Failed to execute insert: %s\n", sqlite3_errmsg(db_global));
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);

    // 2. Lấy id của user vừa tạo hoặc đã tồn tại
    const char* sql_select = "SELECT id FROM users WHERE username = ?;";
    prepare_statement(&stmt, sql_select);
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
    } else {
        fprintf(stderr, "[DB] User not found after insert\n");
    }

    sqlite3_finalize(stmt);
    return user_id;
}


User get_user_by_id(int user_id) {
    User user = {0};
    if (!db_global) return user;

    const char* sql = "SELECT id, username, password FROM users WHERE id = ?";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) return user;

    sqlite3_bind_int(stmt, 1, user_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user.id = sqlite3_column_int(stmt, 0);
        const unsigned char* username_text = sqlite3_column_text(stmt, 1);
        const unsigned char* password_text = sqlite3_column_text(stmt, 2);
        if (username_text) {
            strncpy(user.username, (const char*)username_text, sizeof(user.username) - 1);
        }
        if (password_text) {
            strncpy(user.password, (const char*)password_text, sizeof(user.password) - 1);
        }
    }

    sqlite3_finalize(stmt);
    return user;
}

int user_verify_credentials(const char* username, const char* password) {
    if (!db_global || !username || !password) return 0;

    const char* sql = "SELECT id FROM users WHERE username = ? AND password = ?";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[DB] Failed to prepare verify statement: %s\n", sqlite3_errmsg(db_global));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);

    int user_id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        user_id = sqlite3_column_int(stmt, 0);
        printf("[AUTH] User '%s' authenticated successfully, user_id=%d\n", username, user_id);
    } else {
        printf("[AUTH] Authentication failed for user '%s'\n", username);
    }

    sqlite3_finalize(stmt);
    return user_id;
}

// Generate random token and store in sessions table
char* user_create_session_token(int user_id, int expiry_hours) {
    if (!db_global || user_id <= 0) return NULL;

    // Generate random token (32 hex chars = 16 bytes random)
    unsigned char random_bytes[16];
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[TOKEN] Failed to open /dev/urandom\n");
        return NULL;
    }
    if (read(fd, random_bytes, 16) != 16) {
        close(fd);
        fprintf(stderr, "[TOKEN] Failed to read random bytes\n");
        return NULL;
    }
    close(fd);

    // Convert to hex string
    char* token = malloc(65); // 64 hex chars + null
    if (!token) return NULL;
    
    for (int i = 0; i < 16; i++) {
        sprintf(token + i * 4, "%04x", (unsigned int)random_bytes[i]);
    }
    token[64] = '\0';

    // Insert into sessions table
    const char* sql = "INSERT INTO sessions (user_id, token, expires_at) "
                     "VALUES (?, ?, datetime('now', '+' || ? || ' hours'))";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[TOKEN] Failed to prepare insert: %s\n", sqlite3_errmsg(db_global));
        free(token);
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, token, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, expiry_hours);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "[TOKEN] Failed to insert session: %s\n", sqlite3_errmsg(db_global));
        sqlite3_finalize(stmt);
        free(token);
        return NULL;
    }

    sqlite3_finalize(stmt);
    printf("[TOKEN][INFO] Created session token for user_id=%d, expires in %d hours\n", 
           user_id, expiry_hours);
    return token;
}

// Verify token and return user_id if valid and not expired
bool user_verify_token(const char* token, int* user_id_out) {
    if (!db_global || !token || !user_id_out) return false;

    const char* sql = "SELECT user_id FROM sessions "
                     "WHERE token = ? AND expires_at > datetime('now')";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_global, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[TOKEN] Failed to prepare verify: %s\n", sqlite3_errmsg(db_global));
        return false;
    }

    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_STATIC);

    bool valid = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *user_id_out = sqlite3_column_int(stmt, 0);
        valid = true;
        printf("[TOKEN][INFO] Token verified for user_id=%d\n", *user_id_out);
    } else {
        printf("[TOKEN][WARN] Invalid or expired token\n");
    }

    sqlite3_finalize(stmt);
    return valid;
}