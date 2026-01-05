#include "handlers/item_handler.h"
#include "cJSON.h"
#include "database.h"
#include "services/item_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// List permissions for target (folder/file)
void handle_cmd_list_permissions(Conn *c, Frame *f) {
    if (!db_global || c->user_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authenticated\"}");
        send_data(c, resp);
        return;
    }
    cJSON *root = cJSON_Parse((char *)f->payload);
    if (!root) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"invalid_json\"}");
        send_data(c, resp);
        return;
    }
    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(root, "target_type");
    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(root, "target_id");
    if (!cJSON_IsNumber(type_item) || !cJSON_IsNumber(id_item)) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_fields\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }
    int target_type = type_item->valueint;
    int target_id = id_item->valueint;
    cJSON_Delete(root);

    cJSON *perms = list_permissions(target_type, target_id);
    if (!perms) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }
    char *json_resp = cJSON_PrintUnformatted(perms);
    cJSON_Delete(perms);
    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    send_data(c, resp);
    free(json_resp);
}

// Update permission for a target
void handle_cmd_update_permission(Conn *c, Frame *f) {
    if (!db_global || c->user_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authenticated\"}");
        send_data(c, resp);
        return;
    }
    cJSON *root = cJSON_Parse((char *)f->payload);
    if (!root) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"invalid_json\"}");
        send_data(c, resp);
        return;
    }
    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(root, "target_type");
    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(root, "target_id");
    cJSON *user_item = cJSON_GetObjectItemCaseSensitive(root, "username");
    cJSON *perm_item = cJSON_GetObjectItemCaseSensitive(root, "permission");
    if (!cJSON_IsNumber(type_item) || !cJSON_IsNumber(id_item) || !cJSON_IsString(user_item) || !cJSON_IsNumber(perm_item)) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_fields\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }
    int target_type = type_item->valueint;
    int target_id = id_item->valueint;
    
    char username_buf[128];
    memset(username_buf, 0, sizeof(username_buf));
    snprintf(username_buf, sizeof(username_buf), "%s", user_item->valuestring ? user_item->valuestring : "");

    int perm = perm_item->valueint;
    
    cJSON_Delete(root);

    int rc = update_permission(c->user_id, target_type, target_id, username_buf, perm);
    
    if (rc == -1) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_owner_or_not_found\"}");
        send_data(c, resp);
        return;
    }
    if (rc == -2) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"user_not_found_or_invalid\"}");
        send_data(c, resp);
        return;
    }
    if (rc != 0) {
        Frame resp;
        build_respond_frame(&resp, f->header.cmd.request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        return;
    }

    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK,
                        "{\"status\":\"ok\"}");
    send_data(c, resp);
}
