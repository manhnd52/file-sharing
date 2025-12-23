
#include "client.h"
#include "../protocol/cJSON.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>

// ===== Internal types =====

// Callback high-level: trả về status + JSON string (có thể NULL).
typedef void (*FsApiCallback)(int status, const char *json_resp, void *user_data);

typedef struct {
    uint32_t request_key; // request_id ở dạng network order (htonl)
    FsApiCallback cb;
    void *user_data;
    int in_use;
} FsPendingEntry;

#define FS_MAX_PENDING 1024

// Định nghĩa struct FsClient (khớp với khai báo trong client.h)
struct FsClient {
    Connect *conn; // Kết nối TCP tới server (Connect lo pending/heartbeat)

    pthread_mutex_t lock;
    FsPendingEntry pending[FS_MAX_PENDING];
};

// Giản lược: chỉ một FsClient được dùng cho API high-level.
static FsClient *g_fs_client = NULL;

// Cầu nối từ callback của Connect sang callback high-level.
static void fs_api_resp_bridge(Frame *resp) {
    if (!g_fs_client || !resp) return;

    FsClient *fc = g_fs_client;
    uint32_t resp_key = (uint32_t)htonl(resp->header.resp.request_id);

    FsApiCallback cb = NULL;
    void *user_data = NULL;

    pthread_mutex_lock(&fc->lock);
    for (int i = 0; i < FS_MAX_PENDING; ++i) {
        if (fc->pending[i].in_use && fc->pending[i].request_key == resp_key) {
            cb = fc->pending[i].cb;
            user_data = fc->pending[i].user_data;
            fc->pending[i].in_use = 0;
            fc->pending[i].cb = NULL;
            fc->pending[i].user_data = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&fc->lock);

    if (!cb) return;

    int status = STATUS_NOT_OK;
    if (resp->msg_type == MSG_RESPOND) {
        status = resp->header.resp.status;
    }

    char *json_copy = NULL;
    if (resp->payload_len > 0) {
        json_copy = (char *)malloc(resp->payload_len + 1);
        if (json_copy) {
            memcpy(json_copy, resp->payload, resp->payload_len);
            json_copy[resp->payload_len] = '\0';
        }
    }

    cb(status, json_copy, user_data);

    if (json_copy) {
        free(json_copy);
    }
}

// Tạo client và kết nối tới server.
FsClient *fs_client_create(const char *host, uint16_t port) {
    FsClient *fc = (FsClient *)calloc(1, sizeof(struct FsClient));
    if (!fc) return NULL;

    fc->conn = connect_create(host, port);
    if (!fc->conn) {
        free(fc);
        return NULL;
    }

    pthread_mutex_init(&fc->lock, NULL);
    memset(fc->pending, 0, sizeof(fc->pending));

    // Gán làm client mặc định cho API high-level.
    g_fs_client = fc;
    return fc;
}

// Hủy client, đóng kết nối.
void fs_client_destroy(FsClient *fc) {
    if (!fc) return;
    if (fc->conn) {
        connect_destroy(fc->conn);
        fc->conn = NULL;
    }
    pthread_mutex_destroy(&fc->lock);
    if (g_fs_client == fc) {
        g_fs_client = NULL;
    }
    free(fc);
}

// Gửi frame AUTH bất đồng bộ.
// token: AUTH_TOKEN_SIZE bytes.
// json_payload: JSON bổ sung (có thể NULL).
// cb: callback được gọi khi nhận RESPOND; chạy trong thread reader của Connect.
int fs_client_send_auth(FsClient *fc,
                        const uint8_t token[AUTH_TOKEN_SIZE],
                        const char *json_payload,
                        resp_callback cb) {
    if (!fc || !fc->conn || !token) return -1;

    Frame f;
    if (build_auth_frame(&f, 0, token, json_payload) != 0) {
        return -1;
    }
    return fc->conn->send_auth(fc->conn, &f, cb);
}

// Gửi CMD bất đồng bộ với payload JSON.
// json_payload: chuỗi JSON (không NULL).
// cb: callback được gọi khi nhận RESPOND.
int fs_client_send_cmd(FsClient *fc,
                       const char *json_payload,
                       resp_callback cb,
                       uint32_t *out_request_id) {
    if (!fc || !fc->conn || !json_payload || !cb) return -1;

    Frame f;
    if (build_cmd_frame(&f, 0, json_payload) != 0) {
        return -1;
    }
    int rc = fc->conn->send_cmd(fc->conn, &f, cb);
    if (rc != 0) {
        return rc;
    }

    if (out_request_id) {
        uint32_t key = (uint32_t)get_request_id(&f);
        *out_request_id = key;
    }
    return 0;
}

// Gửi DATA frame bất đồng bộ (upload/download chunk).
int fs_client_send_data(FsClient *fc,
                        const uint8_t session_id[SESSIONID_SIZE],
                        uint32_t chunk_index,
                        uint32_t chunk_length,
                        const uint8_t *data,
                        resp_callback cb) {
    if (!fc || !fc->conn || !session_id) return -1;

    Frame f;
    if (build_data_frame(&f, 0, session_id, chunk_index, chunk_length, data) != 0) {
        return -1;
    }
    return fc->conn->send_data(fc->conn, &f, cb);
}

// ===== High-level JSON API (async, multi-pending via request_id) =====

// Helper: gửi CMD với payload JSON (string) và callback high-level.
static int fs_send_json_cmd(FsClient *fc,
                            const char *json_payload,
                            FsApiCallback cb,
                            void *user_data) {
    if (!fc || !json_payload || !cb) return -1;
    if (!fc->conn) return -1;

    g_fs_client = fc;

    uint32_t request_key = 0;
    int rc = fs_client_send_cmd(fc, json_payload, fs_api_resp_bridge, &request_key);
    if (rc != 0) {
        return -1;
    }

    // Đăng ký callback vào bảng pending
    pthread_mutex_lock(&fc->lock);
    int inserted = 0;
    for (int i = 0; i < FS_MAX_PENDING; ++i) {
        if (!fc->pending[i].in_use) {
            fc->pending[i].in_use = 1;
            fc->pending[i].request_key = request_key;
            fc->pending[i].cb = cb;
            fc->pending[i].user_data = user_data;
            inserted = 1;
            break;
        }
    }
    pthread_mutex_unlock(&fc->lock);

    if (!inserted) {
        // Không tìm được slot lưu callback; coi như lỗi high-level.
        return -1;
    }
    return 0;
}

// REGISTER
int fs_api_register(FsClient *fc,
                    const char *username,
                    const char *password,
                    FsApiCallback cb,
                    void *user_data) {
    if (!username || !password || !cb) return -1;

    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "cmd", "REGISTER");
    cJSON_AddStringToObject(root, "username", username);
    cJSON_AddStringToObject(root, "password", password);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return -1;

    int rc = fs_send_json_cmd(fc, payload, cb, user_data);
    free(payload);
    return rc;
}

