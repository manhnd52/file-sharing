#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include "frame.h"
#include "server.h"

void handle_cmd_delete_file(Conn *c, Frame *f);
void handle_cmd_share_file(Conn *c, Frame *f);
void handle_cmd_rename_file(Conn *c, Frame *f);
void handle_cmd_search_files(Conn *c, Frame *f);
void handle_cmd_copy_file(Conn *c, Frame *f);

#endif
