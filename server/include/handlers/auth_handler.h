#ifndef AUTH_HANDLER_H
#define AUTH_HANDLER_H
#include "server.h"
#include "frame.h"
#include <stdint.h>

void handle_login(Conn *c, Frame *f);

#endif