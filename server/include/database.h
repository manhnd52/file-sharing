#ifndef DB_H
#define DB_H

#include <stdbool.h>
#include <sqlite3.h>

extern sqlite3* db_global;

// Khởi tạo DB từ file SQL
bool db_start();
bool db_init(const char* sql_file);

// Đóng DB
void db_close();

#endif
