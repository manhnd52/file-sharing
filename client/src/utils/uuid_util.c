#include "utils/uuid_util.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

int uuid_string_to_bytes(const char *uuid_str, uint8_t out[SESSIONID_SIZE]) {
    if (!uuid_str || strlen(uuid_str) != 36) {
        return -1;
    }

    const int str_idxs[SESSIONID_SIZE] = {
        0, 2, 4, 6,
        9, 11,
        14, 16,
        19, 21,
        24, 26, 28, 30, 32, 34
    };

    for (int i = 0; i < SESSIONID_SIZE; ++i) {
        char byte_str[3] = { uuid_str[str_idxs[i]], uuid_str[str_idxs[i] + 1], '\0' };
        unsigned int byte_val = 0;
        if (sscanf(byte_str, "%02x", &byte_val) != 1) {
            return -1;
        }
        out[i] = (uint8_t)byte_val;
    }
    return 0;
}