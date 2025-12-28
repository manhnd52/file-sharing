#ifndef CMD_HANDLER_H
#define CMD_HANDLER_H

#include "frame.h"
#include "server.h"

// Individual CMD handlers
void handle_cmd_list(Conn *c, Frame *f, const char *cmd);
void handle_cmd_upload(Conn *c, Frame *f, const char *cmd);
void handle_cmd_download(Conn *c, Frame *f, const char *cmd);
void handle_cmd_ping(Conn *c, Frame *f, const char *cmd);
void handle_cmd_mkdir(Conn *c, Frame *f, const char *cmd);
void handle_cmd_list_own_folders(Conn *c, Frame *f, const char *cmd);
void handle_cmd_list_shared_folders(Conn *c, Frame *f, const char *cmd);
void handle_cmd_delete_folder(Conn *c, Frame *f, const char *cmd);
void handle_cmd_delete_file(Conn *c, Frame *f, const char *cmd);
void handle_cmd_share_folder(Conn *c, Frame *f, const char *cmd);
void handle_cmd_share_file(Conn *c, Frame *f, const char *cmd);
void handle_cmd_list_permissions(Conn *c, Frame *f, const char *cmd);
void handle_cmd_update_permission(Conn *c, Frame *f, const char *cmd);
void handle_cmd_rename_item(Conn *c, Frame *f, const char *cmd);

#endif
