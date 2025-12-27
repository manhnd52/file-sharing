#include "cJSON.h"
#include "client.h"
#include "api/auth_api.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Connect *g_conn = NULL;

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <host> <port> <username> <password>\n", prog);
    exit(EXIT_FAILURE);
}

static void reset_frame(Frame *frame) {
    if (frame) {
        memset(frame, 0, sizeof(*frame));
    }
}

static bool response_is_ok(const Frame *frame) {
    return frame && frame->msg_type == MSG_RESPOND &&
           frame->header.resp.status == STATUS_OK;
}

static void print_frame(const char *label, const Frame *frame) {
    if (!frame) {
        printf("[%s] <null frame>\n", label);
        return;
    }

    const char *type = "UNKNOWN";
    const char *status = "N/A";
    uint32_t request_id = 0;

    switch (frame->msg_type) {
    case MSG_CMD:
        type = "CMD";
        break;
    case MSG_RESPOND:
        type = "RESPOND";
        request_id = frame->header.resp.request_id;
        status = frame->header.resp.status == STATUS_OK ? "OK" : "NOT_OK";
        break;
    case MSG_DATA:
        type = "DATA";
        break;
    case MSG_AUTH:
        type = "AUTH";
        break;
    default:
        type = "UNKNOWN";
    }

    printf("[%s] %s request_id=%u status=%s payload_len=%zu\n", label, type,
           request_id, status, frame->payload_len);
    if (frame->payload_len > 0) {
        printf("[%s] payload: %.*s\n", label, (int)frame->payload_len,
               (char *)frame->payload);
    }
}

// Why need
static char *duplicate_payload(const Frame *frame) {
    if (!frame || frame->payload_len == 0) {
        return NULL;
    }
    size_t len = frame->payload_len;
    char *dup = malloc(len + 1);
    if (!dup) {
        return NULL;
    }
    memcpy(dup, frame->payload, len);
    dup[len] = '\0';
    return dup;
}

static char *extract_token(const Frame *frame) {
    char *payload = duplicate_payload(frame);
    if (!payload) {
        return NULL;
    }
    cJSON *root = cJSON_Parse(payload);
    free(payload);
    if (!root) {
        return NULL;
    }
    cJSON *token_item = cJSON_GetObjectItemCaseSensitive(root, "token");
    char *token = NULL;
    if (cJSON_IsString(token_item) && token_item->valuestring) {
        token = strdup(token_item->valuestring);
    }
    cJSON_Delete(root);
    return token;
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

int main(int argc, char **argv) {
    if (argc != 5) {
        usage(argv[0]);
    }

    const char *host = argv[1];
    const char *port_str = argv[2];
    const char *username = argv[3];
    const char *password = argv[4];

    char *endptr = NULL;
    long port = strtol(port_str, &endptr, 10);
    if (*endptr != '\0' || port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %s\n", port_str);
        return EXIT_FAILURE;
    }

    Connect *conn = connect_create(host, (uint16_t)port, 5);
    if (!conn) {
        perror("connect_create");
        return EXIT_FAILURE;
    }
    g_conn = conn;

    Frame resp = {0};
    int exit_code = EXIT_SUCCESS;
    char *token = NULL;

    int rc = register_api(username, password, &resp);
    print_frame("REGISTER", &resp);
    if (rc != 0) {
        fprintf(stderr, "REGISTER: failed to build or send payload\n");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }
    if (!response_is_ok(&resp)) {
        printf("REGISTER responded with NOT_OK; the user might already exist.\n");
    }

    reset_frame(&resp);
    rc = login_api(username, password, &resp);
    print_frame("LOGIN", &resp);
    if (rc != 0) {
        fprintf(stderr, "LOGIN: failed to build or send payload\n");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }
    if (!response_is_ok(&resp)) {
        fprintf(stderr, "LOGIN: server denied credentials\n");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    token = extract_token(&resp);
    if (!token) {
        fprintf(stderr, "LOGIN: response missing token\n");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    reset_frame(&resp);
    rc = auth_api(token, &resp);
    print_frame("AUTH", &resp);
    if (rc != 0 || !response_is_ok(&resp)) {
        fprintf(stderr, "AUTH: failed or server rejected the token\n");
        exit_code = EXIT_FAILURE;
    }

cleanup:
    reset_frame(&resp);
    rc = logout_api(conn, &resp);
    print_frame("LOGOUT", &resp);
    if (rc != 0 || !response_is_ok(&resp)) {
        fprintf(stderr, "LOGOUT: failed to send logout command or response denied\n");
        exit_code = EXIT_FAILURE;
    }

    free(token);
    connect_destroy(conn);
    g_conn = NULL;
    return exit_code;
}
