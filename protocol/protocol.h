#ifndef PROTOCOL_H
#define PROTOCOL_H

// Cần khai báo các payload ở đây
#include "payload/register.h"
#include "payload/ping.h"

// Mã loại gói tin
typedef enum {
    PT_USER_REGISTER_REQ,
    PT_USER_REGISTER_RES,
    PT_PING,
    // Thêm các loại khác
} PacketType;

typedef union {
    RegisterRequestPayload register_request;
    RegisterResponsePayload register_response;
    PingPayload ping;
    // Thêm các payload khác
} PacketPayload;

// Cấu trúc gói tin chung
typedef struct {
    PacketType type;
    PacketPayload payload;
} Packet;

#endif
