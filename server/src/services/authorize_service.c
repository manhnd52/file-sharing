// File này chứa các hàm kiểm tra cấp quyền truy cập thư mục, file cho user
#include "services/authorize_service.h"
#include "database.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdbool.h>

// Kiểm tra xem user có quyền truy cập folder không
bool authorize_folder_access(int user_id, int folder_id) {
    // TODO: 
}

bool authorize_file_access(int user_id, int file_id) {
    // TODO: 
}
