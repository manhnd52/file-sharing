#ifndef AUTHORIZE_SERVICE_H
#define AUTHORIZE_SERVICE_H
#include <stdbool.h>

typedef enum {
    PERM_NONE = 0,
    PERM_READ = 1,
    PERM_WRITE = 2
} PermissionLevel;

// Trả về mức quyền của user với folder (owner => PERM_WRITE)
PermissionLevel get_folder_permission(int user_id, int folder_id);
PermissionLevel get_file_permission(int user_id, int file_id);

bool authorize_folder_access(int user_id, int folder_id, PermissionLevel required);
bool authorize_file_access(int user_id, int file_id, PermissionLevel required);

// Bỏ quyền share cho user với target (target_type: 0 file, 1 folder)
bool revoke_permission(int user_id, int target_type, int target_id);

#endif // AUTHORIZE_SERVICE_H
