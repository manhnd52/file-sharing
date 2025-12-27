#include "cJSON.h"
#include "utils/config_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_entire_file(const char *path, char **out_buffer, size_t *out_len) {
    if (!path || !out_buffer || !out_len)
        return -1;

    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    long length = ftell(fp);
    if (length < 0) {
        fclose(fp);
        return -1;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    char *buffer = (char *)malloc((size_t)length + 1);
    if (!buffer) {
        fclose(fp);
        return -1;
    }

    size_t read_len = fread(buffer, 1, (size_t)length, fp);
    fclose(fp);

    if (read_len != (size_t)length) {
        free(buffer);
        return -1;
    }

    buffer[length] = '\0';
    *out_buffer = buffer;
    *out_len = (size_t)length;
    return 0;
}

int config_load_file(const char *path, ConfigData *out) {
    if (!path || !out)
        return -1;

    char *content = NULL;
    size_t content_len = 0;
    if (read_entire_file(path, &content, &content_len) != 0)
        return -1;

    cJSON *json = cJSON_Parse(content);
    free(content);
    if (!json)
        return -1;

    cJSON *server_item = cJSON_GetObjectItemCaseSensitive(json, "server");
    cJSON *port_item = cJSON_GetObjectItemCaseSensitive(json, "port");
    int status = 0;

    if (!cJSON_IsString(server_item) || !server_item->valuestring)
        status = -1;
    else
        strncpy(out->server, server_item->valuestring, SERVER_MAX_LEN - 1);

    out->server[SERVER_MAX_LEN - 1] = '\0';

    if (!cJSON_IsNumber(port_item))
        status = -1;
    else
        out->port = port_item->valueint;

    cJSON_Delete(json);
    return status;
}

int config_load_default(ConfigData *out) {
    return config_load_file(DEFAULT_PATH, out);
}

int config_get_server(char *server_out, size_t server_out_size) {
    if (!server_out || server_out_size == 0)
        return -1;

    ConfigData cfg;
    if (config_load_default(&cfg) != 0)
        return -1;

    strncpy(server_out, cfg.server, server_out_size - 1);
    server_out[server_out_size - 1] = '\0';
    return 0;
}

int config_get_port(int *port_out) {
    if (!port_out)
        return -1;

    ConfigData cfg;
    if (config_load_default(&cfg) != 0)
        return -1;

    *port_out = cfg.port;
    return 0;
}
