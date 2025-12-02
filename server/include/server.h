#ifndef SERVER_H
#define SERVER_H

// Khởi động server (blocking)
void server_start(int port);

// Yêu cầu server dừng (an toàn)
void server_stop();

#endif
