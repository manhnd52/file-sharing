#include <stdio.h>
#include "router.h"
#include "server.h"

#include "database.h"

// Khai báo handler
// #include "handlers/user_handler.h"

int main() {
    db_start();
    // Đăng ký route
    // register_route(PT_USER_REGISTER_REQ, handle_user_registration);

    // Khởi động server
    server_start(5555);

    return 0;
}
