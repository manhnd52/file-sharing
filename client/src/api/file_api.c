#include "api/file_api.h"

#include "client.h"
#include "cJSON.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int send_list_request(int folder_id, Frame *resp) {
    if (!resp) {
        return -1;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return -1;
    }

    cJSON_AddStringToObject(json, "cmd", "LIST");
    if (folder_id > 0) {
        cJSON_AddNumberToObject(json, "folder_id", folder_id);
    }

    int rc = send_cmd(json, resp);
    cJSON_Delete(json);
    return rc;
}

// LIST: get folder info and its item
int list_api(int folder_id, Frame *resp) {
    return send_list_request(folder_id, resp);
}

int ping_api(Frame *resp) {
    return send_simple_cmd("PING", resp);
}

