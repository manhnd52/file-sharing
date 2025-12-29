#ifndef FILE_SYSTEM_UTILS
#define FILE_SYSTEM_UTILS
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

void extract_file_name(const char *path, char *out, size_t out_size);
uint64_t get_file_size(const char *file_path);
char* create_unique_filepath(const char *filepath);
int mkdirs(const char *path);
bool ensure_directory(const char* path);
bool join_path(char* out, size_t out_size, const char* base,
                      const char* component);
#endif