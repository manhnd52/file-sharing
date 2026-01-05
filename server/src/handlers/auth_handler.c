// handlers/auth.c
#include "handlers/auth_handler.h"
#include "cJSON.h"
#include "services/user_service.h"
#include "services/folder_service.h"
#include "frame.h"
#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <time.h>

void handle_login(Conn *c, Frame *f) {
    if (!c || !f || f->msg_type != MSG_CMD) {
        printf("[AUTH:LOGIN][ERROR] Invalid frame type for login (fd=%d, msg_type=%d)\n", 
               c ? c->sockfd : -1, f ? f->msg_type : -1);
        return;
    }

    uint32_t request_id = f->header.cmd.request_id;
    
    // Parse JSON payload to get username and password
    if (f->payload_len == 0) {
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                          "{\"error\":\"missing_credentials\"}");
        send_data(c, resp);
        return;
    }

    cJSON *root = cJSON_Parse((char *)f->payload);
    if (!root) {
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                          "{\"error\":\"invalid_json\"}");
        send_data(c, resp);
        return;
    }

    cJSON *username_item = cJSON_GetObjectItemCaseSensitive(root, "username");
    cJSON *password_item = cJSON_GetObjectItemCaseSensitive(root, "password");

    if (!cJSON_IsString(username_item) || !cJSON_IsString(password_item)) {
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                          "{\"error\":\"missing_username_or_password\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }

    const char *username = username_item->valuestring;
    const char *password = password_item->valuestring;

    printf("[AUTH:LOGIN][INFO] Login attempt: username='%s' (fd=%d)\n", username, c->sockfd);

    // Verify credentials
    int user_id = user_verify_credentials(username, password);

    if (user_id > 0) {
        // Authentication successful
        c->logged_in = true;
        c->user_id = user_id;
        
        // Create session token (expires in 24 hours)
        char* token = user_create_session_token(user_id, 24);
        if (!token) {
            Frame resp;
            build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                              "{\"error\":\"token_creation_failed\"}");
            send_data(c, resp);
            cJSON_Delete(root);
            printf("[AUTH:LOGIN][ERROR] Failed to create token for user_id=%d (fd=%d)\n", 
                   user_id, c->sockfd);
            return;
        }
        
        // Store token in connection
        strncpy(c->auth_token, token, sizeof(c->auth_token) - 1);
        c->auth_token[sizeof(c->auth_token) - 1] = '\0';
        c->auth_expiry = time(NULL) + (24 * 3600); // 24 hours from now
        
        // Build success response with token
        char payload[512];
        int root_folder_id = folder_get_or_create_user_root(user_id);
        snprintf(payload, sizeof(payload), 
                "{\"success\":true,\"user_id\":%d,\"username\":\"%s\",\"root_folder_id\":%d,\"token\":\"%s\",\"expires_in\":86400}",
                user_id, username, root_folder_id, token);
        
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_OK, payload);
        send_data(c, resp);

        // Update connection state
        c->logged_in = true;
        
        printf("[AUTH:LOGIN][SUCCESS] User authenticated: username='%s', user_id=%d, token=%s (fd=%d)\n", 
               username, user_id, token, c->sockfd);
        
        free(token);
    } else {
        // Authentication failed
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                          "{\"error\":\"invalid_credentials\"}");
        send_data(c, resp);
        
        printf("[AUTH:LOGIN][WARN] Authentication failed: username='%s' (fd=%d)\n", username, c->sockfd);
    }

    cJSON_Delete(root);
}


void handle_register(Conn *c, Frame *f) {

    if (!c || !f || f->msg_type != MSG_CMD) {
        printf("[AUTH:REGISTER][ERROR] Invalid frame type (fd=%d, msg_type=%d)\n",
               c ? c->sockfd : -1, f ? f->msg_type : -1);
        return;
    }

    uint32_t request_id = f->header.cmd.request_id;

    if (f->payload_len == 0) {
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_payload\"}");
        send_data(c, resp);
        return;
    }

    cJSON *root = cJSON_Parse((char *)f->payload);
    if (!root) {
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                            "{\"error\":\"invalid_json\"}");
        send_data(c, resp);
        return;
    }

    cJSON *username_item = cJSON_GetObjectItemCaseSensitive(root, "username");
    cJSON *password_item = cJSON_GetObjectItemCaseSensitive(root, "password");

    if (!cJSON_IsString(username_item) || !cJSON_IsString(password_item)) {
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_username_or_password\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }

    const char *username = username_item->valuestring;
    const char *password = password_item->valuestring;

    printf("[AUTH:REGISTER][INFO] Register attempt: username='%s' (fd=%d)\n",
           username, c->sockfd);

    int user_id = user_create(username, password);

    if (user_id < 0) {
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                            "{\"error\":\"username_already_exists\"}");
        send_data(c, resp);
        printf("[AUTH:REGISTER][WARN] Username already exists: '%s' (fd=%d)\n",
               username, c->sockfd);
    } else if (user_id == 0) {
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                            "{\"error\":\"db_error\"}");
        send_data(c, resp);
        printf("[AUTH:REGISTER][WARN] Failed to register username='%s' (fd=%d)\n",
               username, c->sockfd);
    } else {
        char payload[256];
        snprintf(payload, sizeof(payload),
                 "{\"success\":true,\"user_id\":%d,\"username\":\"%s\"}",
                 user_id, username);

        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_OK, payload);
        send_data(c, resp);

        printf("[AUTH:REGISTER][SUCCESS] User registered: username='%s', user_id=%d (fd=%d)\n",
               username, user_id, c->sockfd);
    }

    cJSON_Delete(root);
}

