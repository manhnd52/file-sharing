#include <utils/file_system_util.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

void extract_file_name(const char *path, char *out, size_t out_size) {
    if (!path || !out || out_size == 0) {
        return;
    }
    const char *last_slash = strrchr(path, '/');
    const char *last_backslash = strrchr(path, '\\');
    const char *start = path;
    if (last_slash && last_backslash) {
        start = (last_backslash > last_slash) ? last_backslash + 1 : last_slash + 1;
    } else if (last_slash) {
        start = last_slash + 1;
    } else if (last_backslash) {
        start = last_backslash + 1;
    }

    size_t len = strlen(start);
    if (len >= out_size) {
        len = out_size - 1;
    }
    memcpy(out, start, len);
    out[len] = '\0';
}


uint64_t get_file_size(const char *file_path) {
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        return (uint64_t)-1; 
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return (uint64_t)-1;
    }

    long file_size_long = ftell(fp);
    if (file_size_long < 0) {
        fclose(fp);
        return (uint64_t)-1;
    }

    uint64_t file_size = (uint64_t)file_size_long;

    fclose(fp);
    return file_size;
}