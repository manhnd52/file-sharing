#ifndef AUTH_API_H
#define AUTH_API_H

#include "connect.h"
#include "frame.h"

int login_api(const char *username, const char *password, Frame *res);
int register_api(const char *username, const char *password, Frame *res);
int auth_api(const char *token, Frame *res);
int logout_api(Frame *res);
int me_api(Frame *resp);

#endif
