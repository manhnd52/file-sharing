#include "api/file_api.h"

#include "../../protocol/connect.h"
#include "../../protocol/frame.h"
#include "cJSON.h"

#include <stdlib.h>

static int build_and_send(const char *cmd, Connect *conn, cJSON *json, Frame *resp) {
    if (!cmd || !conn || !resp) {
        return -1;
    }
    if (!json) {
        json = cJSON_CreateObject();
        if (!json) {
            return -1;
        }
    }
    cJSON_AddStringToObject(json, "cmd", cmd);
    char *payload = cJSON_PrintUnformatted(json);
    if (!payload) {
        cJSON_Delete(json);
        return -1;
    }

    Frame request = {0};
    int rc = -1;
    if (build_cmd_frame(&request, 0, payload) == 0) {
        rc = connect_send_request(conn, &request, resp);
    }

    free(payload);
    cJSON_Delete(json);
    return rc;
}

int file_api_list(Connect *conn, const char *path, Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return -1;
    }
    cJSON_AddStringToObject(json, "path", path && path[0] ? path : "/");
    return build_and_send("LIST", conn, json, resp);
}

int file_api_me(Connect *conn, Frame *resp) {
    return build_and_send("GET_ME", conn, NULL, resp);
}

int file_api_ping(Connect *conn, Frame *resp) {
    return build_and_send("PING", conn, NULL, resp);
}
