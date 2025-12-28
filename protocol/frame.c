#include "frame.h"
#include "cJSON.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// Build CMD frame from json payload
// @param f: pointer to Frame to fill
// @param request_id: request identifier, ussually from Conn State
// @param json_payload: JSON string payload (can be NULL)
// @return 0 on success
int build_cmd_frame(Frame *f, uint32_t request_id, const char *json_payload) {
  f->msg_type = MSG_CMD;
  f->header.cmd.request_id = request_id;
  if (json_payload) {
    f->payload_len = strlen(json_payload);
    memcpy(f->payload, json_payload, f->payload_len);
  } else {
    f->payload_len = 0;
  }
    // total_length excludes the 4-byte length field; include msg_type + header + payload
    f->total_length = MST_TYPE_SIZE + CMD_HEADER_SIZE + f->payload_len; // host order
  return 0;
}

// @param f: pointer to Frame to fill
// @param request_id: request identifier, ussually from Conn State
// @param token: authentication token
// @param json_payload: JSON string payload (can be NULL - optional)
// @return 0 on success
int build_auth_frame(Frame *f, uint32_t request_id,
                     const uint8_t token[AUTH_TOKEN_SIZE],
                     const char *json_payload) {
  f->msg_type = MSG_AUTH;
  f->header.auth.request_id = request_id;
  memcpy(f->header.auth.auth_token, token, AUTH_TOKEN_SIZE);
  if (json_payload) {
    f->payload_len = strlen(json_payload);
    memcpy(f->payload, json_payload, f->payload_len);
  } else
    f->payload_len = 0;
  // total_length excludes the 4-byte length field; include msg_type + header + payload
  f->total_length = MST_TYPE_SIZE + AUTH_HEADER_SIZE + f->payload_len; // host order
  return 0;
}

// @param f: pointer to Frame to fill
// @param request_id: request identifier, ussually from Conn State
// @param status: Status code (0: OK, 1: NOT OK)
// @param json_payload: JSON string payload (can be NULL - optional)
// @return 0 on success
int build_respond_frame(Frame *f, uint32_t request_id, Status status,
                        const char *json_payload) {
  f->msg_type = MSG_RESPOND;
  f->header.resp.request_id = request_id;
  f->header.resp.status = (uint8_t)status;
  if (json_payload) {
    f->payload_len = strlen(json_payload);
    memcpy(f->payload, json_payload, f->payload_len);
  } else {
    f->payload_len = 0;
  }
    // total_length excludes the 4-byte length field; include msg_type + header + payload
    f->total_length = MST_TYPE_SIZE + RESP_HEADER_SIZE + f->payload_len; // host order
  return 0;
}

// @param f: pointer to Frame to fill
// @param request_id: request identifier, ussually from Conn State
// @param file_id: file identifier
// @param chunk_index: index of chunk
// @param chunk_length: length of chunk data
// @param flag: flags
// @param data: pointer to chunk data
// @return 0 on success
int build_data_frame(Frame *f, uint32_t request_id,
                     const uint8_t session_id[SESSIONID_SIZE], uint32_t chunk_index,
                     uint32_t chunk_length, const uint8_t *data) {
  f->msg_type = MSG_DATA;
  f->header.data.request_id = request_id;
  memcpy(f->header.data.session_id, session_id, SESSIONID_SIZE);
  f->header.data.chunk_index = htonl(chunk_index);
  f->header.data.chunk_length = htonl(chunk_length);
  if (data && chunk_length > 0) {
    f->payload_len = chunk_length;
    memcpy(f->payload, data, chunk_length);
  } else
    f->payload_len = 0;
  // total_length excludes the 4-byte length field; include msg_type + header + payload
  f->total_length = MST_TYPE_SIZE + DATA_HEADER_SIZE + f->payload_len; // host order
  return 0;
}

