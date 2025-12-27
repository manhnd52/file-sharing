#ifndef CONNECT_H
#define CONNECT_H

#include <stdint.h>
#include <pthread.h>
#include "frame.h"
#include <time.h>

typedef struct Connect {
    int sockfd;
    uint32_t request_counter;
    pthread_mutex_t request_lock;
    time_t last_sent_time;
    int timeout_seconds;
} Connect;

Connect* connect_create(const char *host, uint16_t port, int timeout_seconds);
void connect_destroy(Connect *c);
int connect_send_request(Connect *c, Frame *f, Frame *resp);

#endif
