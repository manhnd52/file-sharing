#include <stdio.h>
#include "router.h"
#include "server.h"
#include "handlers/folder_handler.h"
#include "handlers/upload_handler.h"
#include "handlers/auth_handler.h"
#include "handlers/ping_handler.h"
#include "database.h"
#include "services/file_service.h"
#include "handlers/download_handler.h"
#include <stdlib.h>

int main() {
    // Khởi tạo DB
    if (!db_start()) {
        fprintf(stderr, "Failed to start database\n");
        return 1;
    }
    
    // Register CMD routes
    register_cmd_route("LOGIN", handle_login);
    register_cmd_route("REGISTER", handle_register);
    register_cmd_route("AUTH", handle_auth_token);
    register_cmd_route("LOGOUT", handle_logout);
    register_cmd_route("GET_ME", handle_get_me);

    register_cmd_route("PING", handle_cmd_ping);

    register_cmd_route("LIST", handle_cmd_list);
    register_cmd_route("MKDIR", handle_cmd_mkdir);
    register_cmd_route("DELETE_FOLDER", handle_cmd_delete_folder);
    register_cmd_route("DELETE_FILE", handle_cmd_delete_file);
    register_cmd_route("SHARE_FOLDER", handle_cmd_share_folder);
    register_cmd_route("SHARE_FILE", handle_cmd_share_file);
    register_cmd_route("LIST_PERMISSIONS", handle_cmd_list_permissions);
    register_cmd_route("UPDATE_PERMISSION", handle_cmd_update_permission);
    register_cmd_route("RENAME_ITEM", handle_cmd_rename_item);

    register_cmd_route("LIST_OWN_FOLDERS", handle_cmd_list_own_folders);
    register_cmd_route("LIST_SHARED_FOLDERS", handle_cmd_list_shared_folders);

    register_cmd_route("UPLOAD_INIT", upload_init_handler);
    register_cmd_route("UPLOAD_FINISH", upload_finish_handler);

    register_cmd_route("DOWNLOAD_INIT", download_init_handler);
    register_cmd_route("DOWNLOAD_CHUNK", download_chunk_handler);
    register_cmd_route("DOWNLOAD_FINISH", download_finish_handler);

    char *out = cJSON_Print(get_file_info(6)); // Test hàm mới thêm
    printf("Test get_file_info output: %s\n", out);
    free(out);
    server_start(5555);

    return 0;
}
