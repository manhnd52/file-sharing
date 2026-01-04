#include "client.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>

Connect *g_conn = NULL;

int client_connect(const char *host, uint16_t port, int timeout_seconds) {
    if (g_conn) {
        return -1;
    }
    Connect *conn = connect_create(host, port, timeout_seconds);
    if (!conn) {
        return -1;
    }
    g_conn = conn;
    printf("[CLIENT] Connected to server: %s:%d", host, port);
    return 0;
}

void client_disconnect(void) {
    if (!g_conn) {
        return;
    }
    connect_destroy(g_conn);
    g_conn = NULL;
}

bool client_is_connected(void) {
    return g_conn != NULL;
}

int send_cmd(cJSON *json, Frame *res) {
    if (!json || !res || !g_conn) {
        return REQ_ERROR;
    }

    char *payload = cJSON_PrintUnformatted(json);
    if (!payload) {
        return REQ_ERROR;
    }

    Frame request = {0};
    int rc = build_cmd_frame(&request, 0, payload);
    printf("[DEBUG] Sending command: %s\n", payload);
    print_frame(&request);
    puts("");
    
    if (rc == 0) {
        rc = connect_send_request(g_conn, &request, res);
    }
    printf("[DEBUG] Received response with rc = %d:\n", rc);
    print_frame(res);
    puts("");
    free(payload);

    return rc;
}

int send_simple_cmd(const char *cmd, Frame *resp) {
    if (!cmd || !resp) {
        return REQ_ERROR;
    }

    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return REQ_ERROR;
    }

    cJSON_AddStringToObject(json, "cmd", cmd);
    int rc = send_cmd(json, resp);
    cJSON_Delete(json);
    return rc;
}
