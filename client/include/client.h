#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "connect.h"
#include "frame.h"
#include "cJSON.h"

extern Connect *g_conn;

int send_cmd(cJSON *json, Frame *res);
int send_simple_cmd(const char *cmd, Frame *resp);
int client_connect(const char *host, uint16_t port, int timeout_seconds);
void client_disconnect(void);
bool client_is_connected(void);

#endif