int print_frame(Frame *f) {
  if (!f) {
    return -1;
  }

  printf("[FRAME] total_length=%u msg_type=%u payload_len=%zu\n", f->total_length,
         f->msg_type, f->payload_len);
  switch (f->msg_type) {
  case MSG_CMD:
    printf("  CMD request_id=%u\n", f->header.cmd.request_id);
    break;
  case MSG_RESPOND:
    printf("  RESPOND request_id=%u status=%s\n", f->header.resp.request_id,
           f->header.resp.status == STATUS_OK ? "OK" : "NOT_OK");
    break;
  case MSG_DATA: {
    char session_hex[SESSIONID_SIZE * 2 + 1] = {0};
    for (int i = 0; i < SESSIONID_SIZE; ++i) {
      snprintf(session_hex + i * 2, 3, "%02x", f->header.data.session_id[i]);
    }
    printf("  DATA request_id=%u chunk_index=%u chunk_length=%u session_id=%s\n",
           f->header.data.request_id, f->header.data.chunk_index,
           f->header.data.chunk_length, session_hex);
    break;
  }
  case MSG_AUTH:
    printf("  AUTH request_id=%u\n", f->header.auth.request_id);
    break;
  default:
    printf("  <unknown frame type>\n");
    return -1;
  }

  if (f->payload_len > 0) {
    if (f->msg_type == MSG_CMD || f->msg_type == MSG_RESPOND) {
      char *payload_copy = (char *)malloc(f->payload_len + 1);
      if (payload_copy) {
        memcpy(payload_copy, f->payload, f->payload_len);
        payload_copy[f->payload_len] = '\0';
        cJSON *root = cJSON_Parse(payload_copy);
        if (root) {
          char *pretty = cJSON_Print(root);
          if (pretty) {
            printf("  payload JSON:\n%s\n", pretty);
            cJSON_free(pretty);
          } else {
            printf("  payload JSON: %s\n", payload_copy);
          }
          cJSON_Delete(root);
        } else {
          printf("  payload JSON: %s\n", payload_copy);
        }
        free(payload_copy);
      }
    } else {
      size_t len = f->payload_len;
      size_t preview = len > 64 ? 64 : len;
      printf("  payload (%zu bytes): ", len);
      for (size_t i = 0; i < preview; ++i) {
        printf("%02x", f->payload[i]);
      }
      if (preview < len) {
        printf("...");
      }
      printf("\n");
    }
  }

  return 0;
}

int get_request_id(Frame *f) {
  if (!f) return -1;
  switch (f->msg_type) {
    case MSG_CMD:
      return f->header.cmd.request_id;
    case MSG_RESPOND:
      return f->header.resp.request_id;
    case MSG_DATA:
      return f->header.data.request_id;
    case MSG_AUTH:
      return f->header.auth.request_id;
    default:
      return -1;
  }
}
// --- parse frame ---
int parse_frame(uint8_t *buf, size_t len, Frame *f) {
  if (len < MST_TYPE_SIZE)
    return -1;
  f->msg_type = buf[0];
  int header_size = 0;
  switch (f->msg_type) {
  case MSG_CMD:
    header_size = CMD_HEADER_SIZE;
    break;
  case MSG_RESPOND:
    header_size = RESP_HEADER_SIZE;
    break;
  case MSG_DATA:
    header_size = DATA_HEADER_SIZE;
    break;
  case MSG_AUTH:
    header_size = AUTH_HEADER_SIZE;
    break;
  default:
    return -3; // unknown type
  }
  if (len < MST_TYPE_SIZE + header_size)
    return -2;
  f->total_length = len;  // len is in host byte order, we store it as-is
  f->msg_type = buf[0];
  memcpy(&f->header, buf + MST_TYPE_SIZE, header_size);
  
  switch (f->msg_type) {
    case MSG_CMD:
      f->header.cmd.request_id = ntohl(f->header.cmd.request_id);
      break;
    case MSG_RESPOND: 
      f->header.resp.request_id = ntohl(f->header.resp.request_id);
      break;
    case MSG_DATA:
      f->header.data.request_id = ntohl(f->header.data.request_id);
      f->header.data.chunk_index = ntohl(f->header.data.chunk_index);
      f->header.data.chunk_length = ntohl(f->header.data.chunk_length);
      break;
    case MSG_AUTH:
      f->header.auth.request_id = ntohl(f->header.auth.request_id);
      break;
    default:
      return -3; // unknown type
  }

  f->payload_len = len - MST_TYPE_SIZE - header_size;
  if (f->payload_len > 0)
    memcpy(f->payload, buf + MST_TYPE_SIZE + header_size, f->payload_len);
  return 0;
}

