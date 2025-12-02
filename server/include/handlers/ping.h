#ifndef PING_HANDLER_H
#define PING_HANDLER_H

#include "protocol.h"

void handle_ping(Packet* pkt, int client_sock);

#endif
