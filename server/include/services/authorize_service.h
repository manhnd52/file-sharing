#ifndef AUTHORIZE_SERVICE_H
#define AUTHORIZE_SERVICE_H
#include <stdbool.h>

// Kiểm tra xem user có quyền truy cập folder không
bool authorize_folder_access(int user_id, int folder_id);

#endif // AUTHORIZE_SERVICE_H