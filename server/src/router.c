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

void router_handle(Conn *c, Frame *req) {
    // AUTH guard: only allow first AUTH per connection
    int request_id = 0;
    switch (req->msg_type) {
        case MSG_AUTH:
            request_id = req->header.auth.request_id;
            break;
        case MSG_CMD:
            request_id = req->header.cmd.request_id;
            break;
        case MSG_DATA:
            request_id = req->header.data.request_id;
            break;
        default:
            break;
    }

    printf("Router: Handling msg_type=%d, request_id=%d\n", req->msg_type, request_id);
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
            printf("No handler for AUTH\n");
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
        
        if (c->logged_in == false) {
            Frame resp;
            build_respond_frame(&resp, request_id, STATUS_NOT_OK,
                                "{\"error\":\"not_authenticated\"}");
            send_data(c, resp);
            cJSON_Delete(root);
            printf("CMD received before AUTH\n");
            return;
        }

        if (!root) {
            Frame resp;
            build_respond_frame(&resp, ntohl(req->header.cmd.request_id), STATUS_NOT_OK,
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
                    cmd_routes[i].handler(c, req, cmd);
                    return;
                }
            }

            send_error_response(c, req->header.cmd.request_id,
                                "{\"error\":\"unknown_command\"}");
                        
            // TODO: Send error response
            return;
        }
    }
    
    // Default routing by msg_type
    if (req->msg_type < MAX_MSG_TYPE && routes[req->msg_type]) {
        routes[req->msg_type](c, req);
    } else {
        printf("No handler for msg_type: %d\n", req->msg_type);
        // Optional: send error response?
    }
}
