#ifndef AUTH_HANDLER_H
#define AUTH_HANDLER_H
#include "server.h"
#include "frame.h"
#include <stdint.h>
#include "services/file_service.h"

void handle_login(Conn *c, Frame *f, const char *cmd);
void handle_register(Conn *c, Frame *f, const char *cmd);
void handle_auth_token(Conn *c, Frame *f, const char *cmd);
void handle_logout(Conn *c, Frame *f, const char *cmd);
void handle_get_me(Conn *c, Frame *f, const char *cmd);

#endif
