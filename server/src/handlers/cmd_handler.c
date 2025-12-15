#include "handlers/cmd_handler.h"
#include "router.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

void handle_cmd_list(Conn *c, Frame *f, const char *cmd) {
    cJSON *root = cJSON_Parse((char *)f->payload);
    if (!root) {
        printf("[CMD:LIST][ERROR] Failed to parse JSON payload (fd=%d, user_id=%d)\n", c->sockfd, c->user_id);
        return;
    }
    
    cJSON *path_item = cJSON_GetObjectItem(root, "path");
    const char *path = (path_item && cJSON_IsString(path_item)) ? path_item->valuestring : "/";
        
    // Build JSON response
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "path", path);
    cJSON *files = cJSON_CreateArray();
    cJSON_AddItemToArray(files, cJSON_CreateString("file1.txt"));
    cJSON_AddItemToArray(files, cJSON_CreateString("file2.pdf"));
    cJSON_AddItemToObject(response, "files", files);
    
    char *json_resp = cJSON_PrintUnformatted(response);
    
    // Send RESPOND
    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    
    send_data(c, resp);
    
    printf("[CMD:LIST][SUCCESS] Sent file list for path='%s' (fd=%d, user_id=%d, request_id=%d)\n", 
           path, c->sockfd, c->user_id, f->header.cmd.request_id);
    
    free(json_resp);
    cJSON_Delete(response);
    cJSON_Delete(root);
}

// Handler for UPLOAD command
void handle_cmd_upload(Conn *c, Frame *f, const char *cmd) {
    printf("[CMD:UPLOAD][INFO] Processing UPLOAD request (fd=%d, user_id=%d, request_id=%d)\n", 
           c->sockfd, c->user_id, ntohl(f->header.cmd.request_id));
    
    // Parse JSON payload
    cJSON *root = cJSON_Parse((char *)f->payload);
    if (!root) {
        printf("[CMD:UPLOAD][ERROR] Failed to parse JSON payload (fd=%d, user_id=%d)\n", c->sockfd, c->user_id);
        return;
    }
    
    cJSON *filename_item = cJSON_GetObjectItem(root, "filename");
    cJSON *size_item = cJSON_GetObjectItem(root, "size");
    
    const char *filename = (filename_item && cJSON_IsString(filename_item)) ? filename_item->valuestring : "unknown";
    int file_size = (size_item && cJSON_IsNumber(size_item)) ? size_item->valueint : 0;
    
    printf("[CMD:UPLOAD][INFO] File metadata: filename='%s', size=%d bytes (fd=%d, user_id=%d)\n", 
           filename, file_size, c->sockfd, c->user_id);
    
    // Build response
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ready");
    cJSON_AddStringToObject(response, "message", "Ready to receive data");
    cJSON_AddNumberToObject(response, "chunk_size", 8192);
    
    char *json_resp = cJSON_PrintUnformatted(response);
    
    Frame resp;
    build_respond_frame(&resp, ntohl(f->header.cmd.request_id), STATUS_OK, json_resp);
    
    pthread_mutex_lock(&c->write_lock);
    send_frame(c->sockfd, &resp);
    pthread_mutex_unlock(&c->write_lock);
    
    free(json_resp);
    cJSON_Delete(response);
    cJSON_Delete(root);
}

// Handler for DOWNLOAD command
void handle_cmd_download(Conn *c, Frame *f, const char *cmd) {
    printf("[CMD:DOWNLOAD][INFO] Processing DOWNLOAD request (fd=%d, user_id=%d, request_id=%d)\n", 
           c->sockfd, c->user_id, f->header.cmd.request_id);
    
    // TODO: Parse JSON to get file path
    // TODO: Check file exists and permissions
    // TODO: Send RESPOND with file metadata
    // TODO: Start sending DATA frames
    
    Frame resp;
    const char *json_resp = "{\"status\":\"ok\",\"file_size\":1024}";
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    
    pthread_mutex_lock(&c->write_lock);
    send_frame(c->sockfd, &resp);
    pthread_mutex_unlock(&c->write_lock);
}

// Handler for PING command
void handle_cmd_ping(Conn *c, Frame *f, const char *cmd) {
    printf("[CMD:PING][INFO] PING received (fd=%d, user_id=%d, request_id=%d)\n", 
           c->sockfd, c->user_id, f->header.cmd.request_id);
    
    // Build response
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "pong");
    
    char *json_resp = cJSON_PrintUnformatted(response);
    
    Frame resp;
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    
    pthread_mutex_lock(&c->write_lock);
    send_frame(c->sockfd, &resp);
    pthread_mutex_unlock(&c->write_lock);
    
    free(json_resp);
    cJSON_Delete(response);
}

// Handler for MKDIR command
void handle_cmd_mkdir(Conn *c, Frame *f, const char *cmd) {
    printf("[CMD:MKDIR][INFO] Processing MKDIR request (fd=%d, user_id=%d, request_id=%d)\n", 
           c->sockfd, c->user_id, f->header.cmd.request_id);
    
    // TODO: Parse JSON to get directory path
    // TODO: Create directory
    // TODO: Send RESPOND
    
    Frame resp;
    const char *json_resp = "{\"status\":\"ok\",\"message\":\"Directory created\"}";
    build_respond_frame(&resp, f->header.cmd.request_id, STATUS_OK, json_resp);
    
    pthread_mutex_lock(&c->write_lock);
    send_frame(c->sockfd, &resp);
    pthread_mutex_unlock(&c->write_lock);
}
