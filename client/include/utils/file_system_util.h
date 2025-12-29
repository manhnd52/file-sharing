#ifndef FILE_SYSTEM_UTILS
#define FILE_SYSTEM_UTILS
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void extract_file_name(const char *path, char *out, size_t out_size);
uint64_t get_file_size(const char *file_path);
FILE* fopen_mkdir(const char *filepath, const char *mode);
int mkdirs(const char *path);
#endif