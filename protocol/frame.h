#ifndef FRAME_H
#define FRAME_H

#include <stddef.h>
#include <stdint.h>

#define MAX_PAYLOAD 8192
#define AUTH_TOKEN_SIZE 16
#define SESSIONID_SIZE 16

typedef enum {
  MSG_CMD = 1,
  MSG_RESPOND = 2,
  MSG_DATA = 3,
  MSG_AUTH = 4
} MsgType;

typedef enum { STATUS_OK = 0, STATUS_NOT_OK = 1 } Status;

// --- header types ---
typedef struct {
  uint32_t request_id;
} __attribute__((packed)) CMDHeader;

typedef struct {
  uint32_t request_id;
  uint8_t status;
} __attribute__((packed)) RESPHeader;

typedef struct {
  uint32_t request_id;
  uint8_t session_id[SESSIONID_SIZE];
  uint32_t chunk_index;
  uint32_t chunk_length;
} __attribute__((packed)) DATAHeader;

typedef struct {
  uint32_t request_id;
  uint8_t auth_token[AUTH_TOKEN_SIZE];
} __attribute__((packed)) AUTHHeader;

// --- union header ---
typedef union {
  CMDHeader cmd;
  RESPHeader resp;
  DATAHeader data;
  AUTHHeader auth;
} FrameHeader;

#define LENGTH_FIELD_SIZE 4
#define MST_TYPE_SIZE 1
#define CMD_HEADER_SIZE sizeof(CMDHeader)
#define RESP_HEADER_SIZE sizeof(RESPHeader)
#define DATA_HEADER_SIZE sizeof(DATAHeader)
#define AUTH_HEADER_SIZE sizeof(AUTHHeader)

// --- frame ---
typedef struct {
  uint32_t total_length; // network byte order
  uint8_t msg_type;
  FrameHeader header; // union header
  size_t payload_len;
  uint8_t payload[MAX_PAYLOAD];
} Frame;

int get_request_id(Frame *f);
// --- build frames ---
int build_cmd_frame(Frame *f, uint32_t request_id, const char *json_payload);
int build_auth_frame(Frame *f, uint32_t request_id,
                     const uint8_t token[AUTH_TOKEN_SIZE],
                     const char *json_payload);
int build_respond_frame(Frame *f, uint32_t request_id, Status status,
                        const char *json_payload);
int build_data_frame(Frame *f, uint32_t request_id,
                     const uint8_t session_id[SESSIONID_SIZE], uint32_t chunk_index,
                     uint32_t chunk_length, const uint8_t *data);

// --- parse frame from buffer ---
int parse_frame(uint8_t *buf, size_t len, Frame *f);

// --- I/O ---
int send_frame(int sockfd, Frame *f);
int recv_frame(int sockfd, Frame *f);

#endif
