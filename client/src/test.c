#include "api/auth_api.h"
#include "api/download_api.h"
#include "api/upload_api.h"
#include "client.h"
#include "cJSON.h"
#include "frame.h"
#include "utils/config_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static char *payload_to_string(const Frame *resp) {
    if (!resp || resp->payload_len == 0) {
        return NULL;
    }
    char *text = (char *)malloc(resp->payload_len + 1);
    if (!text) {
        return NULL;
    }
    memcpy(text, resp->payload, resp->payload_len);
    text[resp->payload_len] = '\0';
    return text;
}

static bool validate_response(const char *label, RequestResult rc, const Frame *resp, bool expect_ok) {
    if (rc != REQ_OK) {
        fprintf(stderr, "[TEST] %s failed to receive response (%d)\n", label, rc);
        return false;
    }
    if (!resp || resp->msg_type != MSG_RESPOND) {
        fprintf(stderr, "[TEST] %s got unexpected frame type\n", label);
        return false;
    }
    bool status_ok = resp->header.resp.status == STATUS_OK;
    if (expect_ok && !status_ok) {
        fprintf(stderr, "[TEST] %s response not ok: %d\n", label, resp->header.resp.status);
        char *payload = payload_to_string(resp);
        if (payload) {
            fprintf(stderr, "        payload: %s\n", payload);
            free(payload);
        }
        return false;
    }
    if (!expect_ok && status_ok) {
        fprintf(stderr, "[TEST] %s unexpectedly succeeded\n", label);
        return false;
    }
    return true;
}

static bool test_ping(void) {
    Frame resp = {0};
    int rc = send_simple_cmd("PING", &resp);
    return validate_response("PING", rc, &resp, true);
}

static bool test_login_and_auth(char *token_out, size_t token_out_len) {
    Frame login_resp = {0};
    int rc = login_api("alice", "123", &login_resp);
    if (!validate_response("login_api", rc, &login_resp, true)) {
        return false;
    }

    char *payload = payload_to_string(&login_resp);
    if (!payload) {
        fprintf(stderr, "[TEST] login_api payload missing\n");
        return false;
    }
    cJSON *root = cJSON_Parse(payload);
    free(payload);
    if (!root) {
        fprintf(stderr, "[TEST] login_api payload invalid json\n");
        return false;
    }

    cJSON *token_item = cJSON_GetObjectItemCaseSensitive(root, "token");
    if (!cJSON_IsString(token_item) || token_item->valuestring[0] == '\0') {
        fprintf(stderr, "[TEST] login_api missing token\n");
        cJSON_Delete(root);
        return false;
    }

    if (token_out && token_out_len > 0) {
        strncpy(token_out, token_item->valuestring, token_out_len - 1);
        token_out[token_out_len - 1] = '\0';
    }
    cJSON_Delete(root);

    Frame auth_resp = {0};
    rc = auth_api(token_out, &auth_resp);
    return validate_response("auth_api", rc, &auth_resp, true);
}

static bool test_get_me(void) {
    Frame resp = {0};
    int rc = me_api(&resp);
    return validate_response("me_api", rc, &resp, true);
}

static bool test_cancel_download(void) {
    Frame resp = {0};
    int rc = download_cancel_api("00000000-0000-0000-0000-000000000000", &resp);
    return validate_response("download_cancel_api", rc, &resp, false);
}

static bool test_cancel_upload(void) {
    Frame resp = {0};
    int rc = upload_cancel_api("00000000-0000-0000-0000-000000000000", &resp);
    return validate_response("upload_cancel_api", rc, &resp, false);
}

int run_client_tests(void) {
    ConfigData cfg = {0};
    if (config_load_default(&cfg) != 0) {
        fprintf(stderr, "Failed to load %s\n", DEFAULT_PATH);
        return EXIT_FAILURE;
    }

    printf("Connecting to %s:%d\n", cfg.server, cfg.port);
    if (client_connect(cfg.server, (uint16_t)cfg.port, 10) != 0) {
        fprintf(stderr, "Unable to connect to %s:%d\n", cfg.server, cfg.port);
        return EXIT_FAILURE;
    }

    bool success = true;
    success &= test_ping();

    char token[128] = {0};
    success &= test_login_and_auth(token, sizeof(token));
    success &= test_get_me();
    success &= test_cancel_download();
    success &= test_cancel_upload();

    Frame logout_resp = {0};
    logout_api(&logout_resp);

    client_disconnect();

    printf("\nClient tests %s.\n", success ? "PASSED" : "FAILED");
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
