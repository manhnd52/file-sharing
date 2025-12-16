#include "connect.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

// --- generate request_id ---
static uint32_t generate_request_id(Connect *c){
    pthread_mutex_lock(&c->request_lock);
    uint32_t id = c->request_counter++;
    pthread_mutex_unlock(&c->request_lock);
    return id;
}

// --- async send frame ---
static int connect_send_frame(Connect *c, Frame *f, resp_callback cb){
    uint32_t rid = generate_request_id(c);

    switch(f->msg_type){
        case MSG_AUTH: f->header.auth.request_id = (rid); break;
        case MSG_CMD:  f->header.cmd.request_id = (rid); break;
        case MSG_DATA: f->header.data.request_id = (rid); break;
        default: return -1;
    }

    pthread_mutex_lock(&c->pending_lock);
    int inserted = 0;
    for(int i = 0; i < 1024; i++){
        if(!c->pending[i].in_use){
            c->pending[i].in_use = 1;
            c->pending[i].request_id = rid;
            c->pending[i].cb = cb;
            inserted = 1;
            break;
        }
    }
    pthread_mutex_unlock(&c->pending_lock);
    if(!inserted) return -1;

    // Update last sent time
    c->last_sent_time = time(NULL);

    return send_frame(c->sockfd, f);
}

// --- API function pointers ---
static int _send_auth(Connect *c, Frame *f, resp_callback cb){
    f->msg_type = MSG_AUTH;
    return connect_send_frame(c, f, cb);
}

static int _send_cmd(Connect *c, Frame *f, resp_callback cb){
    f->msg_type = MSG_CMD;
    return connect_send_frame(c, f, cb);
}

static int _send_data(Connect *c, Frame *f, resp_callback cb){
    f->msg_type = MSG_DATA;
    return connect_send_frame(c, f, cb);
}

// --- resp reader thread ---
// Thread này liên tục đọc RESPOND từ server và gọi callback tương ứng
static void *resp_reader_thread_func(void *arg){
    printf("Response reader thread started\n");

    Connect *c = (Connect*)arg;
    Frame resp;

    while(1) {
        if(recv_frame(c->sockfd, &resp) != 0){
            perror("recv_frame");
            break;
        }
        
        if(resp.msg_type != MSG_RESPOND && resp.msg_type != MSG_DATA) continue;

        uint32_t rid;
        if (resp.msg_type == MSG_RESPOND) {
            rid = resp.header.resp.request_id;
        } else {
            rid = resp.header.data.request_id;
        }

        printf("[DEBUG] Received response with request_id=%u, msg_type=%d\n", rid, resp.msg_type);

        pthread_mutex_lock(&c->pending_lock);

        int found = 0;
        for(int i=0; i < MAX_PENDING_REQUESTS; i++){
            if(c->pending[i].in_use && c->pending[i].request_id == rid){
                printf("[DEBUG] Found matching pending request at index %d\n", i);
                if(c->pending[i].cb) c->pending[i].cb(&resp);
                c->pending[i].in_use = 0;
                found = 1;
                break;
            }
        }
        if(!found) {
            printf("[DEBUG] No matching pending request found for request_id=%u\n", rid);
        }
        pthread_mutex_unlock(&c->pending_lock);
    }

    return NULL;
}

// --- heartbeat thread ---
static void *heartbeat_thread_func(void *arg){
    Connect *c = (Connect*)arg;
    while(c->keep_alive){
        sleep(1);
        time_t now = time(NULL);
        if(difftime(now, c->last_sent_time) >= HEARTBEAT_INTERVAL){
            // Send PING
            Frame f;
            // Payload: {"cmd":"PING"}
            const char *ping_json = "{\"cmd\":\"PING\"}";
            // Note: request_id will be filled by connect_send_frame
            if (build_cmd_frame(&f, generate_request_id(c), ping_json) == 0){
                _send_cmd(c, &f, NULL);
                printf("Heartbeat: PING sent\n");
            }
        }
    }
    return NULL;
}

// --- create Connect ---
Connect* connect_create(const char *host, uint16_t port){
    Connect *c = calloc(1, sizeof(Connect));
    if(!c) return NULL;

    c->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(c->sockfd < 0){ free(c); return NULL; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if(connect(c->sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0){
        perror("connect");
        close(c->sockfd);
        free(c);
        return NULL;
    }

    pthread_mutex_init(&c->pending_lock, NULL);
    pthread_mutex_init(&c->request_lock, NULL);
    c->request_counter = 1;
    c->last_sent_time = time(NULL);
    c->keep_alive = 1;

    // gán function pointer
    c->send_auth = _send_auth;
    c->send_cmd  = _send_cmd;
    c->send_data = _send_data;
    c->destroy   = connect_destroy;

    // thread đọc RESP
    pthread_t tid;
    if(pthread_create(&tid, NULL, resp_reader_thread_func, c) != 0){
        perror("pthread_create resp");
        close(c->sockfd);
        free(c);
        return NULL;
    }
    pthread_detach(tid);

    // thread heartbeat
    if(pthread_create(&c->heartbeat_tid, NULL, heartbeat_thread_func, c) != 0){
        perror("pthread_create heartbeat");
        // Non-fatal? Or fatal? Let's treat as warning but continue, or fail.
        // For now, let's just print error.
    }

    return c;
}

// --- destroy Connect ---
void connect_destroy(Connect *c){
    if(!c) return;
    
    c->keep_alive = 0;
    pthread_join(c->heartbeat_tid, NULL);

    close(c->sockfd);
    pthread_mutex_destroy(&c->pending_lock);
    pthread_mutex_destroy(&c->request_lock);
    free(c);
}
