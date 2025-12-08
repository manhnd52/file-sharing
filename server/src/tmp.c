// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <pthread.h>
// #include <sys/select.h>
// #include <sys/socket.h>
// #include <errno.h>
// #include <stdbool.h>
// #include <stdint.h>

// #define MAX_CONNS 1024
// #define BUF_SIZE 8192

// typedef enum { MSG_CMD = 1, MSG_RES = 2, MSG_DATA = 3 } MsgType;

// typedef enum { ST_CONNECTED, ST_AUTH, ST_UPLOADING, ST_DOWNLOADING } ConnState;

// typedef struct {
//     int fd;
//     pthread_mutex_t write_lock;  // bảo vệ socket
//     ConnState state;

//     // buffer nhận TCP
//     uint8_t recv_buf[BUF_SIZE];
//     size_t recv_len;
    
//     // session upload/download
//     uint32_t request_id;
//     size_t total_size;
//     size_t received;
//     char temp_file_path[256];

//     bool busy_worker; // true nếu worker đang xử lý task nặng
// } Conn;

// Conn *connections[MAX_CONNS];

// // --- send_exact: gửi toàn bộ data ---
// ssize_t send_exact(int fd, const void *buf, size_t len) {
//     size_t total = 0;
//     const char *p = buf;
//     while (total < len) {
//         ssize_t n = send(fd, p + total, len - total, 0);
//         if (n < 0) {
//             if (errno == EINTR) continue;
//             return -1;
//         }
//         total += n;
//     }
//     return total;
// }

// ssize_t read_all(int fd, void *buf, size_t len) {
//     size_t total = 0;
//     char *p = buf;
//     while (total < len) {
//         ssize_t n = recv(fd, p + total, len - total, 0);
//         if (n <= 0) {
//             if (n < 0 && errno == EINTR) continue;
//             return n; // lỗi hoặc kết nối đóng
//         }
//         total += n;
//     }
//     return total;
// }

// // --- conn_send: thread-safe ---
// ssize_t conn_send(Conn *c, const void *buf, size_t len) {
//     pthread_mutex_lock(&c->write_lock);
//     ssize_t sent = send_exact(c->fd, buf, len);
//     pthread_mutex_unlock(&c->write_lock);
//     return sent;
// }

// // --- xử lý msg CMD / DATA ---
// void handle_msg(Conn *c, MsgType type, uint8_t *payload, size_t len) {
//     switch(type) {
//         case MSG_CMD:
//             printf("CMD received on fd %d, len=%zu\n", c->fd, len);
//             // Nếu task nặng: spawn worker
//             if (!c->busy_worker && len > 4096) { // giả sử >4KB là task nặng
//                 c->busy_worker = true;
//                 // tạo detached thread xử lý upload/download
//                 pthread_t tid;
//                 pthread_attr_t attr;
//                 pthread_attr_init(&attr);
//                 pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
//                 pthread_create(&tid, &attr, (void*(*)(void*))handle_msg, c); // worker xử lý task nặng
//                 pthread_attr_destroy(&attr);
//             } else {
//                 // xử lý nhẹ ngay
//                 const char *res = "ACK CMD";
//                 conn_send(c, res, strlen(res));
//             }
//             break;
//         case MSG_DATA:
//             printf("DATA received fd=%d, len=%zu\n", c->fd, len);
//             // lưu vào temp file (hoặc update session)
//             c->received += len;
//             const char *ack = "ACK DATA";
//             conn_send(c, ack, strlen(ack));
//             break;
//         case MSG_RES:
//             printf("RES received fd=%d, len=%zu\n", c->fd, len);
//             break;
//     }
// }

// // --- parse buffer TCP thành msg ---
// void parse_tcp_buffer(Conn *c) {
//     // giả lập: mỗi message bắt đầu 1 byte type + payload
//     while (c->recv_len >= 1) {
//         MsgType type = (MsgType)c->recv_buf[0];
//         size_t payload_len = c->recv_len - 1; // toàn bộ còn lại
//         ssize_t payload = read_all(c->fd, c->recv_buf+1, payload_len);
//         handle_msg(c, type, c->recv_buf+1, payload);
//         c->recv_len = 0; // giả lập xử lý hết buffer
//     }
// }

// // --- main loop ---
// void main_loop(int listen_fd) {
//     while(1) {
//         fd_set read_fds; // tập các fd mà select() theo dõi
//         FD_ZERO(&read_fds); // khởi tạo tập rỗng
//         FD_SET(listen_fd, &read_fds); // thêm listen_fd vào tập
//         int maxfd = listen_fd; // theo dõi fd lớn nhất

//         for(int i=0;i<MAX_CONNS;i++){
//             Conn *c = connections[i];
//             if(c){
//                 FD_SET(c->fd, &read_fds);
//                 if(c->fd > maxfd) maxfd = c->fd;
//             }
//         }

//         int ret = select(maxfd+1, &read_fds, NULL, NULL, NULL);
//         if(ret < 0) continue;

//         // Nếu có kết nối mới
//         if(FD_ISSET(listen_fd, &read_fds)){
//             // accept new connection (pseudo)
//             // Tạo Conn mới
//             printf("New connection accepted\n");
//         }

//         for(int i=0;i<MAX_CONNS;i++){
//             Conn *c = connections[i];
//             if(c && FD_ISSET(c->fd, &read_fds)){
//                 ssize_t n = recv(c->fd, c->recv_buf, sizeof(c->recv_buf), 0);
//                 if(n <= 0){
//                     printf("Client disconnected fd=%d\n", c->fd);
//                     close(c->fd);
//                     free(c);
//                     connections[i]=NULL;
//                 } else {
//                     c->recv_len = n;
//                     parse_tcp_buffer(c);
//                 }
//             }
//         }
//     }
// }

// int main() {
//     int listen_fd = 0; // pseudo listen fd
//     // init connections array
//     memset(connections, 0, sizeof(connections));

//     main_loop(listen_fd);
//     return 0;
// }
