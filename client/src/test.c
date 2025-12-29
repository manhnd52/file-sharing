#include "test.h"

#include "api/auth_api.h"
#include "api/folder_api.h"
#include "api/download_api.h"
#include "api/file_api.h"
#include "api/upload_api.h"
#include "client.h"
#include "cJSON.h"
#include "frame.h"
#include "utils/config_util.h"

#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char saved_token[256] = {0};
static int saved_root_folder_id = 0;

static void trim_newline(char *text) {
    if (!text) {
        return;
    }
    size_t len = strlen(text);
    if (len == 0) {
        return;
    }
    if (text[len - 1] == '\n') {
        text[len - 1] = '\0';
    }
}

static bool read_line(const char *prompt, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return false;
    }
    if (prompt) {
        printf("%s: ", prompt);
    }
    fflush(stdout);
    if (!fgets(buffer, (int)size, stdin)) {
        buffer[0] = '\0';
        return false;
    }
    trim_newline(buffer);
    return true;
}

static char *payload_to_string(const Frame *resp) {
    if (!resp || resp->payload_len == 0) {
        return NULL;
    }
    char *text = (char *)malloc(resp->payload_len + 1);
    if (!text) {
        return NULL;
    }
    memcpy(text, resp->payload, resp->payload_len);
    text[resp->payload_len] = '\0';
    return text;
}

static void print_response(const Frame *resp) {
    if (!resp) {
        return;
    }

    const char *status = resp->header.resp.status == STATUS_OK ? "ok" : "not_ok";
    printf("  status: %s\n", status);

    char *payload = payload_to_string(resp);
    if (payload) {
        printf("  payload: %s\n", payload);
        free(payload);
    } else {
        printf("  payload: <empty>\n");
    }
}

static void update_saved_token(const Frame *resp) {
    char *payload = payload_to_string(resp);
    if (!payload) {
        return;
    }
    cJSON *root = cJSON_Parse(payload);
    free(payload);
    if (!root) {
        return;
    }

    cJSON *token_item = cJSON_GetObjectItemCaseSensitive(root, "token");
    if (cJSON_IsString(token_item) && token_item->valuestring) {
        strncpy(saved_token, token_item->valuestring, sizeof(saved_token) - 1);
        saved_token[sizeof(saved_token) - 1] = '\0';
    }

    cJSON *root_folder_id = cJSON_GetObjectItemCaseSensitive(root, "root_folder_id");
    if (cJSON_IsNumber(root_folder_id)) {
        saved_root_folder_id = root_folder_id->valueint;
    }

    cJSON_Delete(root);
}

static void handle_register(void) {
    char username[128] = {0};
    char password[128] = {0};

    puts("Register new user:");
    read_line("  username", username, sizeof(username));
    read_line("  password", password, sizeof(password));

    Frame resp = {0};
    int rc = register_api(username, password, &resp);
    if (rc != 0) {
        fprintf(stderr, "register_api failed (%d)\n", rc);
        return;
    }

    print_response(&resp);
}

static void handle_login(void) {
    char username[128] = {0};
    char password[128] = {0};

    puts("Login to server:");
    read_line("  username", username, sizeof(username));
    read_line("  password", password, sizeof(password));

    Frame resp = {0};
    int rc = login_api(username, password, &resp);
    if (rc != 0) {
        fprintf(stderr, "login_api failed (%d)\n", rc);
        return;
    }

    print_response(&resp);
    if (resp.header.resp.status == STATUS_OK) {
        update_saved_token(&resp);
    }
}

static void handle_auth(void) {
    char token[256] = {0};
    puts("Authenticate using token:");
    if (saved_token[0]) {
        printf("  Use cached token (%s)? [Y/n]: ", saved_token);
        fflush(stdout);
        char choice[8] = {0};
        if (fgets(choice, sizeof(choice), stdin)) {
            if (choice[0] != 'n' && choice[0] != 'N') {
                strncpy(token, saved_token, sizeof(token) - 1);
                token[sizeof(token) - 1] = '\0';
            }
        }
    }

    if (token[0] == '\0') {
        read_line("  token", token, sizeof(token));
    }

    if (token[0] == '\0') {
        puts("  no token provided");
        return;
    }

    Frame resp = {0};
    int rc = auth_api(token, &resp);
    if (rc != 0) {
        fprintf(stderr, "auth_api failed (%d)\n", rc);
        return;
    }

    print_response(&resp);
    if (resp.header.resp.status == STATUS_OK) {
        update_saved_token(&resp);
    }
}

static void handle_logout(void) {
    Frame resp = {0};
    int rc = logout_api(&resp);
    if (rc != 0) {
        fprintf(stderr, "logout_api failed (%d)\n", rc);
        return;
    }
    print_response(&resp);
    memset(saved_token, 0, sizeof(saved_token));
    saved_root_folder_id = 0;
}

static void handle_get_me(void) {
    Frame resp = {0};
    int rc = me_api(&resp);
    if (rc != 0) {
        fprintf(stderr, "me_api failed (%d)\n", rc);
        return;
    }
    print_response(&resp);
}

static void handle_ping(void) {
    Frame resp = {0};
    int rc = send_simple_cmd("PING", &resp);
    if (rc != 0) {
        fprintf(stderr, "ping failed (%d)\n", rc);
        return;
    }
    print_response(&resp);
}

