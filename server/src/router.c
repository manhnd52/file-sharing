// router.c
#include "router.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "cJSON.h"
#include "server.h"
#include "handlers/upload_handler.h"

#define MAX_MSG_TYPE 256
#define MAX_CMD_ROUTES 64

static FrameHandler routes[MAX_MSG_TYPE] = {0};

// CMD routing table
typedef struct {
    char cmd[32];
    CMDHandler handler;
} CMDRoute;

static int cmd_route_count = 0;
static CMDRoute cmd_routes[MAX_CMD_ROUTES];

static AUTHHandler authHandler = NULL;

void register_route(MsgType type, FrameHandler handler) {
    if(type < MAX_MSG_TYPE) {
        routes[type] = handler;
    }
}

void register_auth_handler(AUTHHandler handler) {
    authHandler = handler;
}


void register_cmd_route(const char *cmd, CMDHandler handler) {
    if (cmd_route_count < MAX_CMD_ROUTES) {
        strncpy(cmd_routes[cmd_route_count].cmd, cmd, 31);
        cmd_routes[cmd_route_count].cmd[31] = '\0';
        cmd_routes[cmd_route_count].handler = handler;
        cmd_route_count++;
    }
}

void router_handle(Conn *c, Frame *req) {
    // AUTH guard: only allow first AUTH per connection
    int request_id = get_request_id(req);
    
    printf("[ROUTER][DEBUG] Received frame: msg_type=%d, request_id=%d (fd=%d, user_id=%d, logged_in=%d)\n", 
           req->msg_type, request_id, c->sockfd, c->user_id, c->logged_in);
    
    sleep(10000000); //Debug: delete this after fix

    if (req->msg_type == MSG_AUTH) {
        if (c->logged_in) {
            Frame resp;
            build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                                "{\"error\":\"already_authed\"}");
            send_data(c, resp);
            return;
        }
        // If a dedicated AUTH handler is registered, dispatch; else reject
        if (authHandler) {
            authHandler(c, req);
        } else {
            printf("[ROUTER][ERROR] No AUTH handler registered (fd=%d, request_id=%d)\n", c->sockfd, request_id);
            Frame resp;
            build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                                "{\"error\":\"Server Error\"}");
            send_data(c, resp);
        }
        return;
    }

    // Special handling for CMD type - route by JSON "cmd" field
    if (req->msg_type == MSG_CMD && req->payload_len > 0) {
        // Bổ sung Json parsing để lấy "cmd"
        cJSON *root = cJSON_Parse((char *)req->payload);
        
        if (!root) {
            Frame resp;
            build_respond_frame(&resp, req->header.cmd.request_id, STATUS_NOT_OK,
                                "{\"error\":\"invalid_json\"}");
            send_data(c, resp);
            return;
        }

        cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
        if (!cmd_item || !cJSON_IsString(cmd_item)) {
            Frame resp;
            build_respond_frame(&resp, req->header.cmd.request_id, STATUS_NOT_OK,
                                "{\"error\":\"missing_cmd_field\"}");
            send_data(c, resp);
            cJSON_Delete(root);
            return;
        }
        
        char* cmd = cmd_item->valuestring;
        printf("[ROUTER][INFO] CMD request: cmd='%s' (fd=%d, user_id=%d, request_id=%d) \n", 
               cmd, c->sockfd, c->user_id, request_id);
        
        // Allow LOGIN, REGISTER and AUTH commands without prior authentication
        if (strcmp(cmd, "LOGIN") != 0 &&
            strcmp(cmd, "REGISTER") != 0 &&
            strcmp(cmd, "AUTH") != 0 &&
            c->logged_in == false) {
            
            Frame resp;
            build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                                "{\"error\":\"not_authenticated\"}");
            send_data(c, resp);
            cJSON_Delete(root);
            printf("[ROUTER][WARN] Unauthenticated CMD attempt: cmd='%s' (fd=%d, request_id=%d)\n", 
                   cmd, c->sockfd, request_id);
            return;
        }
        
        if (cmd) {
            printf("[ROUTER][DEBUG] Routing CMD='%s' to handler (fd=%d, user_id=%d)\n", 
                   cmd, c->sockfd, c->user_id);
            // Find matching CMD handler
            for (int i = 0; i < cmd_route_count; i++) {
                if (strcmp(cmd_routes[i].cmd, cmd) == 0) {
                    cmd_routes[i].handler(c, req, cmd);
                    return;
                }
            }

            send_error_response(c, req->header.cmd.request_id,
                                "{\"error\":\"unknown_command\"}");
                        
            return;
        }

        cJSON_Delete(root);
        

    }

    if (req->msg_type == MSG_DATA) {
        if (c->logged_in == false) {
            Frame resp;
            build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                                "{\"error\":\"not_authenticated\"}");
            send_data(c, resp);
            printf("[ROUTER][WARN] Unauthenticated DATA frame attempt (fd=%d, request_id=%d)\n", 
                   c->sockfd, request_id);
            return;
        }
        upload_handler(c, req);
        return;
    }

    return;
}
