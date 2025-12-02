#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../protocol/protocol.h"
#include "../protocol/payload/register.h"

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5555);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    // Cách khai báo nhanh gói tin
    Packet pkt = {
        .type = PT_USER_REGISTER_REQ,
        .payload.register_request = {
            .username = "testuser1",
            .password = "password123",
        }
    };

    write(sock, &pkt, sizeof(pkt));

    Packet res;
    read(sock, &res, sizeof(res));
    RegisterResponsePayload res_payload = res.payload.register_response;
    printf("Server response: %d\n", res_payload.success);

    close(sock);
}
