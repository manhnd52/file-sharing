#ifndef PING_HANDLER_H
#define PING_HANDLER_H
#include "server.h"
#include "frame.h"
#include <cJSON.h>

void handle_cmd_ping(Conn *c, Frame *f, const char *cmd);

#endif