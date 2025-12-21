#include "connect.h"
#include "frame.h"
#include "cJSON.h"
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
static uint8_t g_session_id[SESSIONID_SIZE];
static int g_session_ready = 0;
static uint32_t g_upload_chunk_size = 0;
static char g_token[256] = {0};
static Connect *g_conn = NULL;

static int hexval(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Chuyển cặp hex thành byte
static int hexpair_to_byte(char a, char b, uint8_t *out) {
    int hi = hexval(a);
    int lo = hexval(b);
    if (hi < 0 || lo < 0) return -1;
    *out = (uint8_t)((hi << 4) | lo);
    return 0;
}

static int parse_uuid_to_bytes(const char *uuid_str, uint8_t out[SESSIONID_SIZE]) {
  if (!uuid_str)
    return -1;
  char hex[33];
  int h = 0;
  for (const char *p = uuid_str; *p && h < 32; ++p) {
    if (*p == '-')
      continue;
    hex[h++] = *p;
  }
  if (h != 32)
    return -1;
  hex[32] = '\0';
  for (int i = 0; i < 16; ++i) {
    if (hexpair_to_byte(hex[2 * i], hex[2 * i + 1], &out[i]) != 0)
      return -1;
  }
  return 0;
}

// Callback để xử lý RESPOND chung
static void print_response(Frame *resp) {
  printf("\n=== Client: Received RESPOND ===\n");
  printf("Request ID: %u\n", resp->header.resp.request_id);
  printf("Status: %d (%s)\n", resp->header.resp.status,
         resp->header.resp.status == 0 ? "OK" : "ERROR");

  if (resp->payload_len > 0) {
    printf("Payload (%zu bytes): %.*s\n", resp->payload_len,
           (int)resp->payload_len, resp->payload);

    cJSON *json = cJSON_Parse((char *)resp->payload);
    if (json) {
      char *pretty = cJSON_Print(json);
      printf("JSON Response:\n%s\n", pretty);
      free(pretty);
      cJSON_Delete(json);
    }
  }
  printf("================================\n\n");
}

// Callback dùng cho các request bình thường
static void handle_response(Frame *resp) { print_response(resp); }

// Callback dành riêng cho LOGIN để lưu token phiên
static void handle_login_response(Frame *resp) {
  print_response(resp);
  if (resp->header.resp.status != STATUS_OK || resp->payload_len == 0)
    return;

  cJSON *json = cJSON_Parse((char *)resp->payload);
  if (!json)
    return;

  cJSON *token_item = cJSON_GetObjectItemCaseSensitive(json, "token");
  if (cJSON_IsString(token_item) && token_item->valuestring) {
    strncpy(g_token, token_item->valuestring, sizeof(g_token) - 1);
    g_token[sizeof(g_token) - 1] = '\0';
    printf("Stored auth token: %s\n", g_token);
  }

  cJSON_Delete(json);
}

// Callback dành riêng cho UPLOAD_INIT để lấy sessionId
static void handle_upload_init_resp(Frame *resp) {
  print_response(resp);
  if (resp->header.resp.status != STATUS_OK)
    return;

  cJSON *json = cJSON_Parse((char *)resp->payload);
  if (!json)
    return;

  cJSON *sid = cJSON_GetObjectItemCaseSensitive(json, "sessionId");
  if (cJSON_IsString(sid) && sid->valuestring) {
    uint8_t tmp[SESSIONID_SIZE];
    if (parse_uuid_to_bytes(sid->valuestring, tmp) == 0) {
      pthread_mutex_lock(&g_lock);
      memcpy(g_session_id, tmp, SESSIONID_SIZE);
      g_session_ready = 1;
      pthread_cond_signal(&g_cond);
      pthread_mutex_unlock(&g_lock);
      printf("Stored session id for upload.\n");
    } else {
      printf("Failed to parse sessionId to bytes.\n");
    }
  }
  cJSON_Delete(json);
}

// ===== Helper CLI commands =====

static void cli_help(void) {
  printf("Available commands:\n");
  printf("  help                       - Show this help\n");
  printf("  register <user> <pass>     - Register new account\n");
  printf("  login <user> <pass>        - Login and get token\n");
  printf("  auth [token]               - Auth using token (arg or stored)\n");
  printf("  me                         - Get current user info\n");
  printf("  logout                     - Logout and invalidate token\n");
  printf("  ping                       - Send PING\n");
  printf("  list [path]                - List files at path\n");
  printf("  upload_demo                - Demo upload: INIT + one DATA chunk\n");
  printf("  token                      - Print stored auth token\n");
  printf("  exit / quit                - Exit client\n");
}

static void cli_cmd_register(const char *username, const char *password) {
  if (!username || !password) {
    printf("Usage: register <username> <password>\n");
    return;
  }

  Frame f;
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "cmd", "REGISTER");
  cJSON_AddStringToObject(json, "username", username);
  cJSON_AddStringToObject(json, "password", password);
  char *payload = cJSON_PrintUnformatted(json);

  build_cmd_frame(&f, 0, payload);
  printf("Sending REGISTER CMD for user '%s'...\n", username);
  g_conn->send_cmd(g_conn, &f, handle_response);

  free(payload);
  cJSON_Delete(json);
}

