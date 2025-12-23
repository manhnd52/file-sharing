// #include "services/file_service.h"
// #include <stdio.h>
// // File này test các tương tác với db bằng cách gọi các hàm trong file_service

// int main() {
//     // Khởi tạo DB
//     if (!db_start()) {
//         fprintf(stderr, "Failed to start database\n");
//         return 1;
//     }

//     int user_id = 1; // Giả sử user_id = 1

//     // Tạo thư mục root cho user
//     int root_folder_id = folder_get_or_create_user_root(user_id);
//     if (root_folder_id <= 0) {
//         fprintf(stderr, "Failed to create or get root folder for user %d\n", user_id);
//         db_close();
//         return 1;
//     }
//     printf("Root folder ID for user %d: %d\n", user_id, root_folder_id);

//     // Tạo thư mục con
//     const char* new_folder_path = "/documents";
//     int new_folder_id = folder_save_metadata(user_id, new_folder_path);
//     if (new_folder_id <= 0) {
//         fprintf(stderr, "Failed to create folder %s for user %d\n", new_folder_path, user_id);
//         db_close();
//         return 1;
//     }
//     printf("Created folder %s with ID %d for user %d\n", new_folder_path, new_folder_id, user_id);

//     db_close();
//     return 0;
// }

