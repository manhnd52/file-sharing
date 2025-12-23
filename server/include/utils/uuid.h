#ifndef UUID_H
#define UUID_H
#include <stdint.h>
#include <stddef.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define BYTE_UUID_SIZE 16

int bytes_to_uuid_string(const uint8_t id[BYTE_UUID_SIZE], char out[37]);
int uuid_string_to_bytes(const char *uuid_str, uint8_t out[BYTE_UUID_SIZE]);
int generate_byte_uuid(uint8_t out[BYTE_UUID_SIZE]);

#endif // UUID_H
