// router.h
#ifndef ROUTER_H
#define ROUTER_H

#include "frame.h"
#include "server.h"

// Handler function pointer
typedef void (*FrameHandler)(Conn *sc, Frame *f);

// CMD handler function pointer (for JSON command routing)
typedef void (*CMDHandler)(Conn *sc, Frame *f, const char *cmd);
typedef void (*AUTHHandler)(Conn *sc, Frame *f);

// Register a handler for a specific message type (MSG_AUTH, MSG_CMD, MSG_DATA)
void register_route(MsgType type, FrameHandler handler);

// Register a CMD handler for a specific command string
void register_cmd_route(const char *cmd, CMDHandler handler);

// Register AUTH handler
void register_auth_handler(AUTHHandler handler);

// Dispatch packet to registered handler
void router_handle(Conn *sc, Frame *f);

#endif
