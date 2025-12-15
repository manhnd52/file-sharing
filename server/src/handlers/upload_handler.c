#include "handlers/upload_handler.h"
#include "frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

UploadSession ss[MAX_SESSION];

static int bytes_to_uuid_string(const uint8_t id[SESSIONID_SIZE], char out[37]) {
	// Format as UUID v4 style: 8-4-4-4-12 from 16 bytes
	// If bytes already a UUID, this will render correctly; otherwise deterministic formatting.
	static const int idxs[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
	// snprintf into out
	int n = snprintf(out, 37,
									 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
									 id[idxs[0]], id[idxs[1]], id[idxs[2]], id[idxs[3]],
									 id[idxs[4]], id[idxs[5]],
									 id[idxs[6]], id[idxs[7]],
									 id[idxs[8]], id[idxs[9]],
									 id[idxs[10]], id[idxs[11]], id[idxs[12]], id[idxs[13]], id[idxs[14]], id[idxs[15]]);
	return (n == 36) ? 0 : -1;
}

static int ensure_tmp_dir() {
	struct stat st;
	if (stat("data/storage/tmp", &st) == 0) {
		if (S_ISDIR(st.st_mode)) return 0;
		errno = ENOTDIR;
		return -1;
	}
	return mkdir("data/storage/tmp", 0755);
}

static int find_session(const uint8_t sid[SESSIONID_SIZE], UploadSession **out) {
	// find existing
	for (int i = 0; i < MAX_SESSION; ++i) {
		if (memcmp(ss[i].session_id, sid, SESSIONID_SIZE) == 0 && ss[i].filepath[0] != '\0') {
			*out = &ss[i];
			return 0;
		}
	}
	return -1;
}

// Generate random session ID ~ UUID ở dạng byte array
static int generate_session_id(uint8_t out[SESSIONID_SIZE]) {
	int fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0) return -1;
	ssize_t r = read(fd, out, SESSIONID_SIZE);
	close(fd);
	return (r == SESSIONID_SIZE) ? 0 : -1;
}

/*
Handler for received DATA frames (file upload chunks)
*/
void data_handler(Conn *c, Frame *data) {
	if (!c || !data) return;
	if (data->msg_type != MSG_DATA) return;

	printf("[UPLOAD:DATA][INFO] Received chunk: request_id=%u, chunk_index=%u, chunk_length=%u, payload_len=%zu (fd=%d, user_id=%d)\n",
	       data->header.data.request_id,
	       data->header.data.chunk_index,
	       data->header.data.chunk_length,
	       data->payload_len,
	       c->sockfd, c->user_id);

	// Ensure tmp directory exists
	if (ensure_tmp_dir() != 0) {
		Frame err_frame;
		build_respond_frame(&err_frame, data->header.data.request_id, STATUS_NOT_OK,
		                   "{\"error\": \"Failed to ensure tmp directory\"}");
		send_frame(c->sockfd, &err_frame);
		return;
	}

	const uint8_t *sid = data->header.data.session_id;
	uint32_t chunk_index = data->header.data.chunk_index;
	uint32_t chunk_length = data->header.data.chunk_length;

	UploadSession *us = NULL;
	if (find_session(sid, &us) != 0 || !us) {
		Frame err_frame;
		build_respond_frame(&err_frame, data->header.data.request_id, STATUS_NOT_OK,
		                   "{\"error\": \"Session not found\"}");
		send_frame(c->sockfd, &err_frame);
		return;
	}

	// Update session metadata
	if (chunk_index == us->last_received_chunk + 1 && us->chunk_length == chunk_length) {
		us->last_received_chunk = chunk_index;
	} else {
		Frame err_frame;
		build_respond_frame(&err_frame, data->header.data.request_id, STATUS_NOT_OK,
		                   "{\"error\": \"Invalid chunk index or length\"}");
		send_frame(c->sockfd, &err_frame);
		return;
	}
    
	if (data->payload_len > 0) {
		us->total_received_size += (uint64_t)data->payload_len;
	}

	// Write chunk at offset = chunk_index * chunk_length
	// O_CREAT | O_WRONLY: create file if not exists, open for writing only
	// 0644: rw-r--r--
	char file_path[256];
	snprintf(file_path, sizeof(file_path), "data/storage/tmp/upload_%s", us->uuid_str);
	int fd = open(file_path, O_CREAT | O_WRONLY, 0644);
	if (fd < 0) {
		Frame err_frame;
		build_respond_frame(&err_frame, data->header.data.request_id, STATUS_NOT_OK,
		                   "{\"error\": \"Failed to open tmp file\"}");
		send_frame(c->sockfd, &err_frame);
		return;
	}

	off_t offset = (off_t)chunk_index * (off_t)chunk_length;
	ssize_t w = pwrite(fd, data->payload, data->payload_len, offset);
	if (w < 0 || (size_t)w != data->payload_len) {
		Frame err_frame;
		build_respond_frame(&err_frame, data->header.data.request_id, STATUS_NOT_OK,
		                   "{\"error\": \"Failed to write chunk\"}");
		send_frame(c->sockfd, &err_frame);
		close(fd);
		return;
	}

	close(fd);
	
	// Send success response
	cJSON *response = cJSON_CreateObject();
	cJSON_AddStringToObject(response, "status", "ok");
	cJSON_AddNumberToObject(response, "chunk_index", chunk_index);
	cJSON_AddNumberToObject(response, "bytes_written", data->payload_len);
	char *json_resp = cJSON_PrintUnformatted(response);
	
	Frame ok_frame;
	build_respond_frame(&ok_frame, data->header.data.request_id, STATUS_OK, json_resp);
	pthread_mutex_lock(&c->write_lock);
	send_frame(c->sockfd, &ok_frame);
	pthread_mutex_unlock(&c->write_lock);
	
	free(json_resp);
	cJSON_Delete(response);
	
	printf("[UPLOAD:DATA][SUCCESS] Chunk %u written: bytes=%zu, total_received=%llu/%llu bytes (fd=%d, user_id=%d, session=%s)\n", 
	       chunk_index, data->payload_len, 
	       (unsigned long long)us->total_received_size, 
	       (unsigned long long)us->expected_file_size,
	       c->sockfd, c->user_id, us->uuid_str);
}

