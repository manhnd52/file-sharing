#ifndef ITEM_HANDLER_H
#define ITEM_HANDLER_H

#include "frame.h"
#include "server.h"

void handle_cmd_list_permissions(Conn *c, Frame *f);
void handle_cmd_update_permission(Conn *c, Frame *f);

#endif
