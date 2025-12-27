# Connect API - Client Connection Management

## Tổng quan

`Connect` là một client-side API để quản lý kết nối TCP đến server với các tính năng:
- Gửi frame bất đồng bộ (AUTH, CMD, DATA)
- Theo dõi pending requests với callback
- Tự động heartbeat (PING) để duy trì kết nối
- Thread-safe với mutex

## Cấu trúc `Connect`

```c
struct Connect {
    int sockfd;                    // Socket descriptor đến server
    
    // Pending requests tracking
    struct PendingReq {
        uint32_t request_id;       // ID của request đã gửi
        resp_callback cb;          // Callback khi nhận response
        time_t sent_time;          // Thời điểm gửi (dùng cho timeout)
        int in_use;                // 0=slot trống, 1=đang chờ response
    } pending[MAX_PENDING_REQUESTS];  // Max 1024 pending requests
    
    pthread_mutex_t pending_lock;  // Bảo vệ truy cập pending array
    uint32_t request_counter;      // Bộ đếm tăng dần để generate request_id
    pthread_mutex_t request_lock;  // Bảo vệ request_counter
    
    // Heartbeat mechanism
    time_t last_sent_time;         // Thời điểm gửi frame cuối cùng
    int keep_alive;                // 1=running, 0=stop (dùng để stop threads)
    pthread_t heartbeat_tid;       // Thread ID của heartbeat thread
    
    // Function pointers (OOP-style API)
    int (*send_auth)(Connect *c, Frame *f, resp_callback cb);
    int (*send_cmd)(Connect *c, Frame *f, resp_callback cb);
    int (*send_data)(Connect *c, Frame *f, resp_callback cb);
    void (*destroy)(Connect *c);
};
```

## Các Thread

### 1. Main Thread
- Tạo Connect, gửi requests, cleanup
- Gọi `connect_create()` → khởi tạo socket, mutex, start threads
- Gọi `conn->send_cmd()`, `conn->send_auth()`, etc.
- Gọi `connect_destroy()` → stop threads, đóng socket

### 2. Response Reader Thread (`resp_reader_thread_func`)
**Mục đích**: Liên tục đọc RESPOND/DATA frames từ server

**Flow**:
```
while(1) {
    recv_frame(sockfd, &resp)  // Blocking read
    
    if (resp.msg_type == MSG_RESPOND || MSG_DATA):
        rid = extract request_id from resp
        
        lock(pending_lock)
        for each pending request:
            if pending[i].request_id == rid:
                call pending[i].cb(&resp)  // Invoke callback
                pending[i].in_use = 0      // Free slot
                break
        unlock(pending_lock)
}
```

**Đặc điểm**:
- Chạy detached (không cần join khi destroy)
- Blocking trên `recv_frame()` → exit khi socket đóng
- Thread-safe với `pending_lock`

### 3. Heartbeat Thread (`heartbeat_thread_func`)
**Mục đích**: Tự động gửi PING mỗi 10 giây để giữ kết nối sống

**Flow**:
```
while(keep_alive) {
    sleep(1)
    now = time(NULL)
    if (now - last_sent_time >= HEARTBEAT_INTERVAL):
        build_cmd_frame(&f, request_id, "{\"cmd\":\"PING\"}")
        send_cmd(c, &f, NULL)  // No callback needed
        update last_sent_time
}
```

**Đặc điểm**:
- Chạy joinable (phải join khi destroy)
- Dừng khi `keep_alive = 0`
- Không quan tâm response (callback = NULL)

## Request Flow

### Gửi Request
```
Client Call: conn->send_cmd(conn, &frame, callback)
    ↓
_send_cmd(c, f, cb)
    ↓
connect_send_frame(c, f, cb)
    ↓
    1. Generate unique request_id (thread-safe)
    2. Fill frame.header.cmd.request_id = rid
    3. Find free slot in pending[] array
       lock(pending_lock)
       pending[i] = {rid, cb, time, in_use=1}
       unlock(pending_lock)
    4. send_frame(sockfd, f)  // Send qua TCP
    5. Update last_sent_time
```

### Nhận Response
```
Server sends RESPOND frame với request_id=X
    ↓
Response Reader Thread: recv_frame() → nhận frame
    ↓
Extract request_id from frame
    ↓
lock(pending_lock)
Search pending[] for matching request_id
    ↓
Found → Call callback(frame), free slot
unlock(pending_lock)
```

