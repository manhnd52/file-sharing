#ifndef CLIENT_FILE_API_H
#define CLIENT_FILE_API_H

#include "connect.h"
#include "frame.h"

int file_api_list(Connect *conn, const char *path, Frame *resp);
int file_api_me(Connect *conn, Frame *resp);
int file_api_ping(Connect *conn, Frame *resp);

#endif
