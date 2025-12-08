#include "frame.h"
#include "router.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

// Global connection array
Conn *connections[MAX_CONNS];

// --- Handler functions ---
static void handle_cmd(Conn *sc, Frame *f) {
  printf("CMD received on fd %d\n", sc->sockfd);
  // PING is handled by server_conn_read_frame, so this is for other CMDs
}

static void handle_data(Conn *sc, Frame *f) {
  printf("DATA received fd=%d\n", sc->sockfd);
}

static void handle_auth(Conn *sc, Frame *f) {
  printf("AUTH received fd=%d\n", sc->sockfd);
}

// --- Dispatcher ---
void handle_frame(Conn *c, Frame *f) { router_handle(c, f); }

void main_loop(int listen_fd) {
  while (1) {
    // Các fd mà select() theo dõi
    fd_set read_fds;
    FD_ZERO(&read_fds);           // khởi tạo tập rỗng
    FD_SET(listen_fd, &read_fds); // thêm listen_fd vào tập theo dõi
    int maxfd = listen_fd;        // theo dõi fd lớn nhất

    // Thêm các kết nối hiện tại vào tập theo dõi
    for (int i = 0; i < MAX_CONNS; i++) {
      Conn *c = connections[i];
      if (c) {
        FD_SET(c->sockfd, &read_fds);
        if (c->sockfd > maxfd)
          maxfd = c->sockfd;
      }
    }

    // Chờ sự kiện trên các fd
    int ret = select(maxfd + 1, &read_fds, NULL, NULL, NULL);
    if (ret < 0)
      continue;

    // Accept new connection
    if (FD_ISSET(listen_fd, &read_fds)) {
      struct sockaddr_in cli_addr;
      socklen_t cli_len = sizeof(cli_addr);
      int client_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &cli_len);
      if (client_fd >= 0) {
        // tìm slot trống trong Connnections
        int idx = -1;
        for (int i = 0; i < MAX_CONNS; i++) {
          if (!connections[i]) {
            idx = i;
            break;
          }
        }

        if (idx >= 0) {
          Conn *c = calloc(1, sizeof(Conn));
          c->sockfd = client_fd;
          c->state = ST_CONNECTED;
          connections[idx] = c;
          printf("I/O: New connection fd=%d assigned to slot %d\n", client_fd,
                 idx);
        } else {
          printf("I/O: Max connections reached, closing client fd=%d\n",
                 client_fd);
          close(client_fd);
        }
      }
    }

    for (int i = 0; i < MAX_CONNS; i++) {
      Conn *c = connections[i];
      if (c && FD_ISSET(c->sockfd, &read_fds)) {
        Frame f;
        int result = recv_frame(c->sockfd, &f);
        if (result == 0) {
          // Frame nhận thành công
          handle_frame(c, &f);
        } else {
          printf("Result Error: %d\n", result);
          // Lỗi khi nhận frame hoặc client ngắt kết nối
          printf("I/O: recv_frame failed on fd=%d, closing connection\n",
                 c->sockfd);
          close(c->sockfd);
          free(c);
          connections[i] = NULL;
        }
      }
    }
  }
}

int server_start(int port) {
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    perror("socket");
    return -1;
  }

  int opt = 1;
  // Cho phép áp dụng lại địa chỉ đang TIME_WAIT  nhanh sau khi server restart
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);

  if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    perror("bind");
    close(listen_fd);
    return -1;
  }

  if (listen(listen_fd, 128) < 0) {
    perror("listen");
    close(listen_fd);
    return -1;
  }

  printf("Server listening on port %d\n", port);

  // Register routes
  register_route(MSG_CMD, handle_cmd);
  register_route(MSG_DATA, handle_data);
  register_route(MSG_AUTH, handle_auth);

  memset(connections, 0, sizeof(connections));
  main_loop(listen_fd);
  return 0;
}
