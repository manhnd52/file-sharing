#include "test.h"

#include "frame.h"

#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int log_test(const char *name, bool success) {
    if (success) {
        printf("[ OK ] %s\n", name);
        return 0;
    }
    fprintf(stderr, "[FAIL] %s\n", name);
    return 1;
}

static bool test_build_cmd_frame(void) {
    const char payload[] = "{\"action\":\"ping\"}";
    Frame frame = {0};
    if (build_cmd_frame(&frame, 42, payload) != 0) {
        return false;
    }
    if (frame.msg_type != MSG_CMD ||
        frame.header.cmd.request_id != 42 ||
        frame.payload_len != strlen(payload)) {
        return false;
    }
    return memcmp(frame.payload, payload, frame.payload_len) == 0;
}

static bool test_parse_frame_cmd(void) {
    const char payload[] = "{\"cmd\":\"ok\"}";
    const size_t payload_len = sizeof(payload) - 1;
    const size_t buf_len = MST_TYPE_SIZE + CMD_HEADER_SIZE + payload_len;
    uint8_t buffer[buf_len];
    buffer[0] = MSG_CMD;

    const uint32_t request_id = htonl(2023);
    memcpy(buffer + MST_TYPE_SIZE, &request_id, sizeof(request_id));
    memcpy(buffer + MST_TYPE_SIZE + CMD_HEADER_SIZE, payload, payload_len);

    Frame parsed = {0};
    if (parse_frame(buffer, buf_len, &parsed) != 0) {
        return false;
    }

    if (parsed.msg_type != MSG_CMD ||
        parsed.header.cmd.request_id != 2023 ||
        parsed.payload_len != payload_len) {
        return false;
    }
    return memcmp(parsed.payload, payload, payload_len) == 0;
}

int run_client_tests(void) {
    int failures = 0;
    failures += log_test("build_cmd_frame", test_build_cmd_frame());
    failures += log_test("parse_frame_cmd", test_parse_frame_cmd());

    if (failures == 0) {
        puts("All client tests passed.");
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "%d client test(s) failed\n", failures);
    return EXIT_FAILURE;
}
