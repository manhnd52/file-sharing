#ifndef CMD_HANDLER_H
#define CMD_HANDLER_H

#include "frame.h"
#include "server.h"

void handle_cmd_list_shared_folders(Conn *c, Frame *f);
void handle_cmd_list(Conn *c, Frame *f);
void handle_cmd_mkdir(Conn *c, Frame *f);
void handle_cmd_delete_folder(Conn *c, Frame *f);
void handle_cmd_share_folder(Conn *c, Frame *f);
void handle_cmd_rename_folder(Conn *c, Frame *f);

#endif
