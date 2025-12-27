#ifndef CLIENT_AUTH_API_H
#define CLIENT_AUTH_API_H

#include "../../protocol/connect.h"
#include "../../protocol/frame.h"

int login_api(const char *username, const char *password, Frame *res);
int register_api(const char *username, const char *password, Frame *res);
int auth_api(const char *token, Frame *res);
int logout_api(Connect *conn, Frame *res);

#endif
