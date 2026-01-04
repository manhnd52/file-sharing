#include "connect.h"
#include "frame.h"
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

static uint32_t generate_request_id(Connect *c) {
    pthread_mutex_lock(&c->request_lock);
    uint32_t rid = c->request_counter++;
    pthread_mutex_unlock(&c->request_lock);
    return rid;
}

RequestResult connect_send_request(Connect *c, Frame *req, Frame *resp) {
    if (!c || !req || !resp) {
        return REQ_ERROR;
    }

    uint32_t rid = generate_request_id(c);
    switch (req->msg_type) {
        case MSG_AUTH:
            req->header.auth.request_id = rid;
            break;
        case MSG_CMD:
            req->header.cmd.request_id = rid;
            break;
        case MSG_DATA:
            req->header.data.request_id = rid;
            break;
        default:
            return REQ_ERROR;
    }
    pthread_mutex_lock(&c->io_lock);
    if (send_frame(c->sockfd, req) != 0) {
        return REQ_NO_RESP;
    }

    // puts("SENTED: ");
    // print_frame(req);

    c->last_sent_time = time(NULL);

    while (1) {
        if (recv_frame(c->sockfd, resp) != 0) {
            pthread_mutex_unlock(&c->io_lock);
            return REQ_NO_RESP;
        }
        uint32_t resp_id = (uint32_t)get_request_id(resp);
        // Ignore unexpected frames and keep waiting for the matching response.
        if (resp_id == rid) {
            //puts("RECV:");
            //print_frame(resp);
            pthread_mutex_unlock(&c->io_lock);
            return REQ_OK;
        }
    }
}

Connect* connect_create(const char *host, uint16_t port, int timeout_seconds) {
    Connect *c = calloc(1, sizeof(Connect));
    if (!c) {
        return NULL;
    }

    c->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (c->sockfd < 0) {
        free(c);
        return NULL;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(c->sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        perror("connect");
        close(c->sockfd);
        free(c);
        return NULL;
    }

    // ignore SIGPIPE to avoid crash when peer socket is closed
    signal(SIGPIPE, SIG_IGN); 
    pthread_mutex_init(&c->request_lock, NULL);
    c->request_counter = 1;
    c->last_sent_time = time(NULL);
    c->timeout_seconds = timeout_seconds;

    if (timeout_seconds > 0) {
        // Set socket timeout
        struct timeval tv = {0};
        tv.tv_sec = timeout_seconds;
        tv.tv_usec = 0;
        setsockopt(c->sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    }

    return c;
}

void connect_destroy(Connect *c) {
    if (!c) {
        return;
    }
    close(c->sockfd);
    pthread_mutex_destroy(&c->request_lock);
    free(c);
}
