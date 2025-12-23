#ifndef SERVER_H
#define SERVER_H

#include "frame.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include "services/file_service.h"

#define MAX_CONNS 1024
#define BUF_SIZE 8192

typedef enum { ST_CONNECTED, ST_AUTH, ST_IDLE } ConnState;

typedef struct {
  int sockfd;
  ConnState state;
  pthread_mutex_t write_lock; // mutex bảo vệ write/send
  pthread_mutex_t read_lock;  // mutex bảo vệ read/recv
  int current_request_id;

  // Auth info
  time_t last_active;
  bool logged_in;
  uint32_t user_id;
  char auth_token[256];
  time_t auth_expiry;

  bool busy_worker;
  uint8_t buf[BUF_SIZE];
  size_t buf_len;
} Conn;

extern Conn *connections[MAX_CONNS];  // Global connection array
int server_start(int port);
int send_data(Conn *c, Frame f);
int read_data(Conn *c, Frame *f);
int send_error_response(Conn *c, uint32_t request_id,
                       const char *payload);
                       
// Yêu cầu server dừng (an toàn)
void server_stop();

#endif