/*
Handler for UPLOAD_INIT command to start a new upload session
*/
void upload_init_handler(Conn *c, Frame *f) {
	if (!c || !f) return;

	// Expect JSON payload { cmd: "UPLOAD_INIT", path: string, file_size: number, chunk_size: number }
	cJSON *root = cJSON_Parse((const char *)f->payload);
	cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
	cJSON *pathItem = cJSON_GetObjectItemCaseSensitive(root, "path");
	cJSON *fileSizeItem = cJSON_GetObjectItemCaseSensitive(root, "file_size");
	cJSON *chunkSizeItem = cJSON_GetObjectItemCaseSensitive(root, "chunk_size");
	if (!cJSON_IsString(cmd) || strcmp(cmd->valuestring, "UPLOAD_INIT") != 0 ||
		!cJSON_IsString(pathItem) || !cJSON_IsNumber(fileSizeItem) || !cJSON_IsNumber(chunkSizeItem)) {
		cJSON_Delete(root);
		Frame err_frame;
		build_respond_frame(&err_frame, f->header.cmd.request_id, STATUS_NOT_OK,
							"{\"error\": \"Missing or invalid fields (cmd, path, file_size, chunk_size)\"}");
		send_frame(c->sockfd, &err_frame);
		return;
	}
	uint32_t chunk_length = (uint32_t)chunkSizeItem->valuedouble;
	const char *upload_path = pathItem->valuestring;
	uint64_t file_size = (uint64_t)fileSizeItem->valuedouble;
	cJSON_Delete(root);

	if (ensure_tmp_dir() != 0) {
		Frame err_frame;
		build_respond_frame(&err_frame, f->header.cmd.request_id, STATUS_NOT_OK,
							"{\"error\": \"Failed to ensure tmp directory\"}");
		send_frame(c->sockfd, &err_frame);
		return;
	}

	uint8_t sid[SESSIONID_SIZE];
	if (generate_session_id(sid) != 0) {
		Frame err_frame;
		build_respond_frame(&err_frame, f->header.cmd.request_id, STATUS_NOT_OK,
							"{\"error\": \"Failed to generate session id\"}");
		send_frame(c->sockfd, &err_frame);
		return;
	}

	// Create session slot
	UploadSession *us = NULL;
	for (int i = 0; i < MAX_SESSION; ++i) {
		if (ss[i].filepath[0] == '\0') {
			memset(&ss[i], 0, sizeof(UploadSession));
			memcpy(ss[i].session_id, sid, SESSIONID_SIZE);
			char uuid_str[37];
			if (bytes_to_uuid_string(sid, uuid_str) != 0) {
				Frame err_frame;
				build_respond_frame(&err_frame, f->header.cmd.request_id, STATUS_NOT_OK,
									"{\"error\": \"Server Error\"}");
				send_frame(c->sockfd, &err_frame);
				return;
			}

			// Store upload file path to use after all chunks received
			snprintf(ss[i].filepath, sizeof(ss[i].filepath), "%s", upload_path);
			snprintf(ss[i].uuid_str, sizeof(ss[i].uuid_str), "%s", uuid_str);
			ss[i].last_received_chunk = 0;
			ss[i].chunk_length = chunk_length;
			ss[i].total_received_size = 0;
			ss[i].expected_file_size = file_size;
			us = &ss[i];
			break;
		}
	}
	if (!us) {
		Frame err_frame;
		build_respond_frame(&err_frame, f->header.cmd.request_id, STATUS_NOT_OK,
							"{\"error\": \"No available session slots\"}");
		send_frame(c->sockfd, &err_frame);
		return;
	}

	// Respond with RES including sessionId
	char uuid_str[37];
	bytes_to_uuid_string(sid, uuid_str);
	char payload[128];
	snprintf(payload, sizeof(payload), "{\"sessionId\": \"%s\"}", uuid_str);
	Frame ok;
	build_respond_frame(&ok, f->header.cmd.request_id, STATUS_OK, payload);
	send_frame(c->sockfd, &ok);
}
