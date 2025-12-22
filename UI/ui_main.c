#include <gtk/gtk.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../protocol/connect.h"
#include "../protocol/frame.h"
#include "../protocol/cJSON.h"

static Connect *g_conn = NULL;
static char g_token[256] = {0};
static char g_username[128] = {0};

static GtkWidget *g_login_entry_user = NULL;
static GtkWidget *g_login_entry_pass = NULL;
static GtkWidget *g_reg_entry_user = NULL;
static GtkWidget *g_reg_entry_pass = NULL;
static GtkWidget *g_login_error_label = NULL;
static GtkWidget *g_reg_error_label = NULL;
static GtkWidget *g_main_stack = NULL;
static GtkWidget *g_status_label = NULL;
static GtkTextBuffer *g_log_buffer = NULL;

static GtkWidget *g_nav_own_button = NULL;
static GtkWidget *g_nav_shared_button = NULL;
static GtkWidget *g_nav_title_label = NULL;
static GtkWidget *g_tree_view = NULL;
static GtkListStore *g_list_store = NULL;
static int g_current_folder_id = 0;
static gboolean g_ctx_has_item = FALSE;
static gboolean g_ctx_is_folder = FALSE;
static int g_ctx_item_id = 0;

enum {
  COL_NAME = 0,
  COL_IS_FOLDER,
  COL_ID,
  NUM_COLS
};

// Forward decl
static void request_list_for_folder(int folder_id);

typedef enum {
  LOG_NORMAL,
  LOG_ERROR
} LogType;

typedef struct {
  char *text;
  LogType type;
} LogMessage;

static gboolean append_log_idle(gpointer data) {
  LogMessage *msg = (LogMessage *)data;
  if (!g_log_buffer || !msg)
    return FALSE;

  GtkTextIter end;
  gtk_text_buffer_get_end_iter(g_log_buffer, &end);

  gtk_text_buffer_insert(g_log_buffer, &end, msg->text, -1);
  gtk_text_buffer_insert(g_log_buffer, &end, "\n", -1);

  g_free(msg->text);
  g_free(msg);
  return FALSE;
}

static void append_log(const char *text, LogType type) {
  // Chỉ log lỗi, bỏ qua log thường để không spam giao diện main
  if (type != LOG_ERROR)
    return;
  LogMessage *msg = g_new0(LogMessage, 1);
  msg->text = g_strdup(text ? text : "");
  msg->type = type;
  g_idle_add(append_log_idle, msg);
}

// ==== Network callbacks (called from reader thread) ====

static void handle_generic_response(Frame *resp) {
  if (!resp)
    return;

  char info[256];
  snprintf(info, sizeof(info), "RESPOND: req_id=%u status=%d",
           resp->header.resp.request_id, resp->header.resp.status);
  append_log(info, LOG_NORMAL);

  if (resp->payload_len > 0) {
    char *payload = g_strndup((char *)resp->payload, resp->payload_len);
    append_log(payload, LOG_NORMAL);
    g_free(payload);
  }
}

static gboolean update_after_login_idle(gpointer data) {
  (void)data;
  if (g_status_label) {
    char text[256];
    if (g_username[0] != '\0') {
      snprintf(text, sizeof(text), "Logged in as %s", g_username);
    } else {
      snprintf(text, sizeof(text), "Logged in");
    }
    gtk_label_set_text(GTK_LABEL(g_status_label), text);
  }
  if (g_main_stack) {
    gtk_stack_set_visible_child_name(GTK_STACK(g_main_stack), "main");
  }
  // Load root folder list after login
  request_list_for_folder(0);
  return FALSE;
}

typedef struct {
  char *message;
  gboolean success;
} LoginUiResult;

static gboolean update_login_error_idle(gpointer data) {
  LoginUiResult *res = (LoginUiResult *)data;
  if (g_login_error_label) {
    const char *text = (res && !res->success && res->message)
                           ? res->message
                           : "";
    gtk_label_set_text(GTK_LABEL(g_login_error_label), text);
  }
  if (res) {
    g_free(res->message);
    g_free(res);
  }
  return FALSE;
}

typedef struct {
  char *message;
  gboolean success;
} RegisterUiResult;

