#ifndef CONNECT_H
#define CONNECT_H

#include <stdint.h>
#include <pthread.h>
#include "frame.h"
#include <time.h>

typedef enum {
    REQ_OK = 0,          // đã nhận response
    REQ_ERROR = -1,      // lỗi client-side chung
    REQ_NO_RESP = -2,    // quá timeout khi gửi/nhận, server disconnected → ko có phản hồi
} RequestResult;

typedef struct Connect {
    int sockfd;
    uint32_t request_counter;
    pthread_mutex_t request_lock;
    time_t last_sent_time;
    pthread_mutex_t io_lock;
    int timeout_seconds;
} Connect;

Connect* connect_create(const char *host, uint16_t port, int timeout_seconds);
void connect_destroy(Connect *c);
RequestResult connect_send_request(Connect *c, Frame *f, Frame *resp);

#endif
