// router.h
#ifndef ROUTER_H
#define ROUTER_H

#include "protocol.h"

// Định nghĩa hàm handler
typedef void (*PacketHandler)(Packet* pkt, int client_sock);

// Đăng ký route
void register_route(PacketType type, PacketHandler handler);

// Xử lý gói tin
void handle_packet(Packet* pkt, int client_sock);

#endif
