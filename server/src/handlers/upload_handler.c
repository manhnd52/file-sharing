#include "handlers/upload_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

UploadSession ss[MAX_SESSION];

static void respond_upload_finish_error(Conn *c, Frame *f, const char *payload) {
	if (!c || !f || !payload) return;
	Frame err_frame;
	build_respond_frame(&err_frame, f->header.cmd.request_id, STATUS_NOT_OK, payload);
	send_frame(c->sockfd, &err_frame);
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
		if (memcmp(ss[i].session_id, sid, SESSIONID_SIZE) == 0 && ss[i].file_name[0] != '\0') {
			*out = &ss[i];
			return 0;
		}
	}
	return -1;
}

/*
Handler for received DATA frames (file upload chunks)
*/
void upload_handler(Conn *c, Frame *data) {
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
	if (chunk_index == us->last_received_chunk + 1 && us->chunk_length >= chunk_length) {
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

	off_t offset = ((off_t)chunk_index - 1) * (off_t)us->chunk_length;
	ssize_t write_num = pwrite(fd, data->payload, data->payload_len, offset);
	if (write_num < 0 || (size_t)write_num != data->payload_len) {
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
	cJSON_AddNumberToObject(response, "bytes_written", write_num);
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
Handler for UPLOAD_INIT command to start a new upload session when receiving CMD frame with cmd = "UPLOAD_INIT"
  Expected JSON payload: { cmd: "UPLOAD_INIT", path: string, file_size: number, chunk_size: number }
  Response JSON payload: { sessionId: string }
*/
void upload_init_handler(Conn *c, Frame *f) {
	if (!c || !f) return;

	// Expect JSON payload { cmd: "UPLOAD_INIT", path: string, file_size: number, chunk_size: number }
	cJSON *root = cJSON_Parse((const char *)f->payload);
	cJSON *cmd = cJSON_GetObjectItemCaseSensitive(root, "cmd");
	cJSON *parentFolderIdItem = cJSON_GetObjectItemCaseSensitive(root, "parent_folder_id");
	cJSON *fileName = cJSON_GetObjectItemCaseSensitive(root, "file_name");
	cJSON *fileSizeItem = cJSON_GetObjectItemCaseSensitive(root, "file_size");
	cJSON *chunkSizeItem = cJSON_GetObjectItemCaseSensitive(root, "chunk_size");
	
	if (!cJSON_IsString(cmd) || strcmp(cmd->valuestring, "UPLOAD_INIT") != 0 ||
		!cJSON_IsNumber(parentFolderIdItem) || !cJSON_IsString(fileName) || !cJSON_IsNumber(fileSizeItem) || !cJSON_IsNumber(chunkSizeItem)) {
		cJSON_Delete(root);
		Frame err_frame;
		build_respond_frame(&err_frame, f->header.cmd.request_id, STATUS_NOT_OK,
							"{\"error\": \"Missing or invalid fields (cmd, parent_folder_id, file_name, file_size, chunk_size)\"}");
		send_frame(c->sockfd, &err_frame);
		return;
	}

	uint32_t chunk_length = (uint32_t)chunkSizeItem->valuedouble;
	const char *file_name = fileName->valuestring;
	int parent_folder_id = parentFolderIdItem->valueint;
	uint64_t file_size = (uint64_t)fileSizeItem->valuedouble;

	uint8_t sid[BYTE_UUID_SIZE];

	generate_byte_uuid(sid);

	// Create session slot
	UploadSession *us = NULL;
	for (int i = 0; i < MAX_SESSION; ++i) {
		if (ss[i].file_name[0] == '\0') {
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
			snprintf(ss[i].file_name, sizeof(ss[i].file_name), "%s", file_name);
			snprintf(ss[i].uuid_str, sizeof(ss[i].uuid_str), "%s", uuid_str);
			ss[i].parent_folder_id = parent_folder_id;
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
	cJSON_Delete(root);
}

void upload_finish_handler(Conn *c, Frame *f) {
	if (!c || !f) return;

	cJSON *root = cJSON_Parse((const char *)f->payload);
	if (!root) {
		respond_upload_finish_error(c, f, "{\"error\": \"invalid_json\"}");
		return;
	}

	cJSON *session_id_json = cJSON_GetObjectItemCaseSensitive(root, "session_id");
	if (!session_id_json || !cJSON_IsString(session_id_json) || session_id_json->valuestring[0] == '\0') {
		respond_upload_finish_error(c, f, "{\"error\": \"missing_session_id\"}");
		cJSON_Delete(root);
		return;
	}

	const char *session_id_str = session_id_json->valuestring;
	uint8_t session_id[BYTE_UUID_SIZE];

	if (uuid_string_to_bytes(session_id_str, session_id) != 0) {
		respond_upload_finish_error(c, f, "{\"error\": \"invalid_session_id\"}");
		cJSON_Delete(root);
		return;
	}

	UploadSession *us = NULL;
	if (find_session(session_id, &us) != 0 || !us) {
		respond_upload_finish_error(c, f, "{\"error\": \"session_not_found\"}");
		cJSON_Delete(root);
		return;
	}

	if (us->expected_file_size > 0 && us->total_received_size != us->expected_file_size) {
		respond_upload_finish_error(c, f, "{\"error\": \"incomplete_upload\"}");
		cJSON_Delete(root);
		return;
	}

	cJSON_Delete(root);

	// Move file from tmp to final storage location
	char tmp_path[256];
	snprintf(tmp_path, sizeof(tmp_path), "data/storage/tmp/upload_%s", us->uuid_str);
	
	// Remove "upload_" prefix to get final path
	char final_path[256];
	snprintf(final_path, sizeof(final_path),
         "data/storage/%s",
         us->uuid_str);
	
	// Move file
	if (rename(tmp_path, final_path) != 0) {
		perror("rename failed");
	}

	// Save file metadata to database, associate with user, etc. (omitted for brevity)
	int file_id = file_save_metadata(c->user_id, us->parent_folder_id, us->file_name, us->uuid_str, us->total_received_size);

	// Clear session
	memset(us, 0, sizeof(UploadSession));

	// Respond success
	Frame ok;
	char json_resp[128];

	snprintf(
		json_resp,
		sizeof(json_resp),
		"{\"status\":\"upload_finished\", \"file_id\": %d}",
		file_id
	);

	build_respond_frame(
		&ok,
		f->header.cmd.request_id,
		STATUS_OK,
		json_resp
	);

	send_frame(c->sockfd, &ok);
	
	return;
}
