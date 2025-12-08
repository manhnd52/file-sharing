// router.c
#include "router.h"
#include <stdio.h>

#define MAX_MSG_TYPE 256

static FrameHandler routes[MAX_MSG_TYPE] = {0};

void register_route(MsgType type, FrameHandler handler) {
    if(type < MAX_MSG_TYPE) {
        routes[type] = handler;
    }
}

void router_handle(Conn *sc, Frame *f) {
    printf("MSG TYPE %d\n", f->msg_type);
    if (f->msg_type < MAX_MSG_TYPE && routes[f->msg_type]) {
        routes[f->msg_type](sc, f);
    } else {
        printf("No handler for msg_type: %d\n", f->msg_type);
        // Optional: send error response?
    }
}
