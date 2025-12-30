#include "bridge.h"

#include <stdio.h>
#include <string.h>

#include "client.h"
#include "api/auth_api.h"
#include "api/folder_api.h"
#include "api/file_api.h"
#include "api/permission_api.h"
#include "api/upload_api.h"
#include "api/download_api.h"

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

static int handle_respond(RequestResult rc_send, Frame *res, char *out_buf, size_t out_len) {
    if (rc_send != REQ_OK) {
        return rc_send;
    }
    copy_payload(res, out_buf, out_len);
    return (res->msg_type == MSG_RESPOND && res->header.resp.status == STATUS_OK) ? 0 : -1;
}

int fs_login_json(const char *username, const char *password,
                  char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = login_api(username, password, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_register_json(const char *username, const char *password,
                     char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = register_api(username, password, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_auth_json(const char *token, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = auth_api(token, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_logout_json(char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = logout_api(&res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_list_json(int folder_id, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = list_api(folder_id, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_list_shared_items_json(char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = list_shared_items_api(&res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_mkdir_json(int parent_id, const char *name, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = mkdir_api(parent_id, name, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_delete_folder_json(int folder_id, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = delete_folder_api(folder_id, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_delete_file_json(int file_id, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = delete_file_api(file_id, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_share_folder_json(int folder_id, const char *username, int permission,
                         char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = share_folder_api(folder_id, username, permission, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_share_file_json(int file_id, const char *username, int permission,
                       char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = share_file_api(file_id, username, permission, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_rename_folder_json(int folder_id, const char *new_name,
                        char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = rename_folder_api(folder_id, new_name, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_rename_file_json(int file_id, const char *new_name,
                        char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = rename_file_api(file_id, new_name, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_list_folder_permissions_json(int folder_id, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = list_folder_permissions_api(folder_id, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_list_file_permissions_json(int file_id, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = list_file_permissions_api(file_id, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_update_folder_permission_json(int folder_id, const char *username, int permission,
                              char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = update_folder_permission_api(folder_id, username, permission, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_update_file_permission_json(int file_id, const char *username, int permission,
                              char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = update_file_permission_api(file_id, username, permission, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_upload_file_json(const char *file_path, int parent_folder_id, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = upload_file_api(file_path, parent_folder_id, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_download_file_json(const char *dest_dir, int file_id, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = download_file_api(dest_dir, file_id, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}

int fs_download_folder_json(const char *dest_dir, int folder_id, char *out_buf, size_t out_len) {
    Frame res = {0};
    int rc = download_folder_api(dest_dir, folder_id, &res);
    return handle_respond(rc, &res, out_buf, out_len);
}
