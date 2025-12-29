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

bool extract_folder_name(const char *path, char *out_name, size_t out_size) {
    if (!path || !out_name || out_size == 0) {
        return false;
    }

    size_t len = strlen(path);
    while (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        --len;
    }

    if (len == 0) {
        return false;
    }

    const char *start = path + len - 1;
    while (start > path && *(start - 1) != '/' && *(start - 1) != '\\') {
        --start;
    }

    size_t name_len = (size_t)(path + len - start);
    if (name_len >= out_size) {
        name_len = out_size - 1;
    }

    memcpy(out_name, start, name_len);
    out_name[name_len] = '\0';
    return name_len > 0;
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

// Hàm tạo filepath nếu chưa có folder trên path, và tránh trùng lặp
char* create_unique_filepath(const char *filepath) {
    if (!filepath) return NULL;

    char *dup = strdup(filepath);
    if (!dup) return NULL;

    // tách phần thư mục và tên file
    char dir[1024] = "";
    char filename[252] = "";
    char *last_sep = strrchr(dup, '/');

    if (last_sep) {
        *last_sep = 0;
        strncpy(dir, dup, sizeof(dir)-1);
        dir[sizeof(dir)-1] = '\0';
        strncpy(filename, last_sep + 1, sizeof(filename)-1);
        filename[sizeof(filename)-1] = '\0';

        if (mkdirs(dir) != 0) {
            free(dup);
            return NULL;
        }
    } else {
        strncpy(filename, dup, sizeof(filename)-1);
        filename[sizeof(filename)-1] = '\0';
    }

    free(dup);

    // build final path
    char final_path[2048];
    snprintf(final_path, sizeof(final_path), "%s%s%s",
             dir[0] ? dir : "", dir[0] ? "/" : "", filename);

    // thêm "new_" nếu file tồn tại
    while (access(final_path, F_OK) == 0) {
        char new_file_name[256];
        snprintf(new_file_name, sizeof(new_file_name), "new_%s", filename);
        snprintf(final_path, sizeof(final_path), "%s%s%s",
                 dir[0] ? dir : "", dir[0] ? "/" : "", new_file_name);

        // update filename cho vòng while tiếp theo
        strncpy(filename, new_file_name, sizeof(filename)-1);
        filename[sizeof(filename)-1] = '\0';
    }

    return strdup(final_path); // trả về caller, caller free
}

// Ensure the target directory exists, make new folder if needed
bool ensure_directory(const char* path) {
    if (!path || path[0] == '\0') {
        return false;
    }
    return mkdirs(path) == 0;
}

// Build a child path by joining base and component with a separator as needed.
// Component is file or folder name
bool join_path(char* out, size_t out_size, const char* base,
                      const char* component) {
    if (!out || out_size == 0 || !base || !component || component[0] == '\0') {
        return false;
    }
    size_t base_len = strlen(base);
    bool has_sep = base_len > 0 &&
                   (base[base_len - 1] == '/' || base[base_len - 1] == '\\');
    int written = has_sep
                      ? snprintf(out, out_size, "%s%s", base, component)
                      : snprintf(out, out_size, "%s/%s", base, component);
    if (written < 0 || (size_t)written >= out_size) {
        return false;
    }
    return true;
}

bool is_dot_or_dotdot(const char *name) {
    return name && (strcmp(name, ".") == 0 || strcmp(name, "..") == 0);
}