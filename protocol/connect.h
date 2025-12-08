#ifndef CONNECT_H
#define CONNECT_H

#include <stdint.h>
#include <pthread.h>
#include "frame.h"

#include <time.h>

#define MAX_PENDING_REQUESTS 1024
#define HEARTBEAT_INTERVAL 10
#define REQUEST_TIMEOUT 15

typedef struct Connect Connect;
typedef void (*resp_callback)(Frame *resp);

struct Connect {
    int sockfd;

    // pending requests
    struct PendingReq {
        uint32_t request_id;
        resp_callback cb;
        time_t sent_time;
        int in_use; // 0: free, 1: used
    } pending[MAX_PENDING_REQUESTS];

    pthread_mutex_t pending_lock;
    uint32_t request_counter;
    pthread_mutex_t request_lock;

    // Heartbeat
    time_t last_sent_time;
    int keep_alive; // 1: running, 0: stop
    pthread_t heartbeat_tid;

    // --- function pointers ---
    int (*send_auth)(Connect *c, Frame *f, resp_callback cb);
    int (*send_cmd)(Connect *c, Frame *f, resp_callback cb);
    int (*send_data)(Connect *c, Frame *f, resp_callback cb);

    void (*destroy)(Connect *c);
};

// --- API tạo/hủy Connect ---
Connect* connect_create(const char *host, uint16_t port);
void connect_destroy(Connect *c);

#endif
