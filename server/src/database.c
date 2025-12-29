#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <sys/stat.h>

sqlite3* db_global = NULL;

const char* DB_PATH = "data/database/database.db";
const char* DB_SCHEMA_PATH = "data/database/schema.sql";

bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

bool db_start() {
    if (!file_exists(DB_PATH)) {
        // File chưa tồn tại → tạo file và init DB
        FILE* f = fopen(DB_PATH, "a+");
        if (!f) {
            perror("Cannot create database file");
            return false;
        }
        fclose(f);
        // Tạo schema
        db_init(DB_SCHEMA_PATH);
    } 
    if (sqlite3_open(DB_PATH, &db_global) != SQLITE_OK) {
        return false;
    }
    // Enable foreign keys to ensure cascades work
    sqlite3_exec(db_global, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    return true;
}

bool db_init(const char* sql_file) {   
    if (sqlite3_open(DB_PATH, &db_global) != SQLITE_OK) {
        fprintf(stderr, "[DB] Cannot open database: %s\n", sqlite3_errmsg(db_global));
        return false;
    }

    FILE* f = fopen(sql_file, "r");

    if (!f) {
        fprintf(stderr, "[DB] Cannot open SQL file: %s\n", sql_file);
        db_close();
        return false;
    }

    // Tính độ dài file SQL
    fseek(f, 0, SEEK_END); // đưa con trỏ đến cuối file
    long len = ftell(f);    // lấy vị trí hiện tại (độ dài file)
    fseek(f, 0, SEEK_SET); // đưa con trỏ về đầu file

    char* sql = malloc(len + 1);
    if (!sql) {
        fclose(f);
        db_close();
        return false;
    }

    fread(sql, 1, len, f); // đọc toàn bộ file
    sql[len] = '\0';
    fclose(f);

    // Thực thi tất cả lệnh SQL
    char* err_msg = NULL;
    if (sqlite3_exec(db_global, sql, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "[DB] SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        free(sql);
        db_close();
        return false;
    }

    free(sql);
    printf("[DB] Database initialized from file.\n");
    return true;
}

void db_close() {
    if (db_global) {
        sqlite3_close(db_global);
        db_global = NULL;
    }
}
