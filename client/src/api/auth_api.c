#include "api/auth_api.h"

#include "connect.h"
#include "frame.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h" // for g_conn, send_cmd

int login_api(const char *username, const char *password, Frame *resp) {
    
    if (!username || !password) {
        return -1;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return -1;
    }

    cJSON_AddStringToObject(json, "cmd", "LOGIN");
    cJSON_AddStringToObject(json, "username", username);
    cJSON_AddStringToObject(json, "password", password);

    int rc = send_cmd(json, resp);

    cJSON_Delete(json);
    return rc;
}

int register_api(const char *username, const char *password, Frame *resp) {
    if (!username || !password) {
        return -1;
    }
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return -1;
    }
    cJSON_AddStringToObject(json, "cmd", "REGISTER");
    cJSON_AddStringToObject(json, "username", username);
    cJSON_AddStringToObject(json, "password", password);

    int rc = send_cmd(json, resp);

    cJSON_Delete(json);
    return rc;
}

int auth_api(const char *token, Frame *resp) {
    if (!token || token[0] == '\0') {
        return -1;
    }
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return -1;
    }
    cJSON_AddStringToObject(json, "cmd", "AUTH");
    cJSON_AddStringToObject(json, "token", token);
    int rc = send_cmd(json, resp);
    cJSON_Delete(json);
    return rc;
}

int logout_api(Frame *resp) {
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return -1;
    }
    cJSON_AddStringToObject(json, "cmd", "LOGOUT");
    int rc = send_cmd(json, resp);
    cJSON_Delete(json);
    return rc;
}

int me_api(Frame *resp) {
    return send_simple_cmd("GET_ME", resp);
}

