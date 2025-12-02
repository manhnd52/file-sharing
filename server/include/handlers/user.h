#ifndef USER_HANDLER_H
#define USER_HANDLER_H

#include "protocol.h"

void handle_user_registration(Packet* pkt, int client_sock);
void hangle_get_user(Packet* pkt, int client_sock);

#endif 
