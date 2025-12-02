// handlers/auth.c
#include "protocol.h"
#include <stdio.h>
#include <string.h>

void handle_login(Packet* pkt, int client_sock) {
    printf("Login request: %s\n", pkt->payload);
    // Xử lý login, gửi phản hồi client
}
