#include "handlers/ping_handler.h"
#include <stdio.h>
#include "cJSON.h"
#include <stdlib.h>

// Handler for PING command
void handle_cmd_ping(Conn *c, Frame *f) {
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
