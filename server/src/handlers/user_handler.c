#include "handlers/user.h"
#include "services/user_service.h"
#include "payload/register.h"
#include <string.h>  // memcpy
#include <unistd.h>  // write

void handle_user_registration(Packet* pkt, int client_sock) {
    RegisterRequestPayload req_payload = pkt->payload.register_request;
    if (!req_payload.username || !req_payload.password) {
        return;
    }
    
    int user_id = user_create(req_payload.username, req_payload.password);
    
    Packet res;
    res.type = PT_USER_REGISTER_RES;
    
    RegisterResponsePayload res_payload;
    res_payload.success = (user_id != 0);

    memcpy(&res.payload.register_response, &res_payload, sizeof(res_payload));

    write(client_sock, &res, sizeof(res));
}

void hangle_get_user(Packet* pkt, int client_sock) {
    // Xử lý lấy thông tin user
}