static void cli_cmd_login(const char *username, const char *password) {
  if (!username || !password) {
    printf("Usage: login <username> <password>\n");
    return;
  }

  Frame f;
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "cmd", "LOGIN");
  cJSON_AddStringToObject(json, "username", username);
  cJSON_AddStringToObject(json, "password", password);
  char *payload = cJSON_PrintUnformatted(json);

  build_cmd_frame(&f, 0, payload);
  printf("Sending LOGIN CMD for user '%s'...\n", username);
  g_conn->send_cmd(g_conn, &f, handle_login_response);

  free(payload);
  cJSON_Delete(json);
}

static void cli_cmd_auth(const char *token_arg) {
  const char *token = (token_arg && token_arg[0]) ? token_arg : g_token;
  if (!token || token[0] == '\0') {
    printf("Usage: auth <token>\n(no stored token yet)\n");
    return;
  }

  Frame f;
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "cmd", "AUTH");
  cJSON_AddStringToObject(json, "token", token);
  char *payload = cJSON_PrintUnformatted(json);

  build_cmd_frame(&f, 0, payload);
  printf("Sending AUTH CMD...\n");
  g_conn->send_cmd(g_conn, &f, handle_login_response);

  free(payload);
  cJSON_Delete(json);
}

static void cli_cmd_me(void) {
  Frame f;
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "cmd", "GET_ME");
  char *payload = cJSON_PrintUnformatted(json);

  build_cmd_frame(&f, 0, payload);
  printf("Sending GET_ME CMD...\n");
  g_conn->send_cmd(g_conn, &f, handle_response);

  free(payload);
  cJSON_Delete(json);
}

static void cli_cmd_logout(void) {
  Frame f;
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "cmd", "LOGOUT");
  char *payload = cJSON_PrintUnformatted(json);

  build_cmd_frame(&f, 0, payload);
  printf("Sending LOGOUT CMD...\n");
  g_conn->send_cmd(g_conn, &f, handle_response);

  free(payload);
  cJSON_Delete(json);
}

static void cli_cmd_ping(void) {
  Frame f;
  const char *payload = "{\"cmd\":\"PING\"}";
  build_cmd_frame(&f, 0, payload);
  printf("Sending PING CMD...\n");
  g_conn->send_cmd(g_conn, &f, handle_response);
}

static void cli_cmd_list(const char *path) {
  Frame f;
  cJSON *json = cJSON_CreateObject();
  const char *effective_path = (path && path[0]) ? path : "/";
  cJSON_AddStringToObject(json, "cmd", "LIST");
  cJSON_AddStringToObject(json, "path", effective_path);
  char *payload = cJSON_PrintUnformatted(json);

  build_cmd_frame(&f, 0, payload);
  printf("Sending LIST CMD for path '%s'...\n", effective_path);
  g_conn->send_cmd(g_conn, &f, handle_response);

  free(payload);
  cJSON_Delete(json);
}

