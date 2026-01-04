#include "cJSON.h"
#include <unistd.h>

#include "api/download_api.h"
#include "api/upload_api.h"
#include "client.h"
#include "utils/cache_util.h"

static int send_session_cancel_cmd(const char *cmd, const char *session_id, Frame *res) {
    if (!cmd || cmd[0] == '\0' || !session_id || session_id[0] == '\0' || !res) {
        return -1;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return -1;
    }

    cJSON_AddStringToObject(json, "cmd", cmd);
    cJSON_AddStringToObject(json, "session_id", session_id);

    int rc = send_cmd(json, res);
    cJSON_Delete(json);
    return rc;
}

int download_cancel_api(const char* session_id, Frame* res) {
    cache_set_downloading_transfer_state(CACHE_TRANSFER_CANCELLED);
    int rc = send_session_cancel_cmd("DOWNLOAD_CANCEL", session_id, res);
    if (rc != 0) {
        return rc;
    }
    CacheState cache;
    rc = cache_load_default(&cache);
    rc = unlink(cache.downloading.storage_path);
    return rc;
}

int upload_cancel_api(const char* session_id, Frame* res) {
    cache_set_uploading_transfer_state(CACHE_TRANSFER_CANCELLED);
    return send_session_cancel_cmd("UPLOAD_CANCEL", session_id, res);
}
