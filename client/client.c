#include "connect.h"
#include "frame.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

// Callback để xử lý RESPOND từ server
void handle_response(Frame *resp) {
  printf("\n=== Client: Received RESPOND ===\n");
  printf("Request ID: %u\n", ntohl(resp->header.resp.request_id));
  printf("Status: %d (%s)\n", resp->header.resp.status, 
         resp->header.resp.status == 0 ? "OK" : "ERROR");
  
  if (resp->payload_len > 0) {
    printf("Payload (%zu bytes): %.*s\n", resp->payload_len, 
           (int)resp->payload_len, resp->payload);
    
    // Try parse as JSON
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

// Callback để xử lý DATA từ server (trong trường hợp download)
void handle_data_response(Frame *resp) {
  printf("Client: Received DATA response, request_id=%u, chunk_index=%u, "
         "chunk_length=%u\n",
         ntohl(resp->header.data.request_id),
         ntohl(resp->header.data.chunk_index),
         ntohl(resp->header.data.chunk_length));
}

int main(int argc, char *argv[]) {
  const char *server_ip = "127.0.0.1";
  int port = 5555;

  if (argc >= 3) {
    server_ip = argv[1];
    port = atoi(argv[2]);
  }

  printf("Connecting to server %s:%d\n", server_ip, port);

  // Tạo kết nối
  Connect *conn = connect_create(server_ip, port);
  if (!conn) {
    fprintf(stderr, "Failed to connect to server\n");
    return 1;
  }

  printf("Connected successfully!\n\n");

  // ===== Test 1: Gửi CMD frame (LIST) =====
  printf("\n>>> Test 1: Sending LIST command\n");
  Frame cmd_frame;
  
  // Build JSON payload with cJSON
  cJSON *cmd_json = cJSON_CreateObject();
  cJSON_AddStringToObject(cmd_json, "cmd", "LIST");
  cJSON_AddStringToObject(cmd_json, "path", "/home/user/documents");
  char *cmd_payload = cJSON_PrintUnformatted(cmd_json);
  
  build_cmd_frame(&cmd_frame, 0, cmd_payload);
  printf("CMD Length: %u bytes\n", cmd_frame.total_length);
  printf("CMD Payload: %s\n", cmd_payload);
  
  if (conn->send_cmd(conn, &cmd_frame, handle_response) == 0) {
    printf("✓ CMD sent successfully\n");
  } else {
    printf("✗ Failed to send CMD\n");
  }
  
  free(cmd_payload);
  cJSON_Delete(cmd_json);
  sleep(2);

  // ===== Test 2: Gửi DATA frame =====
  Frame data_frame;
  uint8_t file_id[FILEID_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  uint8_t chunk_data[] = "This is chunk data for upload";
  build_data_frame(&data_frame, 0, file_id, 0, strlen((char*)chunk_data),
                   chunk_data);
  if (conn->send_data(conn, &data_frame, handle_response) == 0) {
    printf("DATA sent successfully\n");
  } else {
    printf("Failed to send DATA\n");
  }
  sleep(1);

  // ===== Test 3: Gửi AUTH frame =====
  Frame auth_frame;
  uint8_t auth_token[AUTH_TOKEN_SIZE] = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
  };
  const char *auth_payload = "{\"username\":\"user1\",\"password\":\"pass123\"}";
  build_auth_frame(&auth_frame, 0, auth_token, auth_payload);
  if (conn->send_auth(conn, &auth_frame, handle_response) == 0) {
    printf("AUTH sent successfully\n");
  } else {
    printf("Failed to send AUTH\n");
  }
  sleep(2);

  printf("\nWaiting for responses...\n");
  sleep(3);

  // Cleanup
  printf("\nClosing connection...\n");
  connect_destroy(conn);
  printf("Test completed!\n");
  scanf("%*s");
  return 0;
}