static void cli_cmd_upload_demo(void) {
  // Reset session state
  pthread_mutex_lock(&g_lock);
  g_session_ready = 0;
  pthread_mutex_unlock(&g_lock);

  Frame up_init;
  cJSON *up_json = cJSON_CreateObject();
  cJSON_AddStringToObject(up_json, "cmd", "UPLOAD_INIT");
  cJSON_AddStringToObject(up_json, "path", "data/storage/tmp/client_cli.bin");
  cJSON_AddNumberToObject(up_json, "file_size", 32);
  cJSON_AddNumberToObject(up_json, "chunk_size", 16);
  g_upload_chunk_size = 16;
  char *up_payload = cJSON_PrintUnformatted(up_json);
  build_cmd_frame(&up_init, 0, up_payload);
  printf("Sending UPLOAD_INIT...\n");
  g_conn->send_cmd(g_conn, &up_init, handle_upload_init_resp);
  free(up_payload);
  cJSON_Delete(up_json);

  // Wait for sessionId from server
  pthread_mutex_lock(&g_lock);
  while (!g_session_ready) {
    pthread_cond_wait(&g_cond, &g_lock);
  }

  uint8_t sid_copy[SESSIONID_SIZE];
  memcpy(sid_copy, g_session_id, SESSIONID_SIZE);
  g_session_ready = 0;
  pthread_mutex_unlock(&g_lock);

  // Send first DATA chunk
  Frame data_frame;
  const char *chunk = "0123456789ABCDEF"; // 16 bytes
  size_t chunk_len = strlen(chunk);
  build_data_frame(&data_frame, 0, sid_copy, 1, g_upload_chunk_size,
                   (const uint8_t *)chunk);
  printf("Sending DATA chunk_index=1 chunk_length=%u payload_len=%zu...\n",
         g_upload_chunk_size, chunk_len);
  g_conn->send_data(g_conn, &data_frame, handle_response);
}

static void cli_print_token(void) {
  if (g_token[0] == '\0') {
    printf("No token stored.\n");
  } else {
    printf("Stored token: %s\n", g_token);
  }
}

static void trim_newline(char *s) {
  if (!s)
    return;
  size_t len = strlen(s);
  while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
    s[len - 1] = '\0';
    len--;
  }
}

static void cli_loop(void) {
  char line[512];
  printf("Type 'help' to see available commands.\n");

  while (1) {
    printf("fs-cli> ");
    fflush(stdout);

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\nEOF received, exiting.\n");
      break;
    }

    trim_newline(line);
    if (line[0] == '\0')
      continue;

    char *cmd = strtok(line, " \t");
    if (!cmd)
      continue;

    if (strcmp(cmd, "help") == 0) {
      cli_help();
    } else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
      printf("Exiting client...\n");
      break;
    } else if (strcmp(cmd, "register") == 0) {
      char *u = strtok(NULL, " \t");
      char *p = strtok(NULL, " \t");
      cli_cmd_register(u, p);
    } else if (strcmp(cmd, "login") == 0) {
      char *u = strtok(NULL, " \t");
      char *p = strtok(NULL, " \t");
      cli_cmd_login(u, p);
    } else if (strcmp(cmd, "auth") == 0) {
      char *t = strtok(NULL, "");
      cli_cmd_auth(t);
    } else if (strcmp(cmd, "me") == 0) {
      cli_cmd_me();
    } else if (strcmp(cmd, "logout") == 0) {
      cli_cmd_logout();
    } else if (strcmp(cmd, "ping") == 0) {
      cli_cmd_ping();
    } else if (strcmp(cmd, "list") == 0) {
      char *path = strtok(NULL, "");
      cli_cmd_list(path);
    } else if (strcmp(cmd, "upload_demo") == 0) {
      cli_cmd_upload_demo();
    } else if (strcmp(cmd, "token") == 0) {
      cli_print_token();
    } else {
      printf("Unknown command '%s'. Type 'help' for list.\n", cmd);
    }
  }
}


int main(int argc, char *argv[]) {
  const char *server_ip = "127.0.0.1";
  int port = 5555;

  if (argc >= 3) {
    server_ip = argv[1];
    port = atoi(argv[2]);
  }

  printf("Connecting to server %s:%d\n", server_ip, port);

  g_conn = connect_create(server_ip, port);
  if (!g_conn) {
    fprintf(stderr, "Failed to connect to server\n");
    return 1;
  }

  printf("Connected successfully!\n\n");

  cli_loop();

  printf("\nClosing connection...\n");
  connect_destroy(g_conn);
  printf("Client exited.\n");
  return 0;
}
