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

int main(int argc, char *argv[]) {
  const char *server_ip = "127.0.0.1";
  int port = 5555;

  if (argc >= 3) {
    server_ip = argv[1];
    port = atoi(argv[2]);
  }

  printf("Connecting to server %s:%d\n", server_ip, port);

  Connect *conn = connect_create(server_ip, port);
  if (!conn) {
    fprintf(stderr, "Failed to connect to server\n");
    return 1;
  }

  printf("Connected successfully!\n\n");

  // ===== Bước 1: LOGIN bằng CMD frame =====
  Frame login_frame;
  cJSON *login_json = cJSON_CreateObject();
  cJSON_AddStringToObject(login_json, "cmd", "LOGIN");
  cJSON_AddStringToObject(login_json, "username", "demo");
  cJSON_AddStringToObject(login_json, "password", "demo");
  char *login_payload = cJSON_PrintUnformatted(login_json);
  build_cmd_frame(&login_frame, 0, login_payload);
  printf("Sending LOGIN CMD...\n");
  conn->send_cmd(conn, &login_frame, handle_response);
  free(login_payload);
  cJSON_Delete(login_json);
  sleep(1);

  // ===== Bước 2: Gửi CMD PING =====
  Frame ping_frame;
  const char *ping_payload = "{\"cmd\":\"PING\"}";
  build_cmd_frame(&ping_frame, 0, ping_payload);
  printf("Sending PING CMD...\n");
  conn->send_cmd(conn, &ping_frame, handle_response);
  sleep(1);

  // ===== Bước 3: Gửi CMD LIST =====
  Frame list_frame;
  cJSON *list_json = cJSON_CreateObject();
  cJSON_AddStringToObject(list_json, "cmd", "LIST");
  cJSON_AddStringToObject(list_json, "path", "/home/user/documents");
  char *list_payload = cJSON_PrintUnformatted(list_json);
  build_cmd_frame(&list_frame, 0, list_payload);
  printf("Sending LIST CMD...\n");
  conn->send_cmd(conn, &list_frame, handle_response);
  free(list_payload);
  cJSON_Delete(list_json);
  sleep(1);

  // ===== Bước 4: UPLOAD_INIT =====
  Frame up_init;
  cJSON *up_json = cJSON_CreateObject();
  cJSON_AddStringToObject(up_json, "cmd", "UPLOAD_INIT");
  cJSON_AddStringToObject(up_json, "path", "data/storage/tmp/client_test.bin");
  cJSON_AddNumberToObject(up_json, "file_size", 32);
  cJSON_AddNumberToObject(up_json, "chunk_size", 16);
  g_upload_chunk_size = 16;
  char *up_payload = cJSON_PrintUnformatted(up_json);
  build_cmd_frame(&up_init, 0, up_payload);
  printf("Sending UPLOAD_INIT...\n");
  conn->send_cmd(conn, &up_init, handle_upload_init_resp);
  free(up_payload);
  cJSON_Delete(up_json);

  // Chờ sessionId
  pthread_mutex_lock(&g_lock);
  if (!g_session_ready) {
    pthread_cond_wait(&g_cond, &g_lock);
  }
  
  int ready = g_session_ready;
  uint8_t sid_copy[SESSIONID_SIZE];
  if (ready)
    memcpy(sid_copy, g_session_id, SESSIONID_SIZE);
  pthread_mutex_unlock(&g_lock);

  if (ready) {
    // ===== Bước 5: gửi DATA chunk đầu tiên =====
    Frame data_frame;
        const char *chunk = "0123456789ABCDEF"; // 16 bytes
        size_t chunk_len = strlen(chunk);
        build_data_frame(&data_frame, 0, sid_copy, 1, g_upload_chunk_size,
          (const uint8_t *)chunk);
        printf("Sending DATA chunk_index=1 chunk_length=%u payload_len=%zu...\n",
          g_upload_chunk_size, chunk_len);
    conn->send_data(conn, &data_frame, handle_response);
  } else {
    printf("No sessionId received, skip sending DATA.\n");
  }

  printf("\nWaiting for responses...\n");
  sleep(100);

  printf("\nClosing connection...\n");
  connect_destroy(conn);
  printf("Test completed!\n");
  return 0;
}
