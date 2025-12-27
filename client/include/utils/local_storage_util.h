#ifndef CLIENT_SRC_UTILS_LOCAL_STORAGE_UTIL_H
#define CLIENT_SRC_UTILS_LOCAL_STORAGE_UTIL_H

#include <stddef.h>

#define LOCAL_STORAGE_PATH "localStorage.json"
#define LOCAL_STORAGE_TOKEN_MAX 256
#define LOCAL_STORAGE_FOLDER_ID_MAX 64
#define LOCAL_STORAGE_USER_ID_MAX 64
#define LOCAL_STORAGE_USERNAME_MAX 64
#define LOCAL_STORAGE_EMAIL_MAX 128

typedef struct {
    char id[LOCAL_STORAGE_USER_ID_MAX];
    char username[LOCAL_STORAGE_USERNAME_MAX];
    char email[LOCAL_STORAGE_EMAIL_MAX];
} LocalStorageUser;

typedef struct {
    char token[LOCAL_STORAGE_TOKEN_MAX];
    char root_folder_id[LOCAL_STORAGE_FOLDER_ID_MAX];
    LocalStorageUser user;
} LocalStorageData;

int local_storage_load_file(const char *path, LocalStorageData *out);
int local_storage_load_default(LocalStorageData *out);
int local_storage_save_file(const char *path, const LocalStorageData *data);
int local_storage_save_default(const LocalStorageData *data);

#endif 
