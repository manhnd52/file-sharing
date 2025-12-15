// user_service.c
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

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