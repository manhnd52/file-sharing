#include "frame.h"
#include "router.h"
#include "handlers/folder_handler.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// Global connection array
Conn *connections[MAX_CONNS];

// --- Work queue for frames ---
typedef struct {
  Conn *conn;
  Frame frame;
} WorkItem;

#define QUEUE_CAPACITY 1024
static WorkItem work_queue[QUEUE_CAPACITY];
static int q_head = 0; // dequeue index
static int q_tail = 0; // enqueue index
static int q_size = 0;
static pthread_mutex_t q_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t q_not_empty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t q_not_full = PTHREAD_COND_INITIALIZER;

static bool queue_push(WorkItem item) {
  pthread_mutex_lock(&q_mutex);
  while (q_size == QUEUE_CAPACITY) {
    // queue full, wait until consumer pops
    pthread_cond_wait(&q_not_full, &q_mutex); // trong lúc chờ q_not_full, thả mutex
  }
  work_queue[q_tail] = item;
  q_tail = (q_tail + 1) % QUEUE_CAPACITY;
  q_size++;
  pthread_cond_signal(&q_not_empty);
  pthread_mutex_unlock(&q_mutex);
  return true;
}

static bool queue_pop(WorkItem *out) {
  pthread_mutex_lock(&q_mutex);
  while (q_size == 0) {
    // empty, wait for producer
    pthread_cond_wait(&q_not_empty, &q_mutex); // tương tự bên trên
  }
  *out = work_queue[q_head];
  q_head = (q_head + 1) % QUEUE_CAPACITY;
  q_size--;
  pthread_cond_signal(&q_not_full);
  pthread_mutex_unlock(&q_mutex);
  return true;
}

// --- Helper functions ---
static void conn_init(Conn *c, int sockfd) {
  c->sockfd = sockfd;
  c->state = ST_CONNECTED;
  c->current_request_id = 0;
  c->logged_in = false;
  c->user_id = 0;
  c->last_active = time(NULL);
  pthread_mutex_init(&c->write_lock, NULL);
  pthread_mutex_init(&c->read_lock, NULL);
  c->buf_len = 0;
  
  // Set socket options for reliable communication
  int opt = 1;
  setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
}

static void conn_cleanup(Conn *c) {
  if (!c)
    return;
  if (c->sockfd >= 0) {
    close(c->sockfd);
    c->sockfd = -1;
  }
  pthread_mutex_destroy(&c->write_lock);
  pthread_mutex_destroy(&c->read_lock);
  free(c);
}

// Listener thread: accept sockets and read frames, enqueue to work queue
static void *listener_thread(void *arg) {
  int listen_fd = *(int *)arg;
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
          conn_init(c, client_fd);
          connections[idx] = c;
          printf("[SERVER][INFO] New connection accepted: fd=%d, slot=%d (total_connections=%d/%d)\n", 
                 client_fd, idx, idx+1, MAX_CONNS);
        } else {
          printf("[SERVER][WARN] Connection limit reached, rejecting fd=%d (max=%d)\n",
                 client_fd, MAX_CONNS);
          close(client_fd);
        }
      }
    }

    for (int i = 0; i < MAX_CONNS; i++) {
      Conn *c = connections[i];
      if (c && FD_ISSET(c->sockfd, &read_fds)) {
        Frame f;
        int result = read_data(c, &f);
        if (result == 0) {
          // Enqueue frame for processing by worker
          WorkItem item = { .conn = c, .frame = f };
          queue_push(item);
        } else {
          printf("[SERVER][ERROR] Frame read failed: error_code=%d (fd=%d, user_id=%d)\n", 
                 result, c->sockfd, c->user_id);
          // Lỗi khi nhận frame hoặc client ngắt kết nối
          printf("[SERVER][INFO] Closing connection due to read error (fd=%d, user_id=%d)\n",
                 c->sockfd, c->user_id);
          close(c->sockfd);
          free(c);
          connections[i] = NULL;
        }
      }
    }
  }
  return NULL;
}

// Worker thread: dequeue frames and route to handlers
static void *worker_thread(void *arg) {
  (void)arg;
  while (1) {
    WorkItem item;
    if (queue_pop(&item)) {
      router_handle(item.conn, &item.frame);
    }
  }
  return NULL;
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
  // Register routes by message type

  memset(connections, 0, sizeof(connections));
  // Start listener and worker threads
  pthread_t th_listener, th_worker;
  pthread_create(&th_listener, NULL, listener_thread, &listen_fd);
  pthread_create(&th_worker, NULL, worker_thread, NULL);

  // Join threads (server runs indefinitely) → tạm dừng luồng chính
  pthread_join(th_listener, NULL);
  pthread_join(th_worker, NULL);
  return 0;
}

// Safely send, receive data
int send_data(Conn *c, Frame f) {
  pthread_mutex_lock(&c->write_lock);
  int res = send_frame(c->sockfd, &f);
  pthread_mutex_unlock(&c->write_lock);
  return res;
}

int read_data(Conn *c, Frame *f) {
  pthread_mutex_lock(&c->read_lock);
  int recv = recv_frame(c->sockfd, f);
  pthread_mutex_unlock(&c->read_lock);
  return recv;
}

int send_error_response(Conn *c, uint32_t request_id,
                       const char *payload) {
  Frame resp;
  build_respond_frame(&resp, request_id, STATUS_NOT_OK, payload);
  return send_data(c, resp);
}