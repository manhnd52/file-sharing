// router.h
#ifndef ROUTER_H
#define ROUTER_H

#include "frame.h"
#include "server.h"
// Handler function pointer
typedef void (*FrameHandler)(Conn *sc, Frame *f);

// Register a handler for a specific message type (MSG_AUTH, MSG_CMD, MSG_DATA)
void register_route(MsgType type, FrameHandler handler);

// Dispatch packet to registered handler
void router_handle(Conn *sc, Frame *f);

#endif