static gboolean update_register_error_idle(gpointer data) {
  RegisterUiResult *res = (RegisterUiResult *)data;
  if (g_reg_error_label) {
    const char *text = (res && res->message) ? res->message : "";
    gtk_label_set_text(GTK_LABEL(g_reg_error_label), text);
  }
  if (res) {
    g_free(res->message);
    g_free(res);
  }
  return FALSE;
}

static void handle_login_response(Frame *resp) {
  if (!resp)
    return;

  handle_generic_response(resp);

  if (resp->header.resp.status != STATUS_OK || resp->payload_len == 0) {
    // Show error on login form
    const char *err_msg = "Login failed";
    cJSON *json = NULL;
    cJSON *err_item = NULL;

    if (resp->payload_len > 0) {
      char *payload = g_strndup((char *)resp->payload, resp->payload_len);
      if (payload) {
        json = cJSON_Parse(payload);
        g_free(payload);
      }
      if (json) {
        err_item = cJSON_GetObjectItemCaseSensitive(json, "error");
        if (cJSON_IsString(err_item) && err_item->valuestring) {
          err_msg = err_item->valuestring;
        }
      }
    }

    LoginUiResult *res = g_new0(LoginUiResult, 1);
    res->success = FALSE;
    res->message = g_strdup(err_msg);
    g_idle_add(update_login_error_idle, res);

    if (json)
      cJSON_Delete(json);
    return;
  }

  cJSON *json = cJSON_Parse((char *)resp->payload);
  if (!json)
    return;

  cJSON *token_item = cJSON_GetObjectItemCaseSensitive(json, "token");
  cJSON *user_item = cJSON_GetObjectItemCaseSensitive(json, "username");

  if (cJSON_IsString(token_item) && token_item->valuestring) {
    strncpy(g_token, token_item->valuestring, sizeof(g_token) - 1);
    g_token[sizeof(g_token) - 1] = '\0';
  }
  if (cJSON_IsString(user_item) && user_item->valuestring) {
    strncpy(g_username, user_item->valuestring, sizeof(g_username) - 1);
    g_username[sizeof(g_username) - 1] = '\0';
  }

  cJSON_Delete(json);

  // Clear previous error on login UI
  LoginUiResult *res = g_new0(LoginUiResult, 1);
  res->success = TRUE;
  res->message = NULL;
  g_idle_add(update_login_error_idle, res);

  append_log("Login success – token stored.", LOG_NORMAL);
  g_idle_add(update_after_login_idle, NULL);
}

static void handle_register_response(Frame *resp) {
  if (!resp)
    return;

  handle_generic_response(resp);

  const char *msg = NULL;
  gboolean success = FALSE;

  if (resp->header.resp.status == STATUS_OK && resp->payload_len > 0) {
    // Parse username from success response if available
    cJSON *json = cJSON_Parse((char *)resp->payload);
    cJSON *user_item = NULL;
    if (json) {
      user_item = cJSON_GetObjectItemCaseSensitive(json, "username");
      if (cJSON_IsString(user_item) && user_item->valuestring) {
        static char buf[256];
        snprintf(buf, sizeof(buf), "Register success for '%s'",
                 user_item->valuestring);
        msg = buf;
      }
      cJSON_Delete(json);
    }
    if (!msg)
      msg = "Register success";
    success = TRUE;
  } else {
    // Error case: try to extract error message and map to friendly text
    msg = "Register failed";
    if (resp->payload_len > 0) {
      char *payload = g_strndup((char *)resp->payload, resp->payload_len);
      if (payload) {
        cJSON *json = cJSON_Parse(payload);
        g_free(payload);
        if (json) {
          cJSON *err_item =
              cJSON_GetObjectItemCaseSensitive(json, "error");
          if (cJSON_IsString(err_item) && err_item->valuestring) {
            if (strcmp(err_item->valuestring, "username_already_exists") == 0) {
              msg = "Username already exists";
            } else if (strcmp(err_item->valuestring, "db_error") == 0) {
              msg = "Database error, please try again";
            } else {
              msg = err_item->valuestring;
            }
          }
          cJSON_Delete(json);
        }
      }
    }
  }

  RegisterUiResult *res = g_new0(RegisterUiResult, 1);
  res->success = success;
  res->message = g_strdup(msg);
  g_idle_add(update_register_error_idle, res);
}

