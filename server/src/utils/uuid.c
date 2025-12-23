#include <stdio.h>
#include <string.h>
#include "utils/uuid.h"

int bytes_to_uuid_string(const uint8_t id[BYTE_UUID_SIZE], char out[37]) {
	// Format as UUID v4 style: 8-4-4-4-12 from 16 bytes
	// If bytes already a UUID, this will render correctly; otherwise deterministic formatting.
	static const int idxs[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
	// snprintf into out
	int n = snprintf(out, 37,
									 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
									 id[idxs[0]], id[idxs[1]], id[idxs[2]], id[idxs[3]],
									 id[idxs[4]], id[idxs[5]],
									 id[idxs[6]], id[idxs[7]],
									 id[idxs[8]], id[idxs[9]],
									 id[idxs[10]], id[idxs[11]], id[idxs[12]], id[idxs[13]], id[idxs[14]], id[idxs[15]]);
	return (n == 36) ? 0 : -1;
}

int uuid_string_to_bytes(const char *uuid_str, uint8_t out[BYTE_UUID_SIZE]) {
    if (!uuid_str || strlen(uuid_str) != 36) return -1;

    int idxs[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    int str_idxs[16] = {
        0, 2, 4, 6,
        9, 11,
        14, 16,
        19, 21,
        24, 26, 28, 30, 32, 34
    };

    for (int i = 0; i < 16; ++i) {
        char byte_str[3] = { uuid_str[str_idxs[i]], uuid_str[str_idxs[i]+1], '\0' };
        unsigned int byte_val;
        if (sscanf(byte_str, "%02x", &byte_val) != 1) {
            return -1;
        }
        out[idxs[i]] = (uint8_t)byte_val;
    }
    return 0;
}

// Trả về 16 byte random để làm session ID
int generate_byte_uuid(uint8_t out[BYTE_UUID_SIZE]) {
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0) return -1;
	ssize_t r = read(fd, out, BYTE_UUID_SIZE);
	close(fd);
	return (r == BYTE_UUID_SIZE) ? 0 : -1;
}
