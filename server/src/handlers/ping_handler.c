#include "handlers/ping.h"
#include <string.h>
#include <unistd.h>
#include <time.h>

void handle_ping(Packet* pkt, int client_sock) {
    Packet res;
    res.type = PT_PING;
    // Lấy thời gian hiện tại
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
    PingPayload* p = &res.payload.ping;
    strcpy(p->message, "NOW:");
    strcat(p->message, time_str);
    
    write(client_sock, &res, sizeof(res));
}
