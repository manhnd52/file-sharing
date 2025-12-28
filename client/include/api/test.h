#ifndef CLIENT_API_TEST_H
#define CLIENT_API_TEST_H

#include <stdint.h>

int run_cli_command(const char *prog, int argc, char **argv, const char *host,
                    uint16_t port);

#endif // CLIENT_API_TEST_H
