#ifndef REGISTER_H
#define REGISTER_H

typedef struct {
    char username[64];
    char password[64];
} RegisterRequestPayload;

typedef struct {
    int success;
    char message[256];
} RegisterResponsePayload;

#endif
