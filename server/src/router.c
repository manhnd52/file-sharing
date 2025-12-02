// router.c
#include "router.h"
#include <stdio.h>

#define MAX_ROUTES 256

static PacketHandler routes[MAX_ROUTES] = {0};

void register_route(PacketType type, PacketHandler handler) {
    routes[type] = handler;
}

void handle_packet(Packet* pkt, int client_sock) {
    if (pkt->type < MAX_ROUTES && routes[pkt->type]) {
        printf("HANDLE: %d\n", pkt->type);
        routes[pkt->type](pkt, client_sock);
    } else {
        printf("Unknown packet type: %d\n", pkt->type);
        // Có thể gửi thông báo lỗi cho client
    }
}