static void handle_list(void) {
    char input[64] = {0};
    if (saved_root_folder_id > 0) {
        printf("  default root folder_id=%d (press Enter to reuse)\n", saved_root_folder_id);
    }
    read_line("  folder_id (0 = auto)", input, sizeof(input));

    int folder_id = 0;
    if (input[0] != '\0') {
        folder_id = (int)strtol(input, NULL, 10);
    }

    if (folder_id == 0 && saved_root_folder_id > 0) {
        folder_id = saved_root_folder_id;
    }

    Frame resp = {0};
    int rc = list_api(folder_id, &resp);
    if (rc != 0) {
        fprintf(stderr, "list_api failed (%d)\n", rc);
        return;
    }
    print_response(&resp);
}

static void handle_upload(void) {
    char file_path[512] = {0};
    char parent_input[64] = {0};
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working directory: %s\n", cwd);
    }

    puts("Upload file:");
    read_line("  file path", file_path, sizeof(file_path));
    if (file_path[0] == '\0') {
        puts("  file path required");
        return;
    }

    int parent_folder_id = 0;
    if (saved_root_folder_id > 0) {
        printf("  parent_folder_id default=%d (press Enter to reuse)\n", saved_root_folder_id);
    }
    read_line("  parent_folder_id", parent_input, sizeof(parent_input));
    if (parent_input[0] == '\0' && saved_root_folder_id > 0) {
        parent_folder_id = saved_root_folder_id;
    } else {
        parent_folder_id = (int)strtol(parent_input, NULL, 10);
    }

    if (parent_folder_id <= 0) {
        puts("  invalid parent_folder_id");
        return;
    }

    Frame resp = {0};
    int rc = upload_file_api(file_path, parent_folder_id, &resp);
    if (rc != 0) {
        fprintf(stderr, "upload_file_api failed (%d)\n", rc);
        return;
    }
    print_response(&resp);
}

static void handle_download_folder(void) {
    char storage_path[512] = {0};
    char file_input[64] = {0};
    puts("Download folder:");
    read_line("  folder_id", file_input, sizeof(file_input));
    int folder_id = (int)strtol(file_input, NULL, 10);
    if (folder_id <= 0) {
        puts("  invalid folder_id");
        return;
    }
    
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working directory: %s\n", cwd);
    }

    read_line("  output path", storage_path, sizeof(storage_path));
    if (storage_path[0] == '\0') {
        puts("  output path required");
        return;
    }

    Frame resp = {0};
    int rc = download_folder_api(storage_path, folder_id, &resp);
    if (rc != 0) {
        fprintf(stderr, "download_folder_api failed (%d)\n", rc);
        return;
    }
    print_response(&resp);
}

static void handle_download_file(void) {
    char storage_path[512] = {0};
    char file_input[64] = {0};
    puts("Download file:");
    read_line("  file_id", file_input, sizeof(file_input));
    int file_id = (int)strtol(file_input, NULL, 10);
    if (file_id <= 0) {
        puts("  invalid file_id");
        return;
    }

    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("Current working directory: %s\n", cwd);
    }

    read_line("  output path", storage_path, sizeof(storage_path));
    if (storage_path[0] == '\0') {
        puts("  output path required");
        return;
    }

    Frame resp = {0};
    int rc = download_file_api(storage_path, file_id, &resp);
    if (rc != 0) {
        fprintf(stderr, "download_file_api failed (%d)\n", rc);
        return;
    }
    print_response(&resp);
}

static void show_menu(void) {
    puts("\n=== Client API Tester ===");
    puts(" 1) Register");
    puts(" 2) Login");
    puts(" 3) Authenticate (token)");
    puts(" 4) Logout");
    puts(" 5) GET_ME");
    puts(" 6) PING");
    puts(" 7) LIST folder");
    puts(" 8) Upload file");
    puts(" 9) Download folder");
    puts("10) Download file");
    puts(" 0) Exit");
    printf("Choose an option: ");
    fflush(stdout);
}

int run_client_tests(void) {
    ConfigData cfg = {0};
    if (config_load_default(&cfg) != 0) {
        fprintf(stderr, "Failed to load %s\n", DEFAULT_PATH);
        return EXIT_FAILURE;
    }

    printf("Connecting to %s:%d\n", cfg.server, cfg.port);
    if (client_connect(cfg.server, (uint16_t)cfg.port, 10) != 0) {
        fprintf(stderr, "Unable to connect to %s:%d\n", cfg.server, cfg.port);
        return EXIT_FAILURE;
    }

    bool running = true;
    char choice[8] = {0};

    while (running) {
        show_menu();
        if (!fgets(choice, sizeof(choice), stdin)) {
            break;
        }
        trim_newline(choice);
        if (choice[0] == '\0') {
            continue;
        }

        int option = (int)strtol(choice, NULL, 10);
        switch (option) {
            case 1:
                handle_register();
                break;
            case 2:
                handle_login();
                break;
            case 3:
                handle_auth();
                break;
            case 4:
                handle_logout();
                break;
            case 5:
                handle_get_me();
                break;
            case 6:
                handle_ping();
                break;
            case 7:
                handle_list();
                break;
            case 8:
                handle_upload();
                break;
            case 9:
                handle_download_folder();
                break;
            case 10:
                handle_download_file();
                break;
            case 0:
                running = false;
                break;
            default:
                puts("Unknown option");
        }
    }

    client_disconnect();
    return EXIT_SUCCESS;
}
