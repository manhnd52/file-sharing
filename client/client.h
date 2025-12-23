#ifndef FS_CLIENT_H
#define FS_CLIENT_H

#include <stdint.h>
#include "../protocol/frame.h"
#include "../protocol/connect.h"

// Opaque client handle
typedef struct FsClient FsClient;

// Tạo và hủy client (quản lý Connect bên trong).
FsClient *fs_client_create(const char *host, uint16_t port);
void fs_client_destroy(FsClient *fc);

// ===== Low-level async API (wrap Connect) =====

// API gửi frame bất đồng bộ, bọc quanh Connect. Callback là resp_callback
// từ Connect (chỉ nhận Frame*).
int fs_client_send_auth(FsClient *fc,
                        const uint8_t token[AUTH_TOKEN_SIZE],
                        const char *json_payload,
                        resp_callback cb);

int fs_client_send_cmd(FsClient *fc,
                       const char *json_payload,
                       resp_callback cb,
                       uint32_t *out_request_id);

int fs_client_send_data(FsClient *fc,
                        const uint8_t session_id[SESSIONID_SIZE],
                        uint32_t chunk_index,
                        uint32_t chunk_length,
                        const uint8_t *data,
                        resp_callback cb);

// ===== High-level JSON API (async) =====
// Callback high-level: status + JSON string (có thể NULL). Người gọi
// chỉ đọc json_resp trong callback, không free.
typedef void (*FsApiCallback)(int status, const char *json_resp, void *user_data);

int fs_api_register(FsClient *fc,
                    const char *username,
                    const char *password,
                    FsApiCallback cb,
                    void *user_data);

int fs_api_login(FsClient *fc,
                 const char *username,
                 const char *password,
                 FsApiCallback cb,
                 void *user_data);

int fs_api_list(FsClient *fc,
                int folder_id,
                FsApiCallback cb,
                void *user_data);

int fs_api_mkdir(FsClient *fc,
                 int parent_id,
                 const char *name,
                 FsApiCallback cb,
                 void *user_data);

int fs_api_upload_init(FsClient *fc,
                       const char *path,
                       uint64_t file_size,
                       uint32_t chunk_size,
                       FsApiCallback cb,
                       void *user_data);

int fs_api_download(FsClient *fc,
                    const char *path,
                    FsApiCallback cb,
                    void *user_data);

#endif // FS_CLIENT_H
