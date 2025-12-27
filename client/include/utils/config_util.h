#ifndef CONFIG_UTIL_H
#define CONFIG_UTIL_H

#include <stddef.h>

#define DEFAULT_PATH "config.json"
#define SERVER_MAX_LEN 192

typedef struct {
    char server[SERVER_MAX_LEN];
    int port;
} ConfigData;

int config_load_file(const char *path, ConfigData *out);
int config_load_default(ConfigData *out);
int config_get_server(char *server_out, size_t server_out_size);
int config_get_port(int *port_out);

#endif  /* CLIENT_SRC_UTILS_CONFIG_UTIL_H */