// ==== Helper to send CMD ====

static void send_cmd_with_payload(const char *payload,
                                  void (*cb)(Frame *resp)) {
  if (!g_conn || !payload)
    return;

  Frame f;
  if (build_cmd_frame(&f, 0, payload) != 0) {
    append_log("Failed to build CMD frame", LOG_ERROR);
    return;
  }
  g_conn->send_cmd(g_conn, &f, cb ? cb : handle_generic_response);
}

// ==== LIST / folders handling ====

static gboolean update_list_view_from_json(cJSON *root) {
  if (!g_list_store || !root)
    return FALSE;

  cJSON *items = cJSON_GetObjectItemCaseSensitive(root, "items");
  if (!cJSON_IsArray(items))
    return FALSE;

  gtk_list_store_clear(g_list_store);

  cJSON *item = NULL;
  cJSON_ArrayForEach(item, items) {
    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(item, "name");
    cJSON *type_item = cJSON_GetObjectItemCaseSensitive(item, "type");
    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(item, "id");

    if (!cJSON_IsString(name_item) || !cJSON_IsString(type_item) ||
        !cJSON_IsNumber(id_item)) {
      continue;
    }

    const char *type = type_item->valuestring;
    gboolean is_folder = (strcmp(type, "folder") == 0);

    GtkTreeIter iter;
    gtk_list_store_append(g_list_store, &iter);
    gtk_list_store_set(g_list_store, &iter,
                       COL_NAME, name_item->valuestring,
                       COL_IS_FOLDER, is_folder,
                       COL_ID, id_item->valueint,
                       -1);
  }

  return FALSE;
}

static void handle_list_response(Frame *resp) {
  if (!resp || resp->header.resp.status != STATUS_OK || resp->payload_len == 0)
    return;

  cJSON *root = cJSON_Parse((char *)resp->payload);
  if (!root)
    return;

  cJSON *fid = cJSON_GetObjectItemCaseSensitive(root, "folder_id");
  if (cJSON_IsNumber(fid)) {
    g_current_folder_id = fid->valueint;
  }

  g_idle_add_full(G_PRIORITY_DEFAULT, (GSourceFunc)update_list_view_from_json,
                  root, (GDestroyNotify)cJSON_Delete);
}

static void handle_shared_folders_response(Frame *resp) {
  if (!resp || resp->header.resp.status != STATUS_OK || resp->payload_len == 0)
    return;

  cJSON *root = cJSON_Parse((char *)resp->payload);
  if (!root)
    return;

  cJSON *folders = cJSON_GetObjectItemCaseSensitive(root, "folders");
  if (!g_list_store || !cJSON_IsArray(folders)) {
    cJSON_Delete(root);
    return;
  }

  gtk_list_store_clear(g_list_store);

  cJSON *item = NULL;
  cJSON_ArrayForEach(item, folders) {
    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(item, "name");
    cJSON *id_item = cJSON_GetObjectItemCaseSensitive(item, "id");
    if (!cJSON_IsString(name_item) || !cJSON_IsNumber(id_item))
      continue;

    GtkTreeIter iter;
    gtk_list_store_append(g_list_store, &iter);
    gtk_list_store_set(g_list_store, &iter,
                       COL_NAME, name_item->valuestring,
                       COL_IS_FOLDER, TRUE,
                       COL_ID, id_item->valueint,
                       -1);
  }

  cJSON_Delete(root);
}

static void request_list_for_folder(int folder_id) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "cmd", "LIST");
  if (folder_id > 0) {
    cJSON_AddNumberToObject(root, "folder_id", folder_id);
  }
  char *payload = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  send_cmd_with_payload(payload, handle_list_response);
  free(payload);
}

// ==== Helpers for refreshing current folder after simple CMD ====

static gboolean refresh_current_folder_idle(gpointer data) {
  (void)data;
  request_list_for_folder(g_current_folder_id);
  return FALSE;
}

static void handle_refresh_after_ok(Frame *resp) {
  if (!resp || resp->header.resp.status != STATUS_OK)
    return;
  g_idle_add(refresh_current_folder_idle, NULL);
}

// ==== GTK signal handlers ====

