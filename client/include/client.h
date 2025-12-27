#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>
#include <pthread.h>
#include "connect.h"
#include "frame.h"

extern Connect *g_conn;

int send_cmd(cJSON *json, Frame *res);

#endif
