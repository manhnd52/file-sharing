// user_service.h
#ifndef USER_SERVICE_H
#define USER_SERVICE_H

#include <stdbool.h>
#include "models/user.h"

int user_create(const char* username, const char* password);
User get_user_by_id(int user_id);
int user_verify_credentials(const char* username, const char* password);

#endif // USER_SERVICE_H