#include "client.h"
#include "test.h"
#include "utils/config_util.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <command> [args]\n", prog);
    fprintf(stderr, "Available commands:\n");
    fprintf(stderr, "    session <username> <password>  Run register/login/auth/logout against configured server\n");
    fprintf(stderr, "    --test                        Run client unit smoke tests\n");
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--test") == 0) {
        return run_client_tests();
    }
    if (argc < 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    ConfigData cfg;
    if (config_load_default(&cfg) != 0 || cfg.port <= 0 || cfg.port > 65535) {
        fprintf(stderr, "Failed to load %s\n", DEFAULT_PATH);
        return EXIT_FAILURE;
    }

    return 0;
}
