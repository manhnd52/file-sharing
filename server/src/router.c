// router.c
#include "router.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"
#include "server.h"
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

void router_handle(Conn *c, Frame *f) {
    // AUTH guard: only allow first AUTH per connection
    if (f->msg_type == MSG_AUTH) {
        if (c->logged_in) {
            Frame resp;
            build_respond_frame(&resp, ntohl(f->header.auth.request_id), STATUS_NOT_OK,
                                "{\"error\":\"already_authed\"}");
            send_data(c, resp);
            return;
        }
        // If a dedicated AUTH handler is registered, dispatch; else reject
        if (authHandler) {
            authHandler(c, f);
        } else {
            printf("No handler for AUTH\n");
            Frame resp;
            build_respond_frame(&resp, ntohl(f->header.auth.request_id), STATUS_NOT_OK,
                                "{\"error\":\"Server Error\"}");
            send_data(c, resp);
        }
        return;
    }

    // Special handling for CMD type - route by JSON "cmd" field
    if (f->msg_type == MSG_CMD && f->payload_len > 0) {
        // Bổ sung Json parsing để lấy "cmd"
        cJSON *root = cJSON_Parse((char *)f->payload);
        
        if (c->logged_in == false) {
            Frame resp;
            build_respond_frame(&resp, ntohl(f->header.cmd.request_id), STATUS_NOT_OK,
                                "{\"error\":\"not_authenticated\"}");
            send_data(c, resp);
            cJSON_Delete(root);
            printf("CMD received before AUTH\n");
            return;
        }

        if (!root) {
            Frame resp;
            build_respond_frame(&resp, ntohl(f->header.cmd.request_id), STATUS_NOT_OK,
                                "{\"error\":\"invalid_json\"}");
            send_data(c, resp);
            cJSON_Delete(root);
            return;
        }

        char* cmd = cJSON_GetObjectItem(root, "cmd")->valuestring;
        cJSON_Delete(root);
        if (cmd) {
            printf("CMD: %s\n", cmd);
            // Find matching CMD handler
            for (int i = 0; i < cmd_route_count; i++) {
                if (strcmp(cmd_routes[i].cmd, cmd) == 0) {
                    cmd_routes[i].handler(c, f, cmd);
                    return;
                }
            }

            send_error_response(c, f->header.cmd.request_id,
                                "{\"error\":\"unknown_command\"}");
                        
            // TODO: Send error response
            return;
        }
    }
    
    // Default routing by msg_type
    if (f->msg_type < MAX_MSG_TYPE && routes[f->msg_type]) {
        routes[f->msg_type](c, f);
    } else {
        printf("No handler for msg_type: %d\n", f->msg_type);
        // Optional: send error response?
    }
}