// LOGIN
int fs_api_login(FsClient *fc,
                 const char *username,
                 const char *password,
                 FsApiCallback cb,
                 void *user_data) {
    if (!username || !password || !cb) return -1;

    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "cmd", "LOGIN");
    cJSON_AddStringToObject(root, "username", username);
    cJSON_AddStringToObject(root, "password", password);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return -1;

    int rc = fs_send_json_cmd(fc, payload, cb, user_data);
    free(payload);
    return rc;
}

// LIST folder (folder_id == 0 => root user)
int fs_api_list(FsClient *fc,
                int folder_id,
                FsApiCallback cb,
                void *user_data) {
    if (!cb) return -1;

    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "cmd", "LIST");
    if (folder_id > 0) {
        cJSON_AddNumberToObject(root, "folder_id", folder_id);
    }

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return -1;

    int rc = fs_send_json_cmd(fc, payload, cb, user_data);
    free(payload);
    return rc;
}

// MKDIR
int fs_api_mkdir(FsClient *fc,
                 int parent_id,
                 const char *name,
                 FsApiCallback cb,
                 void *user_data) {
    if (!name || !*name || !cb) return -1;

    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "cmd", "MKDIR");
    cJSON_AddNumberToObject(root, "parent_id", parent_id);
    cJSON_AddStringToObject(root, "name", name);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return -1;

    int rc = fs_send_json_cmd(fc, payload, cb, user_data);
    free(payload);
    return rc;
}

// UPLOAD_INIT: khởi tạo phiên upload, server trả về sessionId.
int fs_api_upload_init(FsClient *fc,
                       const char *path,
                       uint64_t file_size,
                       uint32_t chunk_size,
                       FsApiCallback cb,
                       void *user_data) {
    if (!path || !*path || !cb) return -1;

    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "cmd", "UPLOAD_INIT");
    cJSON_AddStringToObject(root, "path", path);
    cJSON_AddNumberToObject(root, "file_size", (double)file_size);
    cJSON_AddNumberToObject(root, "chunk_size", (double)chunk_size);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return -1;

    int rc = fs_send_json_cmd(fc, payload, cb, user_data);
    free(payload);
    return rc;
}

// DOWNLOAD: hiện server mới trả về metadata giả định; client chỉ cần path.
int fs_api_download(FsClient *fc,
                    const char *path,
                    FsApiCallback cb,
                    void *user_data) {
    if (!path || !*path || !cb) return -1;

    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "cmd", "DOWNLOAD");
    cJSON_AddStringToObject(root, "path", path);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return -1;

    int rc = fs_send_json_cmd(fc, payload, cb, user_data);
    free(payload);
    return rc;
}

// DELETE_FOLDER
int fs_api_delete_folder(FsClient *fc,
                         int folder_id,
                         FsApiCallback cb,
                         void *user_data) {
    if (folder_id <= 0 || !cb) return -1;

    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "cmd", "DELETE_FOLDER");
    cJSON_AddNumberToObject(root, "folder_id", folder_id);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return -1;

    int rc = fs_send_json_cmd(fc, payload, cb, user_data);
    free(payload);
    return rc;
}

// SHARE_FOLDER
int fs_api_share_folder(FsClient *fc,
                        int folder_id,
                        const char *username,
                        int permission,
                        FsApiCallback cb,
                        void *user_data) {
    if (folder_id <= 0 || !username || !*username || !cb) return -1;

    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "cmd", "SHARE_FOLDER");
    cJSON_AddNumberToObject(root, "folder_id", folder_id);
    cJSON_AddStringToObject(root, "username", username);
    cJSON_AddNumberToObject(root, "permission", permission);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return -1;

    int rc = fs_send_json_cmd(fc, payload, cb, user_data);
    free(payload);
    return rc;
}

// RENAME_ITEM (folder/file)
int fs_api_rename_item(FsClient *fc,
                       int item_id,
                       const char *item_type,
                       const char *new_name,
                       FsApiCallback cb,
                       void *user_data) {
    if (item_id <= 0 || !item_type || !*item_type || !new_name || !*new_name || !cb)
        return -1;

    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;
    cJSON_AddStringToObject(root, "cmd", "RENAME_ITEM");
    cJSON_AddNumberToObject(root, "item_id", item_id);
    cJSON_AddStringToObject(root, "item_type", item_type);
    cJSON_AddStringToObject(root, "new_name", new_name);

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) return -1;

    int rc = fs_send_json_cmd(fc, payload, cb, user_data);
    free(payload);
    return rc;
}