static void on_register_clicked(GtkButton *button, gpointer user_data) {
  (void)button;
  (void)user_data;

  const char *user =
      gtk_entry_get_text(GTK_ENTRY(g_reg_entry_user));
  const char *pass =
      gtk_entry_get_text(GTK_ENTRY(g_reg_entry_pass));

  if (!user || !*user || !pass || !*pass) {
    // Hiển thị lỗi ngay dưới form
    if (g_reg_error_label) {
      gtk_label_set_text(GTK_LABEL(g_reg_error_label),
                         "Username and password are required");
    }
    append_log("Register: username/password required", LOG_ERROR);
    return;
  }

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "cmd", "REGISTER");
  cJSON_AddStringToObject(root, "username", user);
  cJSON_AddStringToObject(root, "password", pass);
  char *payload = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  char info[256];
  snprintf(info, sizeof(info), "Sending REGISTER for '%s'", user);
  append_log(info, LOG_NORMAL);

  // Clear previous error/success text
  if (g_reg_error_label) {
    gtk_label_set_text(GTK_LABEL(g_reg_error_label), "");
  }

  send_cmd_with_payload(payload, handle_register_response);
  free(payload);
}

static void on_login_clicked(GtkButton *button, gpointer user_data) {
  (void)button;
  (void)user_data;

  const char *user =
      gtk_entry_get_text(GTK_ENTRY(g_login_entry_user));
  const char *pass =
      gtk_entry_get_text(GTK_ENTRY(g_login_entry_pass));

  if (!user || !*user || !pass || !*pass) {
    // Hiển thị lỗi ngay dưới form
    if (g_login_error_label) {
      gtk_label_set_text(GTK_LABEL(g_login_error_label),
                         "Username and password are required");
    }
    append_log("Login: username/password required", LOG_ERROR);
    return;
  }

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "cmd", "LOGIN");
  cJSON_AddStringToObject(root, "username", user);
  cJSON_AddStringToObject(root, "password", pass);
  char *payload = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  char info[256];
  snprintf(info, sizeof(info), "Sending LOGIN for '%s'", user);
  append_log(info, LOG_NORMAL);

  send_cmd_with_payload(payload, handle_login_response);
  free(payload);
}

static void on_ping_clicked(GtkButton *button, gpointer user_data) {
  (void)button;
  (void)user_data;
  const char *payload = "{\"cmd\":\"PING\"}";
  append_log("Sending PING", LOG_NORMAL);
  send_cmd_with_payload(payload, handle_generic_response);
}

static void on_list_clicked(GtkButton *button, gpointer user_data) {
  (void)button;
  (void)user_data;
  // Refresh current folder view
  request_list_for_folder(g_current_folder_id);
}

static void on_row_activated(GtkTreeView *tree_view,
                             GtkTreePath *path,
                             GtkTreeViewColumn *column,
                             gpointer user_data) {
  (void)tree_view;
  (void)column;
  (void)user_data;

  GtkTreeIter iter;
  if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(g_list_store), &iter, path))
    return;

  gboolean is_folder = FALSE;
  int id = 0;
  gtk_tree_model_get(GTK_TREE_MODEL(g_list_store), &iter,
                     COL_IS_FOLDER, &is_folder,
                     COL_ID, &id,
                     -1);

  if (is_folder && id > 0) {
    request_list_for_folder(id);
  }
}

// ==== Folder navigation actions ====

static void on_nav_own_clicked(GtkButton *button, gpointer user_data) {
  (void)button;
  (void)user_data;

  if (g_nav_title_label) {
    gtk_label_set_text(GTK_LABEL(g_nav_title_label), "My Folders (owner)");
  }

  // Ở chế độ "My folders" ta coi như đang đứng ở root,
  // nên tạo folder mới sẽ là con trực tiếp của root (parent_id = 0).
  g_current_folder_id = 0;

  const char *payload = "{\"cmd\":\"LIST_OWN_FOLDERS\"}";
  append_log("Requesting own folders", LOG_NORMAL);
  send_cmd_with_payload(payload, handle_shared_folders_response);
}

static void on_nav_shared_clicked(GtkButton *button, gpointer user_data) {
  (void)button;
  (void)user_data;

  if (g_nav_title_label) {
    gtk_label_set_text(GTK_LABEL(g_nav_title_label), "Shared Folders");
  }

  const char *payload = "{\"cmd\":\"LIST_SHARED_FOLDERS\"}";
  append_log("Requesting shared folders", LOG_NORMAL);
  send_cmd_with_payload(payload, handle_shared_folders_response);
}