void handle_auth_token(Conn *c, Frame *f) {

    if (!c || !f || f->msg_type != MSG_CMD) {
        printf("[AUTH:AUTH][ERROR] Invalid frame type (fd=%d, msg_type=%d)\n",
               c ? c->sockfd : -1, f ? f->msg_type : -1);
        return;
    }

    uint32_t request_id = f->header.cmd.request_id;

    if (f->payload_len == 0) {
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_token\"}");
        send_data(c, resp);
        return;
    }

    cJSON *root = cJSON_Parse((char *)f->payload);
    if (!root) {
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                            "{\"error\":\"invalid_json\"}");
        send_data(c, resp);
        return;
    }

    cJSON *token_item = cJSON_GetObjectItemCaseSensitive(root, "token");
    if (!cJSON_IsString(token_item)) {
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                            "{\"error\":\"missing_token\"}");
        send_data(c, resp);
        cJSON_Delete(root);
        return;
    }

    const char *token = token_item->valuestring;
    int user_id = 0;
    bool valid = user_verify_token(token, &user_id);

    if (!valid || user_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                            "{\"error\":\"invalid_or_expired_token\"}");
        send_data(c, resp);
        printf("[AUTH:AUTH][WARN] Invalid token authentication (fd=%d)\n", c->sockfd);
        cJSON_Delete(root);
        return;
    }

    User user = get_user_by_id(user_id);

    c->logged_in = true;
    c->user_id = user_id;
    strncpy(c->auth_token, token, sizeof(c->auth_token) - 1);
    c->auth_token[sizeof(c->auth_token) - 1] = '\0';

    char payload[512];
    int root_folder_id = folder_get_or_create_user_root(user_id);

    snprintf(payload, sizeof(payload),
             "{\"success\":true,\"user_id\":%d,\"username\":\"%s\",\"root_folder_id\":%d,\"token\":\"%s\"}",
             user.id, user.username, root_folder_id, token);

    Frame resp;
    build_respond_frame(&resp, request_id, STATUS_OK, payload);
    send_data(c, resp);

    printf("[AUTH:AUTH][SUCCESS] Token authenticated: user_id=%d, username='%s' (fd=%d)\n",
           user.id, user.username, c->sockfd);

    cJSON_Delete(root);
}

void handle_logout(Conn *c, Frame *f) {

    if (!c || !f || f->msg_type != MSG_CMD) {
        printf("[AUTH:LOGOUT][ERROR] Invalid frame type (fd=%d, msg_type=%d)\n",
               c ? c->sockfd : -1, f ? f->msg_type : -1);
        return;
    }

    uint32_t request_id = f->header.cmd.request_id;

    bool token_invalidated = false;
    if (c->auth_token[0] != '\0') {
        token_invalidated = user_invalidate_token(c->auth_token);
    }

    c->logged_in = false;
    c->user_id = 0;
    c->auth_token[0] = '\0';
    c->auth_expiry = 0;

    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"success\":true,\"token_invalidated\":%s}",
             token_invalidated ? "true" : "false");

    Frame resp;
    build_respond_frame(&resp, request_id, STATUS_OK, payload);
    send_data(c, resp);

    printf("[AUTH:LOGOUT][INFO] User logged out (fd=%d)\n", c->sockfd);
}

void handle_get_me(Conn *c, Frame *f) {
    if (!c || !f || f->msg_type != MSG_CMD) {
        printf("[AUTH:GET_ME][ERROR] Invalid frame type (fd=%d, msg_type=%d)\n",
               c ? c->sockfd : -1, f ? f->msg_type : -1);
        return;
    }

    uint32_t request_id = f->header.cmd.request_id;

    if (!c->logged_in || c->user_id <= 0) {
        Frame resp;
        build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                            "{\"error\":\"not_authenticated\"}");
        send_data(c, resp);
        return;
    }

    User user = get_user_by_id(c->user_id);

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"user_id\":%d,\"username\":\"%s\"}",
             user.id, user.username);

    Frame resp;
    build_respond_frame(&resp, request_id, STATUS_OK, payload);
    send_data(c, resp);

    printf("[AUTH:GET_ME][INFO] Returned user info: user_id=%d, username='%s' (fd=%d)\n",
           user.id, user.username, c->sockfd);
}
