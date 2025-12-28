#include "bridge.h"

#include <stdio.h>
#include <string.h>

#include "client.h"
#include "api/auth_api.h"
#include "api/file_api.h"

static int copy_payload(Frame *res, char *out_buf, size_t out_len) {
    if (!out_buf || out_len == 0) return -1;
    out_buf[0] = '\0';
    if (!res || res->payload_len == 0) return 0;
    size_t n = res->payload_len;
    if (n >= out_len) n = out_len - 1;
    memcpy(out_buf, res->payload, n);
    out_buf[n] = '\0';
    return 0;
}

int fs_connect(const char *host, uint16_t port, int timeout_seconds) {
    return client_connect(host, port, timeout_seconds);
}

void fs_disconnect(void) {
    client_disconnect();
}

static int handle_cmd_frame(int rc_send, Frame *res, char *out_buf, size_t out_len) {
    if (rc_send != 0) {
        return rc_send;
    }
    copy_payload(res, out_buf, out_len);
    return (res->msg_type == MSG_RESPOND && res->header.resp.status == STATUS_OK) ? 0 : -1;
}

int fs_login_json(const char *username, const char *password,
                  char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = login_api(username, password, &res);
    return handle_cmd_frame(rc, &res, out_buf, out_len);
}

int fs_register_json(const char *username, const char *password,
                     char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = register_api(username, password, &res);
    return handle_cmd_frame(rc, &res, out_buf, out_len);
}

int fs_list_json(int folder_id, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = list_api(folder_id, &res);
    return handle_cmd_frame(rc, &res, out_buf, out_len);
}

int fs_list_shared_folders_json(char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = list_shared_folders_api(&res);
    return handle_cmd_frame(rc, &res, out_buf, out_len);
}

int fs_mkdir_json(int parent_id, const char *name, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = mkdir_api(parent_id, name, &res);
    return handle_cmd_frame(rc, &res, out_buf, out_len);
}

int fs_delete_folder_json(int folder_id, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = delete_folder_api(folder_id, &res);
    return handle_cmd_frame(rc, &res, out_buf, out_len);
}

int fs_delete_file_json(int file_id, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = delete_file_api(file_id, &res);
    return handle_cmd_frame(rc, &res, out_buf, out_len);
}

int fs_share_folder_json(int folder_id, const char *username, int permission,
                         char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = share_folder_api(folder_id, username, permission, &res);
    return handle_cmd_frame(rc, &res, out_buf, out_len);
}

int fs_share_file_json(int file_id, const char *username, int permission,
                       char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = share_file_api(file_id, username, permission, &res);
    return handle_cmd_frame(rc, &res, out_buf, out_len);
}

int fs_rename_item_json(int item_id, const char *item_type, const char *new_name,
                        char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = rename_item_api(item_id, item_type, new_name, &res);
    return handle_cmd_frame(rc, &res, out_buf, out_len);
}

int fs_list_permissions_json(int target_type, int target_id, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = list_permissions_api(target_type, target_id, &res);
    return handle_cmd_frame(rc, &res, out_buf, out_len);
}

int fs_update_permission_json(int target_type, int target_id, const char *username, int permission,
                              char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = update_permission_api(target_type, target_id, username, permission, &res);
    return handle_cmd_frame(rc, &res, out_buf, out_len);
}