static void on_logout_clicked(GtkButton *button, gpointer user_data) {
  (void)button;
  (void)user_data;

  const char *payload = "{\"cmd\":\"LOGOUT\"}";
  append_log("Sending LOGOUT", LOG_NORMAL);
  send_cmd_with_payload(payload, handle_generic_response);

  g_token[0] = '\0';
  g_username[0] = '\0';

  if (g_status_label)
    gtk_label_set_text(GTK_LABEL(g_status_label), "Not logged in");
  if (g_main_stack)
    gtk_stack_set_visible_child_name(GTK_STACK(g_main_stack), "login");
}

// ==== Context menu actions on tree view ====

static void on_menu_new_folder(GtkMenuItem *menuitem, gpointer user_data) {
  (void)menuitem;
  (void)user_data;

  GtkWidget *parent = gtk_widget_get_toplevel(g_tree_view);
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
      "New folder",
      GTK_WINDOW(parent),
      GTK_DIALOG_MODAL,
      "_Cancel", GTK_RESPONSE_CANCEL,
      "_Create", GTK_RESPONSE_OK,
      NULL);

  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Folder name");
  gtk_box_pack_start(GTK_BOX(content), entry, TRUE, TRUE, 4);
  gtk_widget_show_all(dialog);

  int res = gtk_dialog_run(GTK_DIALOG(dialog));
  if (res == GTK_RESPONSE_OK) {
    const char *name = gtk_entry_get_text(GTK_ENTRY(entry));
    if (name && *name) {
      cJSON *root = cJSON_CreateObject();
      cJSON_AddStringToObject(root, "cmd", "MKDIR");
      cJSON_AddStringToObject(root, "name", name);
      cJSON_AddNumberToObject(root, "parent_id", g_current_folder_id);
      char *payload = cJSON_PrintUnformatted(root);
      cJSON_Delete(root);

      send_cmd_with_payload(payload, handle_refresh_after_ok);
      free(payload);
    }
  }

  gtk_widget_destroy(dialog);
}

static void on_menu_delete_folder(GtkMenuItem *menuitem, gpointer user_data) {
  (void)menuitem;
  (void)user_data;

  if (!g_ctx_has_item || !g_ctx_is_folder || g_ctx_item_id <= 0)
    return;

  GtkWidget *parent = gtk_widget_get_toplevel(g_tree_view);
  GtkWidget *dialog = gtk_message_dialog_new(
      GTK_WINDOW(parent),
      GTK_DIALOG_MODAL,
      GTK_MESSAGE_WARNING,
      GTK_BUTTONS_OK_CANCEL,
      "Delete this folder?");

  int res = gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  if (res != GTK_RESPONSE_OK)
    return;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "cmd", "DELETE_FOLDER");
  cJSON_AddNumberToObject(root, "folder_id", g_ctx_item_id);
  char *payload = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);

  send_cmd_with_payload(payload, handle_refresh_after_ok);
  free(payload);
}

static void on_menu_share_folder(GtkMenuItem *menuitem, gpointer user_data) {
  (void)menuitem;
  (void)user_data;

  if (!g_ctx_has_item || !g_ctx_is_folder || g_ctx_item_id <= 0)
    return;

  GtkWidget *parent = gtk_widget_get_toplevel(g_tree_view);
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
      "Share folder",
      GTK_WINDOW(parent),
      GTK_DIALOG_MODAL,
      "_Cancel", GTK_RESPONSE_CANCEL,
      "_Share", GTK_RESPONSE_OK,
      NULL);

  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  GtkWidget *user_entry = gtk_entry_new();
  GtkWidget *perm_entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(user_entry), "Username");
  gtk_entry_set_placeholder_text(GTK_ENTRY(perm_entry), "Permission (0-3)");
  gtk_box_pack_start(GTK_BOX(box), user_entry, TRUE, TRUE, 2);
  gtk_box_pack_start(GTK_BOX(box), perm_entry, TRUE, TRUE, 2);
  gtk_box_pack_start(GTK_BOX(content), box, TRUE, TRUE, 4);
  gtk_widget_show_all(dialog);

  int res = gtk_dialog_run(GTK_DIALOG(dialog));
  if (res == GTK_RESPONSE_OK) {
    const char *username = gtk_entry_get_text(GTK_ENTRY(user_entry));
    const char *perm_str = gtk_entry_get_text(GTK_ENTRY(perm_entry));
    if (username && *username) {
      int perm = 1;
      if (perm_str && *perm_str)
        perm = atoi(perm_str);

      cJSON *root = cJSON_CreateObject();
      cJSON_AddStringToObject(root, "cmd", "SHARE_FOLDER");
      cJSON_AddNumberToObject(root, "folder_id", g_ctx_item_id);
      cJSON_AddStringToObject(root, "username", username);
      cJSON_AddNumberToObject(root, "permission", perm);
      char *payload = cJSON_PrintUnformatted(root);
      cJSON_Delete(root);

      send_cmd_with_payload(payload, handle_generic_response);
      free(payload);
    }
  }

  gtk_widget_destroy(dialog);
}

