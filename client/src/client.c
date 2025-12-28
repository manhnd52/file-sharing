#include "client.h"
#include "cJSON.h"

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
        return -1;
    }

    char *payload = cJSON_PrintUnformatted(json);
    if (!payload) {
        return -1;
    }

    Frame request = {0};
    int rc = build_cmd_frame(&request, 0, payload);
    if (rc == 0) {
        rc = connect_send_request(g_conn, &request, res);
    }

    free(payload);
    return rc;
}