## Mutex và Thread Safety

### `pending_lock`
**Bảo vệ**: `pending[]` array

**Critical sections**:
- Gửi request: tìm slot trống, ghi `{rid, cb, in_use=1}`
- Nhận response: tìm matching rid, call callback, clear slot

**Tránh deadlock**: Lock thời gian ngắn, chỉ modify array, không gọi I/O

### `request_lock`
**Bảo vệ**: `request_counter`

**Critical sections**:
- `generate_request_id()`: đọc và tăng counter

**Tránh race**: Đảm bảo mỗi request có ID duy nhất

## Request ID Management

**Cơ chế**:
- `request_counter` bắt đầu từ 1
- Mỗi lần gửi frame → `counter++` (atomic với lock)
- Client gửi ID trong header → Server echo lại trong RESPOND
- Response thread match ID để gọi đúng callback

**Lưu ý**:
- Request ID **không được** convert network byte order trong `connect.c` (line 23)
- Vì `build_*_frame()` sẽ tự động `htonl()` khi build frame
- Nếu `htonl()` 2 lần → sai byte order

## Timeout Handling (TODO)

**Hiện tại**: `sent_time` được lưu nhưng chưa check timeout

**Cần implement**:
```c
// Trong response reader hoặc thread riêng
void check_timeouts(Connect *c) {
    time_t now = time(NULL);
    lock(pending_lock);
    for (i in pending[]) {
        if (in_use && now - sent_time > REQUEST_TIMEOUT) {
            if (cb) cb(NULL);  // Signal timeout với NULL
            in_use = 0;
        }
    }
    unlock(pending_lock);
}
```

## API Usage Example

```c
// 1. Connect to server
Connect *conn = connect_create("127.0.0.1", 5555, 10);

// 2. Define callback
void my_callback(Frame *resp) {
    if (resp == NULL) {
        printf("Request timeout!\n");
        return;
    }
    printf("Received response: status=%d\n", resp->header.resp.status);
}

// 3. Send request
Frame cmd;
build_cmd_frame(&cmd, 0, "{\"cmd\":\"LIST\"}");
conn->send_cmd(conn, &cmd, my_callback);

// 4. Wait for response (handled by thread)
sleep(5);

// 5. Cleanup
connect_destroy(conn);
```

## Lifecycle

```
connect_create()
    → socket() + connect()
    → init mutexes
    → spawn resp_reader_thread (detached)
    → spawn heartbeat_thread (joinable)
    → return Connect*

[Normal operation]
    → Main thread: send requests
    → Reader thread: receive responses, call callbacks
    → Heartbeat thread: send PING every 10s

connect_destroy()
    → keep_alive = 0
    → pthread_join(heartbeat_tid)  // Wait heartbeat stop
    → close(sockfd)  // This breaks reader thread's recv_frame()
    → destroy mutexes
    → free(c)
```

## Best Practices

1. **Callback execution**: Callbacks chạy trong reader thread → keep them **short and non-blocking**
2. **Request ID**: Không tự set request_id trong frame; để `connect_send_frame()` generate
3. **Cleanup**: Luôn gọi `connect_destroy()` để tránh leak threads/sockets
4. **Heartbeat**: Nếu không muốn heartbeat, set `HEARTBEAT_INTERVAL` lớn hoặc disable trong code
5. **Pending limit**: Max 1024 pending requests → nếu đầy thì `send_*()` return -1

## Known Issues

1. **Reader thread cleanup**: Thread detached → không đợi exit khi destroy (có thể crash nếu access freed memory)
   - **Fix**: Dùng joinable thread hoặc signal/condition để graceful shutdown
   
2. **Timeout chưa implement**: `sent_time` được lưu nhưng không check
   - **Fix**: Thêm timeout checker trong reader hoặc thread riêng

3. **Error handling**: `recv_frame()` fail → thread exit nhưng không báo cho main thread
   - **Fix**: Thêm callback `on_disconnect` hoặc error handler

4. **Request ID overflow**: `uint32_t counter` có thể overflow sau 4 billion requests
   - **Fix**: Reset counter hoặc dùng 64-bit