static void on_menu_rename_item(GtkMenuItem *menuitem, gpointer user_data) {
  (void)menuitem;
  (void)user_data;

  if (!g_ctx_has_item || g_ctx_item_id <= 0)
    return;

  GtkWidget *parent = gtk_widget_get_toplevel(g_tree_view);
  GtkWidget *dialog = gtk_dialog_new_with_buttons(
      "Rename",
      GTK_WINDOW(parent),
      GTK_DIALOG_MODAL,
      "_Cancel", GTK_RESPONSE_CANCEL,
      "_Rename", GTK_RESPONSE_OK,
      NULL);

  GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget *entry = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "New name");
  gtk_box_pack_start(GTK_BOX(content), entry, TRUE, TRUE, 4);
  gtk_widget_show_all(dialog);

  int res = gtk_dialog_run(GTK_DIALOG(dialog));
  if (res == GTK_RESPONSE_OK) {
    const char *new_name = gtk_entry_get_text(GTK_ENTRY(entry));
    if (new_name && *new_name) {
      cJSON *root = cJSON_CreateObject();
      cJSON_AddStringToObject(root, "cmd", "RENAME_ITEM");
      cJSON_AddNumberToObject(root, "item_id", g_ctx_item_id);
      cJSON_AddStringToObject(root, "item_type",
                              g_ctx_is_folder ? "folder" : "file");
      cJSON_AddStringToObject(root, "new_name", new_name);
      char *payload = cJSON_PrintUnformatted(root);
      cJSON_Delete(root);

      send_cmd_with_payload(payload, handle_refresh_after_ok);
      free(payload);
    }
  }

  gtk_widget_destroy(dialog);
}

static gboolean on_tree_button_press(GtkWidget *widget,
                                     GdkEventButton *event,
                                     gpointer user_data) {
  (void)user_data;

  if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
    GtkTreePath *path = NULL;
    if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                      (gint)event->x, (gint)event->y,
                                      &path, NULL, NULL, NULL)) {
      GtkTreeIter iter;
      if (gtk_tree_model_get_iter(GTK_TREE_MODEL(g_list_store), &iter, path)) {
        gtk_tree_selection_select_path(
            gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)), path);

        gtk_tree_model_get(GTK_TREE_MODEL(g_list_store), &iter,
                           COL_IS_FOLDER, &g_ctx_is_folder,
                           COL_ID, &g_ctx_item_id,
                           -1);
        g_ctx_has_item = TRUE;
      }
      gtk_tree_path_free(path);
    } else {
      g_ctx_has_item = FALSE;
      g_ctx_is_folder = FALSE;
      g_ctx_item_id = 0;
    }

    GtkWidget *menu = gtk_menu_new();

    GtkWidget *mi_new = gtk_menu_item_new_with_label("New folder");
    g_signal_connect(mi_new, "activate",
                     G_CALLBACK(on_menu_new_folder), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi_new);

    if (g_ctx_has_item) {
      if (g_ctx_is_folder) {
        GtkWidget *mi_delete =
            gtk_menu_item_new_with_label("Delete folder");
        g_signal_connect(mi_delete, "activate",
                         G_CALLBACK(on_menu_delete_folder), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi_delete);

        GtkWidget *mi_share =
            gtk_menu_item_new_with_label("Share folder");
        g_signal_connect(mi_share, "activate",
                         G_CALLBACK(on_menu_share_folder), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi_share);
      }

      GtkWidget *mi_rename =
          gtk_menu_item_new_with_label("Rename");
      g_signal_connect(mi_rename, "activate",
                       G_CALLBACK(on_menu_rename_item), NULL);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi_rename);
    }

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
    return TRUE;
  }

  return FALSE;
}

