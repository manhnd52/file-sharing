#include <stdio.h>
#include "router.h"
#include "server.h"
#include "handlers/cmd_handler.h"
#include "handlers/upload_handler.h"
#include "handlers/auth_handler.h"
#include "database.h"

int main() {
    db_start();
    
    // Register AUTH handler
    
    // Register CMD routes
    register_cmd_route("LIST", handle_cmd_list);
    register_cmd_route("DOWNLOAD", handle_cmd_download);
    register_cmd_route("PING", handle_cmd_ping);
    register_cmd_route("MKDIR", handle_cmd_mkdir);
    register_cmd_route("UPLOAD_INIT", upload_init_handler);
    register_cmd_route("LOGIN", handle_login);
    register_cmd_route("REGISTER", handle_register);
    register_cmd_route("AUTH", handle_auth_token);
    register_cmd_route("LOGOUT", handle_logout);
    register_cmd_route("GET_ME", handle_get_me);
    
    server_start(5555);
    return 0;
}
