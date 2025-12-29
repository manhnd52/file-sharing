#include <utils/file_system_util.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

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

int mkdirs(const char *path) {
    char tmp[1024];
    size_t len = strlen(path);
    if (len >= sizeof(tmp)) return -1;

    strcpy(tmp, path);

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = 0;
            if (mkdir(tmp, 0755) != 0) {
                if (errno != EEXIST) return -1;
            }
            *p = '/';
        }
    }
    
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;

    return 0;
}

// Hàm mở file, tự tạo thư mục nếu cần
FILE* fopen_mkdir(const char *filepath, const char *mode) {
    char *dup = strdup(filepath);
    if (!dup) return NULL;

    // tách phần thư mục
    char *last_sep = strrchr(dup, '/');
    char dir[1024] = "";
    char filename[1024] = "";
    if (last_sep) {
        *last_sep = 0;
        strcpy(dir, dup);               // phần thư mục
        strcpy(filename, last_sep + 1); // tên file
        if (mkdirs(dir) != 0) {
            free(dup);
            return NULL;
        }
    } else {
        strcpy(filename, dup); // không có thư mục
    }

    free(dup);

    // kiểm tra file tồn tại, thêm _new trước phần mở rộng
    char final_path[2048];
    snprintf(final_path, sizeof(final_path), "%s%s%s",
             dir[0] ? dir : "", dir[0] ? "/" : "", filename);

    while (access(final_path, F_OK) == 0) {
        char new_file_name[256];
        snprintf(new_file_name, sizeof(new_file_name), "new_%s", filename);
        snprintf(final_path, sizeof(final_path), "%s%s%s",
                 dir[0] ? dir : "", dir[0] ? "/" : "", new_file_name);
    }

    return fopen(final_path, mode);
}