// ==== UI construction ====

static GtkWidget *build_auth_pages(void) {
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

  GtkWidget *stack = gtk_stack_new();
  gtk_stack_set_transition_type(GTK_STACK(stack),
                                GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
  gtk_widget_set_hexpand(stack, TRUE);
  gtk_widget_set_vexpand(stack, TRUE);

  // Login page
  GtkWidget *login_grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(login_grid), 6);
  gtk_grid_set_column_spacing(GTK_GRID(login_grid), 6);

  GtkWidget *lbl_user = gtk_label_new("Username:");
  GtkWidget *lbl_pass = gtk_label_new("Password:");
  g_login_entry_user = gtk_entry_new();
  g_login_entry_pass = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(g_login_entry_pass), FALSE);
  gtk_entry_set_invisible_char(GTK_ENTRY(g_login_entry_pass), '*');

  GtkWidget *btn_login = gtk_button_new_with_label("Login");
  g_signal_connect(btn_login, "clicked",
                   G_CALLBACK(on_login_clicked), NULL);

  g_login_error_label = gtk_label_new("");
  gtk_widget_set_halign(g_login_error_label, GTK_ALIGN_START);

  gtk_grid_attach(GTK_GRID(login_grid), lbl_user, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(login_grid), g_login_entry_user, 1, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(login_grid), lbl_pass, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(login_grid), g_login_entry_pass, 1, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(login_grid), btn_login, 0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(login_grid), g_login_error_label, 0, 3, 2, 1);

  // Register page
  GtkWidget *reg_grid = gtk_grid_new();
  gtk_grid_set_row_spacing(GTK_GRID(reg_grid), 6);
  gtk_grid_set_column_spacing(GTK_GRID(reg_grid), 6);

  GtkWidget *reg_lbl_user = gtk_label_new("Username:");
  GtkWidget *reg_lbl_pass = gtk_label_new("Password:");
  g_reg_entry_user = gtk_entry_new();
  g_reg_entry_pass = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(g_reg_entry_pass), FALSE);
  gtk_entry_set_invisible_char(GTK_ENTRY(g_reg_entry_pass), '*');

  GtkWidget *btn_register = gtk_button_new_with_label("Register");
  g_signal_connect(btn_register, "clicked",
                   G_CALLBACK(on_register_clicked), NULL);

  g_reg_error_label = gtk_label_new("");
  gtk_widget_set_halign(g_reg_error_label, GTK_ALIGN_START);

  gtk_grid_attach(GTK_GRID(reg_grid), reg_lbl_user, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(reg_grid), g_reg_entry_user, 1, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(reg_grid), reg_lbl_pass, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(reg_grid), g_reg_entry_pass, 1, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(reg_grid), btn_register, 0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(reg_grid), g_reg_error_label, 0, 3, 2, 1);

  gtk_stack_add_titled(GTK_STACK(stack), login_grid, "login", "Login");
  gtk_stack_add_titled(GTK_STACK(stack), reg_grid, "register", "Register");

  GtkWidget *switcher = gtk_stack_switcher_new();
  gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher),
                               GTK_STACK(stack));

  gtk_box_pack_start(GTK_BOX(vbox), switcher, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), stack, TRUE, TRUE, 0);

  return vbox;
}

