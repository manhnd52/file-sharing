#include <stdio.h>
#include "router.h"
#include "server.h"
#include "handlers/cmd_handler.h"
#include "database.h"

// Khai báo handler
// #include "handlers/user_handler.h"

int main() {
    db_start();
    // Đăng ký route
    // register_route(PT_USER_REGISTER_REQ, handle_user_registration);

    // Khởi động server
    register_cmd_route("LIST", handle_cmd_list);
    register_cmd_route("UPLOAD", handle_cmd_upload);
    register_cmd_route("DOWNLOAD", handle_cmd_download);
    register_cmd_route("PING", handle_cmd_ping);
    register_cmd_route("MKDIR", handle_cmd_mkdir);
    
    server_start(5555);



    return 0;
}
