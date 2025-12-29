#ifndef UUID_UTIL_H
#define UUID_UTIL_H
#include "frame.h"
#include <stddef.h>

int uuid_string_to_bytes(const char *uuid_str, uint8_t out[SESSIONID_SIZE]);
#endif