static GtkWidget *build_main_page(void) {
  GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

  // Status + control buttons
  GtkWidget *top_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  g_status_label = gtk_label_new("Not logged in");

  GtkWidget *btn_ping = gtk_button_new_with_label("Ping");
  g_signal_connect(btn_ping, "clicked",
                   G_CALLBACK(on_ping_clicked), NULL);
  GtkWidget *btn_logout = gtk_button_new_with_label("Logout");
  g_signal_connect(btn_logout, "clicked",
                   G_CALLBACK(on_logout_clicked), NULL);

  gtk_box_pack_start(GTK_BOX(top_box), g_status_label, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(top_box), btn_logout, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(top_box), btn_ping, FALSE, FALSE, 0);

  // Navigator bar (My folders / Shared)
  GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  g_nav_own_button = gtk_button_new_with_label("My folders");
  g_nav_shared_button = gtk_button_new_with_label("Shared with me");
  g_nav_title_label = gtk_label_new("My folders");

  g_signal_connect(g_nav_own_button, "clicked",
                   G_CALLBACK(on_nav_own_clicked), NULL);
  g_signal_connect(g_nav_shared_button, "clicked",
                   G_CALLBACK(on_nav_shared_clicked), NULL);

  gtk_box_pack_start(GTK_BOX(nav_box), g_nav_own_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(nav_box), g_nav_shared_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(nav_box), g_nav_title_label, FALSE, FALSE, 12);

  // List controls
  GtkWidget *list_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *entry_path = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry_path), "/");
  GtkWidget *btn_list = gtk_button_new_with_label("List path");
  g_signal_connect(btn_list, "clicked",
                   G_CALLBACK(on_list_clicked), entry_path);

  gtk_box_pack_start(GTK_BOX(list_box), entry_path, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(list_box), btn_list, FALSE, FALSE, 0);

  // File/folder list view
  GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
  g_tree_view = gtk_tree_view_new();
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *col =
      gtk_tree_view_column_new_with_attributes("Name", renderer,
                                               "text", COL_NAME, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(g_tree_view), col);

  g_list_store = gtk_list_store_new(NUM_COLS,
                                    G_TYPE_STRING,  // COL_NAME
                                    G_TYPE_BOOLEAN, // COL_IS_FOLDER
                                    G_TYPE_INT      // COL_ID
                                    );
  gtk_tree_view_set_model(GTK_TREE_VIEW(g_tree_view),
                          GTK_TREE_MODEL(g_list_store));
  g_object_unref(g_list_store);

  g_signal_connect(g_tree_view, "row-activated",
                   G_CALLBACK(on_row_activated), NULL);
  g_signal_connect(g_tree_view, "button-press-event",
                   G_CALLBACK(on_tree_button_press), NULL);

  gtk_container_add(GTK_CONTAINER(scrolled), g_tree_view);

  gtk_box_pack_start(GTK_BOX(root), top_box, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(root), nav_box, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(root), list_box, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(root), scrolled, TRUE, TRUE, 0);

  return root;
}

static void activate(GtkApplication *app, gpointer user_data) {
  (void)user_data;

  GtkWidget *window =
      gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "File Sharing Client");
  gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  g_main_stack = gtk_stack_new();
  gtk_stack_set_transition_type(GTK_STACK(g_main_stack),
                                GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);

  GtkWidget *auth_pages = build_auth_pages();
  GtkWidget *main_page = build_main_page();

  gtk_stack_add_titled(GTK_STACK(g_main_stack), auth_pages, "login", "Auth");
  gtk_stack_add_titled(GTK_STACK(g_main_stack), main_page, "main", "Main");

  GtkWidget *switcher = gtk_stack_switcher_new();
  gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher),
                               GTK_STACK(g_main_stack));

  gtk_box_pack_start(GTK_BOX(vbox), switcher, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), g_main_stack, TRUE, TRUE, 0);

  gtk_stack_set_visible_child_name(GTK_STACK(g_main_stack), "login");

  gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
  const char *server_ip = "127.0.0.1";
  int port = 5555;

  if (argc >= 3) {
    server_ip = argv[1];
    port = atoi(argv[2]);
  }

  printf("Connecting to server %s:%d\n", server_ip, port);
  g_conn = connect_create(server_ip, (uint16_t)port);
  if (!g_conn) {
    fprintf(stderr, "Failed to connect to server\n");
    return 1;
  }

  GtkApplication *app =
      gtk_application_new("com.example.fileshare", G_APPLICATION_FLAGS_NONE);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

  int status = g_application_run(G_APPLICATION(app), 0, NULL);

  g_object_unref(app);

  if (g_conn) {
    connect_destroy(g_conn);
    g_conn = NULL;
  }

  return status;
}