// --- I/O Helpers ---
ssize_t send_all(int sockfd, const void *buf, size_t len) {
  size_t total = 0;
  const uint8_t *p = buf;
  while (total < len) {
    ssize_t n = send(sockfd, p + total, len - total, 0);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      perror("send");
      return -1;
    }
    total += n;
  }
  return total;
}

ssize_t read_exact(int sockfd, void *buf, size_t len) {
  size_t total = 0;
  uint8_t *p = buf;
  while (total < len) {
    ssize_t n = recv(sockfd, p + total, len - total, 0);
    if (n <= 0) {
      if (n < 0 && errno == EINTR)
        continue;
      // Don't perror here to allow caller to handle disconnect gracefully
      return -1;
    }
    total += n;
  }
  return total;
}

// --- Send Frame ---
int send_frame(int sockfd, Frame *f) {
  uint32_t net_len = htonl(f->total_length); // convert to network order on send
  if (send_all(sockfd, &net_len, sizeof(net_len)) != sizeof(net_len))
    return -1;
  if (send_all(sockfd, &f->msg_type, sizeof(f->msg_type)) !=
      sizeof(f->msg_type))
    return -1;

  size_t header_size = 0;
  void *header_ptr = NULL;

  switch (f->msg_type) {
  case MSG_CMD:
    header_size = sizeof(f->header.cmd);
    header_ptr = &f->header.cmd;
    f->header.cmd.request_id = htonl(f->header.cmd.request_id);
    break;
  case MSG_RESPOND:
    header_size = sizeof(f->header.resp);
    header_ptr = &f->header.resp;
    f->header.resp.request_id = htonl(f->header.resp.request_id);
    break;
  case MSG_DATA:
    header_size = sizeof(f->header.data);
    header_ptr = &f->header.data;
    f->header.data.request_id = htonl(f->header.data.request_id);
    break;
  case MSG_AUTH:
    header_size = sizeof(f->header.auth);
    header_ptr = &f->header.auth;
    f->header.auth.request_id = htonl(f->header.auth.request_id);
    break;
  default:
    return -1;
  }

  if (send_all(sockfd, header_ptr, header_size) != header_size)
    return -1;

  if (f->payload_len > 0)
    if (send_all(sockfd, f->payload, f->payload_len) != f->payload_len)
      return -1;
  return 0;
}

// --- Recv Frame ---
int recv_frame(int sockfd, Frame *f) {
  uint32_t length;
  if (read_exact(sockfd, &length, LENGTH_FIELD_SIZE) != LENGTH_FIELD_SIZE) {
    printf("Failed to read length field\n");
    return -1;
  }

  f->total_length = ntohl(length);

  // Safety check for max payload
  if (f->total_length > MAX_PAYLOAD + 100) {
    printf("[FRAME] Frame too large: %u\n", f->total_length);
    return -2; // Too large
  }
  uint8_t *buf = malloc(f->total_length); // chứa type, header + payload
  if (!buf) {
    printf("[FRAME] Failed to allocate memory for frame buffer\n");
    return -1;
  }

  if (read_exact(sockfd, buf, f->total_length) != f->total_length) {
    printf("[FRAME] Failed to read frame data\n");
    free(buf);
    return -1;
  }

  if (parse_frame(buf, f->total_length, f) != 0) {
    printf("Failed to parse frame\n");
    free(buf);
    return -1;
  }

  free(buf);
  return 0;
}
