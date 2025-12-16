// user_service.h
#ifndef USER_SERVICE_H
#define USER_SERVICE_H

#include <stdbool.h>
#include "models/user.h"

int user_create(const char* username, const char* password);
User get_user_by_id(int user_id);
int user_verify_credentials(const char* username, const char* password);
char* user_create_session_token(int user_id, int expiry_hours);
bool user_verify_token(const char* token, int* user_id_out);

#endif // USER_SERVICE_H