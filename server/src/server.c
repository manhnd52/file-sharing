#include "server.h"
#include "router.h"
#include "protocol.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BACKLOG 10

void server_start(int port) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Tạo socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket()");
        return;
    }

    // Bind
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind()");
        close(server_fd);
        return;
    }

    // Listen
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen()");
        close(server_fd);
        return;
    }

    printf("[INFO] Server listening on port %d...\n", port);

    // Accept loop
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept()");
            continue;
        }

        // Nhận gói tin
        Packet pkt;
        int bytes = recv(client_fd, &pkt, sizeof(Packet), 0);
        if (bytes <= 0) {
            printf("[WARN] Client disconnected\n");
            close(client_fd);
            continue;
        }

        // Route đến handler tương ứng
        handle_packet(&pkt, client_fd);

        close(client_fd);
    }

    close(server_fd);
}
