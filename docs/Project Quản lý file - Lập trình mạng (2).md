# Giới thiệu tổng quát

## Danh sách thành viên  

Nguyễn Đức Mạnh 20225880 

Dương Đình Khôi 20225869 

 

## Chủ đề 

Xây dựng ứng dụng chia sẻ file trực tuyến 

Giới thiệu: Tương tự Google Drive, cho phép nhiều người dùng cùng truy cập (mục tiêu demo: 1 máy server, 2 máy client).

Cách thức sử dụng: Người dùng đăng nhập vào hệ thống, và thực hiện các thao tác quản lý file và thư mục như với Google Drive.

## Chức năng 

* Quản lý người dùng (User Management): Đăng ký tài khoản và đăng nhập, phiên đăng nhập.   
* Lệnh quản lý tệp (File Management Commands): Liệt kê, tạo, đổi tên, xóa, sao chép, di chuyển tệp hoặc thư mục. (Tương tự giao thức TFP)  
* Tải lên / Tải xuống tệp (File Upload/Download): Cho phép người dùng tải lên/tải xuống từng tệp từ/đến máy chủ.   
* Tải lên / Tải xuống thư mục (Folder Upload/Download): Hỗ trợ tải lên/tải xuống toàn bộ thư mục.   
* Tìm kiếm tệp (File Search): Tìm theo tên tệp và cho phép tải xuống trực tiếp từ kết quả tìm kiếm.   
* Quản lý quyền (Permission Management): Áp dụng quyền cho tệp/thư mục (tương tự Google Drive), cho phép người dùng thiết lập mức chia sẻ cho từng tệp/thư mục với người dùng khác. 


| Quyền | Mô tả |
| :---- | :---- |
| Người xem | Chỉ xem được nội dung, không thể chỉnh sửa, nhận xét hoặc chia sẻ |
| Người chỉnh sửa  | Có thể chỉnh sửa tên và xóa và chia sẻ cho người khác |
| Người sở hữu | Toàn quyền sở hữu đối với tệp |


* Giao diện đồ họa (GUI): Giao diện trực quan, thân thiện với người dùng.   
* Giao diện CLI để test: ls, upload, download, mkdir, search, chmod,...

 

## Điểm đánh giá 

* Xử lý luồng (Stream handling): 1 điểm   
* Cài đặt I/O qua socket trên máy chủ: 2 điểm   
* Đăng ký và quản lý tài khoản: 2 điểm   
* Đăng nhập và quản lý phiên làm việc: 2 điểm   
* Tải lên / Tải xuống tệp: 2 điểm   
* Xử lý tệp dung lượng lớn: 2 điểm   
* Tải lên / Tải xuống toàn bộ thư mục: 3 điểm   
* Thao tác tệp (đổi tên, xóa, sao chép, di chuyển): 2 điểm   
* Thao tác thư mục (tạo, đổi tên, xóa, sao chép, di chuyển): 2 điểm   
* Tìm kiếm và lựa chọn tệp: 3 điểm   
* Ghi log hoạt động: 1 điểm   
* Quản lý quyền người dùng: 5 điểm   
* Giao diện đồ họa người dùng (GUI): 3 điểm 

 

## Phân công chức năng

| Mạnh | Khôi |
| :---- | :---- |
| Xử lý luồng (Stream handling): 3 điểm Thiết kế protocol: 1 điểm  Cài đặt I/O qua socket trên máy chủ: 2 điểm  Tải lên / Tải xuống tệp: 2 điểm  Xử lý tệp dung lượng lớn: 2 điểm  Tải lên / Tải xuống toàn bộ thư mục: 3 điểm Quản lý quyền người dùng: 5 điểm  | Đăng ký và quản lý tài khoản: 2 điểm  Đăng nhập và quản lý phiên làm việc: 2 điểm  Thao tác tệp (đổi tên, xóa, sao chép, di chuyển): 2 điểm  Thao tác thư mục (tạo, đổi tên, xóa, sao chép, di chuyển): 2 điểm Tìm kiếm và lựa chọn tệp: 3 điểm Giao diện đồ họa người dùng (GUI): 3 điểm  |
| Tổng: 15 điểm | Tổng: 14 điểm |

# Kế hoạch thực hiện đến Checkpoint 2

Tập trung Thiết kế:

- Kiến trúc hệ thống  
- Protocol  
- Data

- 

# Giao diện

![][image1]![][image2]

# Chú ý

Các câu hỏi đáng chú ý

| Nhóm | Câu hỏi gợi ý |
| ----- | ----- |
| **Networking** | \- Giao thức client–server có cơ chế ACK/NACK không? \- Nếu client ngắt giữa chừng khi upload, server xử lý thế nào? \- Có hỗ trợ resume upload/download không? |
| **Concurrency** | \- Server dùng thread pool hay mỗi client một thread? \- Có giới hạn số kết nối đồng thời không? \- Làm sao tránh race condition khi hai client ghi cùng file? |
| **Permission & Security** | \- Quyền `rwx` được lưu và áp dụng thế nào? \- Có phân biệt quyền cho thư mục và file không? \- Khi user B tải file của A, server kiểm tra quyền ra sao? |
| **File System** | \- Server tổ chức file thật hay lưu meta vào DB? \- Nếu hai người cùng tên file, có namespace riêng không? \- Thư mục “shared” có cơ chế cleanup? |
| **Client GUI** | \- GUI có phản hồi tiến trình (progress bar) không? \- Có hỗ trợ drag-drop file không? \- Giao diện cập nhật danh sách file realtime hay thủ công? |
| **Logging & Error Handling** | \- Ghi log theo user hay theo session? \- Khi gặp lỗi kết nối, client có tự reconnect không? \- Server log có cơ chế xoay file log (log rotation)? |

Các vấn đề khó khăn cần xử lý:

- Hai người dùng đồng thời edit, delete folder.

**Yêu cầu nào đến trước** được xử lý và cập nhật trạng thái hệ thống.  
**Yêu cầu đến sau** nếu không còn hợp lệ do trạng thái đã thay đổi → bị từ chối và ghi log lỗi.  
Ví dụ metaData của thư mục được lưu trên server . Các thông tin của  được server cung cấp cho user lúc mới vào   
{  
  "folder\_id": "abc123",  
  "name": "Báo cáo Q4",  
  "version": 5,  
  "last\_modified": "2025-11-04T11:00:00Z",  
  "deleted": false  
}  
Khi user A gửi yêu   
{  
  "user\_id": "userA",  
  "action": "rename",  
  "target\_folder": "abc123",  
  "expected\_version": 5,  
  "new\_name": "Báo cáo cuối năm"  
}  
\=\> metadata sẽ thay đổi   
**{**  
  **"folder\_id": "abc123",**  
  **"name": "Báo cáo cuối năm",**  
  **"version": 6,**  
  **"last\_modified": "2025-11-04T11:14:00Z",  // thời điểm xử lý thực tế**  
  **"deleted": false**  
**}**

**Khi user B gửi yêu cầu vẫn expected version \= 5 \=\> log lỗi** 

- Xử lý file lớn như thế nào?  
  FIle lớn được chia nhỏ thành các chunk , tải lên từng phần một cách song song.  
    
- Nếu client ngắt giữa chừng khi upload, server xử lý thế nào?  
  Khi yêu cầu upload \=\> server tạo một phiên gửi về client  \=\> upload từng  chunk lên \=\> server lưu trong bộ nhớ tạm ::: có cài timeout   
  Nếu quá thời gian mà chưa xong thì xóa hết bộ nhớ hủy .  
  Trong trường hợp phía client gửi xong rồi mà phía server check chưa đủ thì server gửi lại cho client một tin là thiếu chunk \=\> client sẽ gửi lại chunk thiếu

- Ở server xử lý thread mỗi người dùng như nào?  
  Sử dụng thread pool : server tạo số lượng thread cố định \=\> nếu số lượng yêu cầu sử dụng quá số lượng thread pool thì yêu cầu vào hàng đợi \=\> khi nào server rảnh thì sẽ xử lý   
- Quyền được xử lý như thế nào?  
- Server tổ chức file thật hay lưu meta vào DB?  
- Nếu hai người cùng tên file, có namespace riêng không?  
- Thư mục “shared” hoạt động như nào?

# Kiến trúc hệ thống

## Kiến trúc hệ thống

Server: C  
Client CLI: C  
Client GUI: PyQt  
DB: SQLite → Metadata  
Storage: File sytem của server

## Server sẽ được tổ chức thành các module:

- authentication: login  
- session: sau khi người dùng đăng nhập thì server cấp cho client mã token có thời hạn để trao đổi với server.  
- file module  
- folder module

## Client:

- Cũng cần có module để phân tích gói tin mà server gửi ngược lại: notification, error,...  
- Client:   
  - GUI: Python (PyQt / Tkinter)  
    - Dùng **ctypes** để gọi hàm từ thư viện C.: Sử dụng API thông qua **dynamically linked libraries** được biên dịch bằng mã C để kết nối C module \-\> trao đổi với Server.   
  - C Module:  
    - network  
    - file\_io.c đọc ghi/file khi upload, download  
    - auth.c // login/signup  
    - api.c // viết các API cung cấp cho GUI và CLI  
    - CLI: main.c (entry của CLI)  
    - command.c      // parse CLI lệnh

# File Storage \+ Database

## File storage

**data/storage**  
├── 7f/  
 │   └── 7f3a9f0b8a23... (binary content)  
 ├── 91/  
 │   └── 91bcefa9233a...  
**data/tmp :** Chứa các chunk, dữ liệu tạm thời trong các phiên upload

// 7f, 91 là hai chữ đầu của hash → tìm kiếm file nhanh hơn, tránh để tất cả vào 1 folder (**Hash** **sharding**)

Mỗi file thật được đặt tên bằng UUID ngẫu nhiên.  
Khi người dùng upload, server tính id và lưu thông tin file vào metadata

Đường dẫn /data/storage/xx/yyyyyy... chỉ là file vật lý, không chứa thông tin người dùng hay quyền.

**Abstract storage → Dễ dàng thay đổi storage sau này**  
getFile(hashcode) → File  
saveFile(File) → hashcode   
deleteFile(hashcode) → …

## Metadata database: SQLite

**users**  
 | id | username | password\_hash | created\_at |

**sessions**  
 | id | user\_id | token | expires\_at |

**folders**  
 | id | name | parent\_id | owner\_id | user\_root

**user\_root**: có phải thư mục hoom của một user (boolean) → lúc đó \= với parent\_id \= nul

**files**  
 | id | name | folder\_id | owner\_id | storage\_hash | size  | created\_at |

**permissions**  
 | id | target\_type | target\_id | user\_id | permission (0=none,1=read,2=write,3=owner) |

target\_type: 0: file; 1: folder

target\_id: folder id hoặc file id

### Tối ưu CRUD và duyệt file/folder nhanh

Khi người dùng mở thư mục, bạn cần hiển thị danh sách file rất nhanh.

### **Giải pháp:**

* Duyệt theo **folder\_id** trong DB, không duyệt trên disk.  
* *Mỗi folder có thể cache số file và tổng dung lượng.*  
* Khi CRUD (upload/delete/rename):  
  * Cập nhật DB trước.  
  * **Việc ghi/xoá thật trên disk thực hiện nền (async worker).**

Dùng **index** trong DB:  
 CREATE INDEX idx\_files\_folder ON files(folder\_id);  
CREATE INDEX idx\_perm\_user\_target ON permissions(user\_id, target\_id);

# Streaming Handle

# Giới thiệu

# Giới thiệu

# **Stream handling giải quyết những vấn đề gì?**

* Cắt message đúng điểm  
* Ghép message nếu bị chia nhỏ  
* Tránh dính message (sticky packet)  
* Xử lý khi connection drop giữa chừng  
* Bảo đảm gửi nhận không bị mất byte

# **Mục tiêu ngắn gọn**

Giao thức TCP byte-stream để:

* Gửi/nhận nhiều loại message (meta, chunk file, control).  
* Đảm bảo message nguyên vẹn, phân rã/ghép đúng.  
* Hỗ trợ resume (tiếp tục tải lên/tải xuống).  
* Có cơ chế ACK/seq để phát hiện mất/thiếu dữ liệu và tái truyền.  
* Đơn giản, dễ cài đặt với C/Java/Kotlin.

# **Phân tầng trong thiết kế protocol tầng ứng dụng**

Để truyền dữ liệu qua TCP stream, bạn phải làm 3 lớp:

(1) Byte stream → (2) Message → (3) Logic của bạn (file-sharing)

1\) **Byte stream**: Do OS phụ trách

2\) **Message layer (framing \+ header)**

Đây là tầng bạn phải tự code:

* Thiết kế các loại message  
* Cơ chế phân tách các message  
* Ghép dữ liệu thành message để gửi.  
* Tách message khi nhận.

**3\) Business logic** File chunking, ACK, Resume, State machine…

# 

# **Những thứ cần thiết kế**

Spec TCP stream cho file-sharing (bao gồm framing, header, checksum, chunking, ACK/retransmit, reconnect, và state machine).

* Framing \= cách bạn “đóng khung” một message trong dòng byte TCP → tạch biệt các message trong dòng byte này.  
* Chunk: chia nhỏ dữ liệu → gửi theo block, hỗ trợ resume, truyền song song, pinelined  
* ACK: xác nhận đã nhận thành công message, chunk  
* Retransmit: gửi lại dữ liệu nếu ACK không đến  
* **Reconnect** \= logic khôi phục quá trình download, upload sau khi kết nối TCP bị mất giữa chừng.  
* State machine \= mô tả từng trạng thái của protocol và cách chuyển giữa chúng bao gồm cả sender và receiver.  
  * `Ví dụ sender trong quá trình upload file: CONNECT → AUTHENTICATED → SENDING_META → SENDING_CHUNKS → COMPLETE`  
  * `Receiver cũng có state machine riêng: WAIT_META → WAIT_CHUNKS → FILE_READY`

# \*Thắc mắc

# Thắc mắc

`Có cần thiết thiết kế checksum khi mà truyền qua TCP đã sẵn có cơ chế của TCP`

`Xử lý Retransmit: gửi lại dữ liệu nếu ACK không đến`

Gộp chung ACK của DATA\_CHUNK với bình thường được ko do có request id rồi mà?

Có nên xử lý việc upload đa luồng gửi nhiều chunk → bitmap ACK?

- [ ] Bộ code **serialize/deserialize** tối ưu

- [ ] Packet builder \+ reader

Quản lý state với state machine cho từng tcp conn session  
Cơ chế Magic để đảm bảo phân biệt các packet

Làm sao để quản lý những gói tin đã được chuyển đi bởi sender, và callback (hoặc xử lý khi có ACK/ERROR)

1 conn TCP cho các CMD trong UI, upload, download sử dụng 1 conn khác. → **Bỏ lúc upload thì khóa UI (đơn giản hó)**

Ađối với trao đổi server và client thông qua 1 TCP socket thì mỗi lúc chỉ được ghi 1 gói tin vào socket đúng ko? Mỗi hành động ghi hoặc đọc phải là atomic à?

**Mỗi trao đổi của client đến server thì phải có token đúng ko? cho dù cùng TCP conn thì tầng ứng dụng cần phân biệt được gói tin đó cho user nào đúng ko?**  
Đúng, dù cùng một TCP connection, tầng ứng dụng vẫn cần **token** để phân biệt user và xác thực mỗi gói tin, vì TCP chỉ đảm bảo kết nối mạng, không biết gói nào của user nào.

**Chỉ có download upload, thì cần tạo thread mới ở server còn lại dùng select().**  
1 thread chỉ làm I/O bằng epoll

Khi cần xử lý file/hash/logic nặng → đẩy vào thread pool

Trả kết quả lại cho event loop để gửi về client

**Server, client break down thành chunk như thế nào? Và giữ, buffer, như nào… trong quá trình truyền?**

# \*State Machine

# Framing

# **Tổng quan framing**

Mỗi message trên TCP dùng **length-prefix \+ type \+ payload**:

\[Total\_length\]\[MsgType\]\[Header\]\[Payload\]

* total\_length: uint32 network-order, tính từ byte `msg_type` đến hết payload (không tính 4 bytes length).  
* msg\_type: 1 byte code loại message. (DATA, CMD, RESPOND)  
* header\_fields: tùy loại message; cố gắng giữ header cố định kích thước thấp (ví dụ 8–16 bytes) để parse nhanh.  
  * **AUTH: \[RequestId\]\[AUTH\_TOKEN\] → dùng để resume auth, hoặc lần khác ko cần đăng nhập**  
  * **CMD: \[RequestId\] → mọi CMD đều cần chờ RES**  
  * **RESPOND: \[RequestId\]\[Status\] → dùng cho phản hồi cho CMD, AUTH, DATA**  
  * **DATA:\[RequestId\]\[FileId\]\[ChunkIndex\]\[ChunkLength\]→ mỗi DATA cũng tưng tự như CMD cũng cần 1 RES để accept. Nghĩa là lúc upload sẽ upload từng DATA một. CÒn lúc download thì yêu cầu từng DATA một.**  
    * **RequestId là Id khác nhau giữa các DATA**

# Payload

Sử dụng JSON ở phần payload → dễ serialize, và có thể chứa những thông tin phức tạp và dài như danh sách các file trong folder  
Binary cho truyền file

Status: 1 byte (0=OK,1=Corrupt)

# Header

# Message Type

[https://docs.google.com/spreadsheets/d/18WYC2hvCD4IAGy9LeZUpPOFvWE\_FGEacISxV5RREYFg/edit?usp=sharing](https://docs.google.com/spreadsheets/d/18WYC2hvCD4IAGy9LeZUpPOFvWE_FGEacISxV5RREYFg/edit?usp=sharing)

# Tạo Payload với Json

# Tạo payload

Nếu payload là **JSON**, bạn gần như **không cần serialize thủ công** – JSON vốn đã là chuỗi byte (UTF-8), chỉ cần gửi **dạng text**. Cách làm:

### **1\. Tạo payload**

Ví dụ struct trong C:

\#include \<cjson/cJSON.h\>

// Tạo JSON  
cJSON \*root \= cJSON\_CreateObject();  
cJSON\_AddStringToObject(root, "name", "Alice");  
cJSON\_AddNumberToObject(root, "age", 30);

char \*json\_str \= cJSON\_PrintUnformatted(root); // tạo chuỗi JSON  
uint32\_t payload\_len \= strlen(json\_str);       // số byte

* Payload là `json_str`  
* Length \= số byte JSON (`strlen`)

---

### **2\. Gửi packet**

send\_packet(fd, 1, json\_str, payload\_len);

* `type = 1` để nhận biết đây là JSON  
  `payload = json_str`  
  `length = payload_len`

---

### **3\. Nhận packet**

* Đọc header → length → read payload  
* Payload bây giờ là **chuỗi JSON UTF-8**  
* Parse lại bằng JSON parser:

char \*recv\_payload \= malloc(payload\_len \+ 1);  
read\_exact(fd, recv\_payload, payload\_len);  
recv\_payload\[payload\_len\] \= '\\0'; 

cJSON \*root \= cJSON\_Parse(recv\_payload);  
if (root) {  
    const cJSON \*name \= cJSON\_GetObjectItem(root, "name");  
    const cJSON \*age \= cJSON\_GetObjectItem(root, "age");  
    printf("Name=%s, Age=%d\\n", name-\>valuestring, age-\>valueint);  
    cJSON\_Delete(root);  
}  
free(recv\_payload);

---

### **4\. Ưu/nhược**

* ✅ Dễ dùng, dễ mở rộng, không cần serialize từng field.  
* ✅ Độc lập ngôn ngữ – client/server khác ngôn ngữ vẫn parse được.  
* ❌ JSON text hơi lớn, parse chậm hơn binary.

# Payload mẫu

[Message Payload](https://docs.google.com/spreadsheets/d/18WYC2hvCD4IAGy9LeZUpPOFvWE_FGEacISxV5RREYFg/edit?usp=sharing)

# Truyền tin trên TCP

# Luồng tổng quát

## **0\) Tạo 1 TCP Connection giữa sender và receiver**

## **A) Khi gửi (WRITE)**

**Bạn *luôn* làm 3 bước:**

### **① Đóng gói message**

Từ thông tin header, payload dạng Json

Bạn tạo một buffer:

\[header\]\[payload\]

### **② Serialize vào một mảng byte**

* copy N byte header  
* copy payload 

### **③ Gửi qua socket bằng write\_all()**

(send nhiều lần cho đến khi hết)

---

## **B) Khi nhận (READ)**

Bạn *luôn* làm 3 bước ngược lại:

### **① Đọc đúng N byte length header (read\_exact)**

→ thông qua trường length của header biết phải đọc thêm bao nhiêu byte.

### **② Đọc đúng length byte tiếp theo**

→ nhận payload có kích thức length

### **③ Chuyển payload đến handler/ xử lý logic dựa vào trường type của N.**

→ đưa vào logic (chunk/ACK/…), chuyển đến handler,... không liên quan TCP.

# Hàm hỗ trợ

# Hàm hỗ trợ

* **read\_exact(sock, buf, n)**: loop cho đến khi đã đọc đủ n byte hoặc lỗi/EOF.  
* **write\_all(sock, buf, n)**: loop cho tới khi gửi hết n byte hoặc lỗi.

### Hàm gửi chuẩn (send\_all)

`ssize_t write_all(int fd, const void *buf, size_t len) {`

    `size_t sent = 0;`

    `while (sent < len) {`

        `ssize_t n = send(fd, (char*)buf + sent, len - sent, 0);`

        `if (n <= 0) return n;`

        `sent += n;`

    `}`

    `return sent;`

`}`

### Hàm nhận chuẩn (recv\_all)

`ssize_t` read\_exact`(int fd, void *buf, size_t len) {`  
    `size_t received = 0;`  
    `while (received < len) {`  
        `ssize_t n = recv(fd, (char*)buf + received, len - received, 0);`  
        `if (n <= 0) return n;`  
        `received += n;`  
    `}`  
    `return received;`  
`}`

# Lúc gửi gói tin

**`Trước đó gói tin đã được gán payload thành JSON.`**

`// Hàm gửi packet`  
`int send_packet(int fd, uint8_t type, const void *payload, uint32_t payload_len) {`  
    `PacketHeader header;`  
    `header.type = type;`  
    `header.length = htonl(payload_len); // convert length sang network byte order`

    `// Gửi header`  
    `if (write_all(fd, &header, sizeof(header)) < 0) return -1;`

    `// Gửi payload`  
    `if (payload_len > 0 && write_all(fd, payload, payload_len) < 0) return -1;`

    `return 0; // thành công`  
`}`

# Handle lúc nhận gói tin

# Handle lúc nhận gói tin

TCP chỉ đưa byte liên tục → bạn phải tự tạo message.

Mỗi lần gửi: serialize thành `[header N byte][payload]` và write\_all.

Mỗi lần nhận: read\_exact N byte   
→ lấy giá trị trường length → biết bao nhiêu byte → read\_exact tiếp → payload  
→ lấy giá trị trường type → chuyển payload đến handle tương ứng.

# **Quy trình nhận packet hoàn chỉnh**

    // Đọc header  
    if (read\_exact(fd, \&header, sizeof(header)) \<= 0\) return \-1;

    uint32\_t payload\_len \= ntohl(header.length);  
    uint8\_t \*payload \= malloc(payload\_len);  
    if (\!payload) return \-1;

    // Đọc payload  
    if (read\_exact(fd, payload, payload\_len) \<= 0\) {  
        free(payload);  
        return \-1;  
    }

# Tại sao?

# **Tại sao cách này là “chuẩn”?**

### **✔ Không bao giờ lệch packet (vì bạn đọc đúng length)**

### **✔ Không bị split TCP (vì recv\_all đọc đủ)**

### **✔ Không dính padding struct khi sử dụng serilize Struct**

### **✔ Hỗ trợ mọi nền tảng (endianness)**

### **✔ Tránh over-read, tránh crash**

### **✔ Không giới hạn số lượng payload khác nhau**

### **✔ Dễ mở rộng như Google Drive/Dropbox/SSH**

# Upload file

`Luồng ví dụ:`  
		`Sender → gửi Chunk #10`

`Receiver → tính checksum ok → gửi ACK(10)`

`Sender → bỏ chunk khỏi queue pending`

`Nếu timeout (vd 5 giây) mà không có ACK: Gửi lại chunk 10 (retransmit)`

`Xử lý reconnect`

# Chunking

# Chunking

Chunking: Chia file cần truyền tải thành chunks kích thước cố định (ví dụ 64 KiB \= 65536\) trừ chunk cuối cùng

**Kích thước chunk cố định do bên gửi quyết định**, nhưng chunk cuối cùng thường nhỏ hơn để vừa với file. Client và server chỉ cần thống nhất kích thước chunk chuẩn.  
**Tác dụng:**

* Truyền file qua mạng ổn định  
* File vài GB mà đọc cả file vào RAM cùng lúc → rất tốn bộ nhớ. Chunk nhỏ giúp client/server đọc và xử lý từng phần, không cần nạp toàn bộ file.  
* Hỗ trợ resume / tải tiếp tục  
* Kiểm tra lỗi từng phần  
* Song song hóa, tăng tốc.

**FILE\_META xác định chunkSize để cả hai bên thống nhất.**

Mỗi chunk gửi như FILE\_CHUNK độc lập (có index). Cho phép **parallel / pipelined sending**.

# Retransmit

**Reliability & retransmit**

**Retransmit: gửi lại dữ liệu nếu ACK không đến**

* Khi gửi chunk, sender giữ chunk trong buffer đến khi nhận CHUNK\_ACK (status 0).  
* Nếu ACK không đến trong timeout T (ví dụ 5s), sender retransmit chunk (tối đa N lần). Tùy ứng dụng, dùng exponential backoff.  
* Nếu CHUNK\_ACK trả status=1 (corrupt), sender resend ngay.

# \*Reconnect

# Reconnect

Khi heartbeat gửi ko có phản hồi → bị mất kết nối với server. Lúc kích hoạt reconnect (bằng ấn nút trên UI) → thực hiện quá trình reconnect.  
Tạo conn mới đến server.

`Khi client kết nối lại:`

* `Reauth → OK ->`   
* `Kiểm tra onGoingSesssion.`  
* `Gửi yêu cầu resume session đến server.`  
* `Server báo đã có chunk 0–532`  
* `Client gửi tiếp chunk 533 trở đi`

`Reconnect logic giúp không phải gửi lại từ đầu.`

`Yêu cầu:`

+ `Lúc tái khởi động lại conn, RE_AUTH, SESSION_RESUME (kiểm tra xem có session nào đang chạy ko)`

# **Reconnect logic**

* Khi client reconnects, gửi HELLO, AUTH (nếu cần) rồi gửi REQUEST\_RESUME hoặc hỏi server: "server đã nhận tới chunkIndex nào của fileId?" Server trả danh sách chunk đã có (bitmap, last contiguous index, hoặc highest index).

* Sender chỉ gửi các chunk còn thiếu.

# **Kịch bản resume nhanh (practical)**

* Server lưu lại bitmap các chunk đã nhận cho mỗi fileID (ví dụ trên disk or DB).  
* Khi reconnect, client gửi REQUEST\_RESUME(fileId) \-\> server trả bitmap  
  Client chỉ gửi các chunk trong missing ranges.

## Giải thích

1. Bitmap

Là một mảng bit, mỗi bit tượng trưng cho một chunk.  
Bit \= 1 → server đã có chunk đó.  
Bit \= 0 → server chưa có chunk đó  
Ví dụ: nếu file chia thành 8 chunk, bitmap 10110100

2\. Last contiguous index (LCI)  
Là chỉ số chunk liên tục cuối cùng mà server đã nhận từ đầu file.  
Ví dụ, nếu server có chunks 0, 1, 2, 3, 5, 6:  
last contiguous index \= 3 (vì 0–3 liên tục)  
Những chunk sau LCI mà server có, nhưng không liên tục (ví dụ chunk 5, 6\) sẽ cần bitmap hoặc danh sách riêng để mô tả.  
Ưu điểm: đơn giản, tốt cho download tuần tự.

3\. Highest index → tệ

* Là **chỉ số chunk lớn nhất mà server đã nhận**, không quan tâm đến sự liên tục.  
* Ví dụ, server có chunks 0, 1, 2, 5, 6:  
  * highest index \= 6  
* Dùng để client biết khoảng “giới hạn trên” mà server đang sở hữu

# **`Client phải lưu sessionId của mọi upload/download đang chạy`**

Khi bắt đầu upload/download:

CMD: UPLOAD\_INIT → requestId \= X

Client phải lưu vào local memory

ongoingSessions\[\] \= {

   sessionId,  
   fileId,  
   type: upload,  
   path: "/path",  
   lastChunkSent: m, // cho đầy đủ chứ chủ yếu lấy từ server để gửi lại  
   lastChunkReceived: n // dùng khi là sessionDownload. \-\> SoT  
   expriedTime:...  
}

`Nếu bạn đang hỏi “TCP connection session state trên server cần lưu những trường gì để quản lý AUTH + SESSION_RESUME + UP/DOWN + Heartbeat?”, thì đây là danh sách tối thiểu nhưng đủ, đúng thực tế và không thừa một byte.`

`Tôi chia thành 3 nhóm cho rõ:`  
 `(1) Connection-level state`  
 `(2) Upload/Download session state`  
 `(3) Heartbeat/timeout state`

---

# **`🔥 1) CONNECTION SESSION STATE (gắn với từng TCP connection)`**

`Đây là state tồn tại trong suốt vòng đời TCP connection, trước cả upload.`

`struct ConnectionSession {`  
    `connId: uint64                // optional: id nội bộ server cấp`  
    `userId: uint32                // sau khi AUTH xong`  
    `isAuthenticated: bool         // AUTH thành công hay chưa`

    `currentRequestId: uint32?     // sessionId mà connection đang phục vụ`  
                                  `// null nếu chưa SESSION_INIT/RESUME`

    `lastHeartbeatTs: timestamp    // kiểm tra chết/kẹt`  
    `createdAt: timestamp          // phục vụ logging`  
    `remoteAddr: string            // IP:port, để debug hợp lệ`  
`}`

`➡ Đây là tất cả state bạn cần để quản lý một TCP connection.`  
 `Những cái còn lại đều nằm ở session upload/download (bên dưới).`

---

# **`🔥 2) PER–UPLOAD/DOWNLOAD SESSION STATE`**

`Cái này được lưu riêng, không phụ thuộc connection.`  
 `Lý do: để resume được sau disconnect.`

`struct FileSession {`  
    `requestId: uint32`  
    `userId: uint32                // xác minh user đúng session`  
    `fileId: UUID`  
    `type: UPLOAD | DOWNLOAD`

    `totalChunks: uint64`  
    `lastCompleteChunk: uint64     // upload resume: N`  
    `receivedBitmap: bitset?       // optional: dùng nếu hỗ trợ out-of-order`

    `nextChunkToSend: uint64       // download resume`  
    `localTempFile: string`

    `expireAt: timestamp`  
    `createdAt: timestamp`  
`}`

`⮕ Điều quan trọng:`  
 `FileSession không nằm trong connection, mà nằm trong một map global:`

`map<requestId, FileSession>`

`→ Connection disconnect không làm mất session.`

---

# **`🔥 3) Heartbeat / timeout state`**

`Không cần quá nhiều biến, chỉ cần:`

`ConnectionSession.lastHeartbeatTs`

`Rules:`

* `Mỗi khi nhận bất kỳ packet hợp lệ → cập nhật timestamp.`

* `Hoặc nếu bạn muốn strict: chỉ khi nhận HEARTBEAT packet.`

`Server có job 1s/5s:`

`if (now - lastHeartbeatTs) > HEARTBEAT_TIMEOUT:`  
    `close connection`

---

# **`⭐ Tổng kết — 3 nhóm trường server cần lưu`**

## **`A. Connection-level (gắn với TCP)`**

`✔ userId`  
 `✔ isAuthenticated`  
 `✔ currentRequestId`  
 `✔ lastHeartbeatTs`  
 `✔ connId (optional)`  
 `✔ createdAt`

## **`B. Session-level (persisted, independent of TCP)`**

`✔ requestId`  
 `✔ userId`  
 `✔ fileId`  
 `✔ type (upload/download)`  
 `✔ totalChunks`  
 `✔ lastCompleteChunk`  
 `✔ receivedBitmap (optional)`  
 `✔ nextChunkToSend`  
 `✔ temp file path`  
 `✔ expireAt`

## **`C. Không cần lưu gì khác để resume hoạt động mượt mà.`**

---

# **`Nếu muốn tôi có thể viết:`**

* **`struct C++/C`** `cho các state này,`

* `hoặc bản tối giản nếu bạn muốn giảm RAM,`

* `hoặc bản nâng cấp nếu bạn muốn hỗ trợ song song nhiều file trên 1 connection.`

`Chỉ cần nói: "viết struct dạng C" hoặc "tối giản RAM" hoặc "nhiều session/1 connection".`

# **`🔥 1) Lưu Connection State → RAM-only (không được persist)`**

`Vì TCP connection chết là mất — không có lý do gì để persist.`

`Bạn lưu vào một ConnectionTable nằm trong server memory:`

`unordered_map<ConnPtr, ConnectionSession> connectionTable;`

`Một entry tồn tại khi:`

* `TCP ACCEPT`

* `Xóa khi CLOSE hoặc TIMEOUT`

`RAM là đủ vì:`

* `Connection state nhỏ`

* `Chết mạng → connection auto out → state auto free`

* `Không cần bền vững`

**`Không dùng DB, không dùng file.`**  
 **`Chỉ để trong RAM.`**

---

# **`🔥 2) Lưu File Session (UPLOAD/DOWNLOAD) → Persistent + In-memory cache`**

`Đây là thứ phải sống sót khi client reconnect.`  
 `Do đó bạn cần 2 lớp:`

## **`A. In-memory cache (nhanh)`**

`unordered_map<uint32_t, FileSession> sessionCache;`

`Server lookup cực nhanh khi client gửi SESSION_RESUME.`

## **`B. Persistent checkpoint (bền vững)`**

`Để bảo đảm khi:`

* `server restart`

* `crash`

* `app deploy`

`→ session vẫn còn.`

`Bạn có 3 lựa chọn:`

---

# **`⭐ Lựa chọn tốt nhất: SQLite`**

**`Đơn giản – bền – ACID – không phụ thuộc network.`**

`Bảng:`

`CREATE TABLE file_session (`  
    `request_id        INTEGER PRIMARY KEY,`  
    `user_id           INTEGER,`  
    `file_id           TEXT,`  
    `type              TEXT,`  
    `total_chunks      INTEGER,`  
    `last_complete_chunk INTEGER,`  
    `bitmap            BLOB,`  
    `next_chunk_to_send INTEGER,`  
    `temp_path         TEXT,`  
    `expire_at         INTEGER,`  
    `updated_at        INTEGER`  
`);`

`Khi mỗi chunk arrive → cập nhật last_complete_chunk, bitmap, updated_at.`

`SQLite là lựa chọn chuẩn cho protocol server mini.`

---

# **`⭐ Lựa chọn đơn giản nhất: JSON/flat-file checkpoint`**

`Cứ mỗi 10 giây hoặc sau mỗi chunk → flush ra file:`

`sessions/`  
   `12345.json`  
   `12346.json`  
   `12347.json`

`Nội dung:`

`{`  
  `"requestId": 12345,`  
  `"userId": 77,`  
  `"lastCompleteChunk": 532,`  
  `"bitmap": "1011010010...",`  
  `...`  
`}`

`Nhược: I/O nhiều, dễ lỗi khi crash giữa chừng.`

---

# **`⭐ Lựa chọn enterprise: Redis`**

`Nếu bạn có cluster:`

* `put session state vào Redis → auto persist RDB/AOF`

* `RAM speed`

* `survive restarts`

`Nhưng thực tế ứng dụng cá nhân/nhỏ → SQLite là đủ.`

---

# **`🔥 3) Lưu bitmap như nào?`**

`Nếu file lớn, bitmap có thể hàng nghìn/hàng chục nghìn bit.`

`Bạn có thể lưu:`

* `dạng binary blob`

* `hoặc base64`

* `hoặc run-length encoding`

`Tôi khuyên binary blob.`

`std::vector<uint8_t> bitmap;`

`Trong SQLite → BLOB.`

---

# **`🔥 4) Kết hợp “RAM cache + persistent checkpoint” như thế nào?`**

`Luồng xử lý chuẩn:`

### **`Khi RECEIVE chunk:`**

1. `Load session từ RAM`

2. `Cập nhật lastCompleteChunk, bitmap`

3. `Append chunk vào temp file`

4. **`Async checkpoint`** `ra SQLite`

### **`Khi SESSION_RESUME(requestId):`**

1. `Lookup từ RAM`

2. `Nếu RAM không có → load từ SQLite`

3. `Trả lastCompleteChunk cho client`

### **`Khi server restart:`**

1. `Load toàn bộ session từ SQLite vào RAM sessionCache`  
    `← chỉ load những session chưa expire.`

---

# **`🔥 5) Tóm tắt — bạn cần lưu như này`**

## **`A. Connection state`**

* `Lưu trong RAM`

* `Không persist`

* `Tự động chết khi TCP chết`

## **`B. Session state`**

* `RAM cache → cho tốc độ`

* `SQLite hoặc Redis → để survive restart`

* `Sau mỗi chunk → checkpoint ra persistent storage`

---

# **`Nếu bạn muốn tôi viết:`**

* `full code C/C++ struct + save/load SQLite`

* `full pseudo-code cho session manager`

* `state diagram cho memory/persist sync`

`Chỉ cần nói: “viết code cho session manager” hoặc “viết schema SQLite hoàn chỉnh”.`

# Quản lý upload, download state ở Server

**tôi nghĩ tôi dùng cách json để lưu upload, download session cho nhanh, chứ db với C viết lâu quá. tổ chức như này: tmp/session/\[fileid\].json tmp/fileid.tmp**

# **1\. Cấu trúc session JSON**

Bạn có thể lưu đủ thông tin để resume upload/download:

{  
  “session\_id”: UUID → được trả về cho client khi client yêu cầu UPLOAD\_INIT và lưu lại cho client.  
  "user\_id": 12345,  
  "file\_id": "abcdef" // UUID,  
  "filename": "video.mp4",  
  "file\_size": 104857600,  
  “total\_chunks” …;  
  "receivedChunk": n,  
  “next\_chunk\_to\_send” ; // download  
  “chunkSize”: 63589,  
  "status": "uploading",  
  "last\_update": 1736326200  
}

* `received`: số byte đã nhận → dùng để resume.  
* `status`: "uploading", "completed", "downloading" → cho logic state machine.  
* `last_update`: timestamp, tiện để xóa session cũ nếu client mất kết nối lâu.

# **2\. Tên file**

* `tmp/session/[sessionId].json` → lưu meta

* `tmp/[sessionId].tmp` → lưu nội dung đang upload

* Khi upload xong → rename hoặc move file vào storage chính.

# **3\. Đọc/ghi session**

* Tạo file session khi có yêu cầu download, upload.  
* **Ghi session** sau mỗi chunk upload/download.  
* **Đọc session** khi client reconnect → resume từ receivedChunk.  
* C trong stdlib: `fopen` \+ `fread/fwrite` hoặc dùng thư viện JSON như cJSON, Jansson.

Pseudo-code:

cJSON \*session \= cJSON\_ParseFile(path);  
uint64\_t offset \= cJSON\_GetObjectItem(session, "received")-\>valueint;  
fseek(fp, offset, SEEK\_SET);

**4\. Cleanup**

* Xóa `.tmp` file nếu client bỏ giữa chừng sau N phút.

* Xóa session JSON cũ (\> 1 ngày chẳng hạn).

# Xử lý ACK/ERR

# Xử lý ACK 

Sender maintain **map ID → callback / trạng thái** để xử lý ACK khi nhận.

### **1\. Đếm tăng dần (incremental counter)**

* Mỗi session (connection TCP) có **một biến counter** bắt đầu từ 1 hoặc 0\.

* Mỗi lần gửi CMD mới, tăng counter lên 1 → dùng làm `seq_id`.

* Ví dụ:

`uint32_t next_seq_id = 1; // khởi tạo khi mở connection`

`uint32_t get_next_id() {`  
    `return next_seq_id++;`  
`}`

→ Dùng cho id trong Header của các gói tin. ACK thì dùng lại id của các gói tin request.

# \*Download Chunk

# Download Chunk

Gửi request từng CHunk một → nhận DATA

# \*Lưu token và hạn

* File JSON hoặc plain text, ví dụ `~/.myapp_auth.json`

`{`  
  `"token": "abcd1234...",`  
  `"expiry": 1736326200`  
`}`

* Khi **khởi động client** hoặc **reconnect**, đọc file vào `AuthSession` → dùng lại nếu chưa hết hạn.  
  FILE \*fp \= fopen("\~/.myapp\_auth.json", "r");  
  if (fp) {  
      fread(buf, 1, sizeof(buf), fp);  
      fclose(fp);  
      parse\_json(buf, \&g\_auth);  
  }  
    
+ Viết một module C để xử lý lưu và load token.  
+ 

# TCP conn state

typedef struct {  
int fd;                     // socket  
pthread\_mutex\_t write\_lock; // mutex bảo vệ write/send

int currentResquestId \= 0;

ConnState state;             // trạng thái state machine hiện tại: CONNECTED, AUTHENTICATING, IDLE, UPLOADING, DOWNLOADING, CLOSING  
time\_t last\_active;          // timestamp lần cuối nhận dữ liệu → dùng cho timeout / heartbeat  
bool logged\_in;              // client đã auth thành công  
uint32\_t user\_id;            // lấy từ AuthToken  
char auth\_token\[256\];        // lưu token (nếu cần xác nhận trong phiên)  
time\_t token\_expiry;         // thời hạn token  
time\_t last\_heartbeat;        // thời điểm nhận heartbeat cuối , những lần request bình thường cũng tính là heartbeat → sau bao nhiêu lâu đó → drop conn

bool busy\_worker;  
uint8\_t buf\[BUF\_SIZE\];  
size\_t buf\_len;

} Conn.

Khởi tạo mutex khi tạo ra Conn.

* Khi tạo `Conn`, khởi tạo mutex:

`pthread_mutex_init(&conn->write_lock, NULL);`

# Hàm gửi safe thread và gửi toàn bộ

ssize\_t send\_exact(int fd, const void \*buf, size\_t len) {  
    size\_t total\_sent \= 0;  
    const char \*p \= buf;

    while (total\_sent \< len) {  
        ssize\_t n \= send(fd, p \+ total\_sent, len \- total\_sent, 0);  
        if (n \< 0\) {  
            if (errno \== EINTR) continue;   // bị signal, retry  
            return \-1;                      // lỗi thật sự  
        }  
        if (n \== 0\) break;                  // socket đóng  
        total\_sent \+= n;  
    }  
    return total\_sent;  
}

// \--- conn\_send: thread-safe gửi toàn bộ dữ liệu \---  
ssize\_t conn\_send(Conn \*c, const void \*buf, size\_t len) {  
    if (\!c) return \-1;

    pthread\_mutex\_lock(\&c-\>write\_lock);  
    ssize\_t sent \= send\_exact(c-\>fd, buf, len);  
    pthread\_mutex\_unlock(\&c-\>write\_lock);

    return sent;  
}

Nếu có nhiều thread gửi dữ liệu trên cùng socket, **mutex \+ send\_exact()** đảm bảo:

1. Chỉ 1 thread gửi tại 1 thời điểm.  
2. Toàn bộ gói tin được gửi đầy đủ, không bị đan lẫn với thread khác.

# Xử lý file nặng ko block server

**Nếu ví dụ 1 conn đang muốn làm một cái lâu như upload file lớn, tôi muốn chuyển nó ra khỏi luồng chính để luồng chính xử lý những việc khác. Thì làm như nào? Và khi nào thực hiện xong upload thì lại gộp conn đó quay lại cơ chế epoll**

Bạn muốn **offload một connection “nặng” (upload/download lớn) ra khỏi event loop chính** để không block `epoll` – về cơ bản là **tách việc I/O heavy ra worker thread**, rồi khi xong lại đưa kết quả về main loop. Đây là cách chuẩn thường dùng:

Main thread (epoll):  
    \- Chỉ quan tâm accept new conn  
    \- Chỉ đọc header / kiểm tra message  
    \- Nếu gói tin “nhẹ” → xử lý luôn  
    \- Nếu gói tin “nặng” (upload/download lớn) → dispatch vào worker

Worker thread:  
    \- Xử lý upload/download file  
    \- Ghi trực tiếp vào temp file  
    \- Cập nhật session struct (shared memory)  
    \- Khi xong → gửi event về main thread

Main thread:  
    \- Nhận thông báo worker xong → có thể tiếp tục gửi các CMD khác.

# Socket I/O

# Socket I/O

## **1\. Ý tưởng tổng quát**

1. **Main thread / main loop**

   * Chỉ quản lý **accept new connection \+ read/write socket nhỏ**

   * Không block vì file lớn upload/download.

2. **Worker thread / thread pool**

   * Xử lý upload/download chunk lớn.  
   * Khi xong, thực hiện trao đổi với client xong task luôn → Hủy luôn thread

---

## **2\. Cách triển khai chi tiết**

### **a. Main loop (select/epoll)**

Conn \*conn; // struct per TCP connection

while (1) {  
    fd\_set read\_fds;  
    FD\_ZERO(\&read\_fds);  
    FD\_SET(listen\_fd, \&read\_fds);

    for each conn in connections:  
        if (\!conn-\>busy\_worker) // chỉ select fd nếu không đang được worker xử lý  
            FD\_SET(conn-\>fd, \&read\_fds);

    select(...);

    if (FD\_ISSET(listen\_fd)) accept\_new\_conn();

    for each conn:  
        if (FD\_ISSET(conn-\>fd)) handle\_read\_write(conn);

    // check events từ worker thread (upload done, download done)  
    handle\_worker\_events();  
}

* `conn->busy_worker` \= true nếu conn đang được worker xử lý upload → loại khỏi fd set main loop.

---

### **b. Worker thread**

* Khi main loop phát hiện upload lớn → đẩy `Conn*` vào **worker queue**.  
* Worker thread:

void \*upload\_worker(void \*arg) {  
    Conn \*c \= arg;  
    for each chunk:  
        read from conn-\>recv\_buf / TCP  
        write vào file temp  
        update session-\>received\_chunks  
    c-\>busy\_worker \= false;   // mark xong  
    signal\_main\_thread(c);    // thông báo main loop  
}

* Worker không trực tiếp gọi `select()` hay block main loop.  
* Có thể dùng **mutex \+ queue** để gửi thông báo hoàn tất về main thread.

---

### **c. Thông báo main loop**

* Khi worker xong → main loop nhận event → tiếp tục select fd đó.  
* Có thể dùng pipe/eventfd:

// worker write(fd\_event, \&conn, sizeof(conn));

* Main thread đọc pipe → đánh dấu `conn->busy_worker = false` → lại FD\_SET trong select/epoll.

---

## **3\. Lợi ích**

* Main loop **không bị block**, vẫn xử lý các client khác.  
* Worker thread có thể upload/download file lớn **theo chunk**.  
* Kết hợp select \+ thread pool → scalable cho nhiều connection đồng thời.

### **4\. Gợi ý nâng cao**

* **Chunk-based**: worker đọc từng chunk → trả control cho main loop → giảm mutex contention.  
* **Thread pool**: tránh tạo thread mới cho mỗi upload.  
* **Lock-free queue**: đẩy `Conn*` giữa main thread ↔ worker.

Nếu bạn muốn, tôi có thể viết **skeleton C code** mô tả **main loop \+ worker thread \+ resume select()** cho upload file lớn, áp dụng trực tiếp cho hệ thống TCP session của bạn. Bạn có muốn tôi làm luôn không?

# Khởi tạo worker thread để thực hiện task nặng

\#include \<stdio.h\>

\#include \<stdlib.h\>

\#include \<pthread.h\>

\#include \<unistd.h\>

\#include \<stdbool.h\>

typedef struct {

    int fd;

    size\_t total\_size;

    size\_t received;

} Conn;

// \--- function worker \---

void \*upload\_worker(void \*arg) {

    Conn \*c \= (Conn \*)arg;

    printf("Worker: start upload for fd %d\\n", c-\>fd);

    while (c-\>received \< c-\>total\_size) {

        size\_t chunk \= (c-\>total\_size \- c-\>received \> 1024\) ? 1024 : c-\>total\_size \- c-\>received;

        usleep(1000); // simulate work

        c-\>received \+= chunk;

    }

    printf("Worker: upload done for fd %d\\n", c-\>fd);

    // thread tự exit, tài nguyên sẽ được OS giải phóng vì thread detached

    return NULL;

}

// \--- push task \---

void start\_upload\_task(Conn \*c) {

    pthread\_t tid;

    pthread\_attr\_t attr;

    pthread\_attr\_init(\&attr);

    pthread\_attr\_setdetachstate(\&attr, PTHREAD\_CREATE\_DETACHED);

    if (pthread\_create(\&tid, \&attr, upload\_worker, (void \*)c) \!= 0\) {

        perror("pthread\_create");

    }

    pthread\_attr\_destroy(\&attr);

}

int main() {

    Conn c \= { .fd \= 1, .total\_size \= 10000, .received \= 0 };

    start\_upload\_task(\&c);

    // main thread vẫn chạy tiếp, có thể xử lý các fd khác

    sleep(1); // giả lập main loop

    printf("Main thread continues\\n");

    sleep(2); // chờ worker xong

    return 0;

}

# Xử lý các chức năng

# Đăng nhập và phiên

# Đăng nhập, phiên đăng nhập \- token

username, password  
Sử dụng token để quản lý phiên.  
Mỗi message gửi kèm token. 

### **Mô hình quản lý phiên đề xuất (simple token-based session)**

#### **1\. Khi đăng nhập**

Client gửi:

 `{`  
  `"action": "login",`  
  `"username": "khoi",`  
  `"password": "123456"`  
`}`

* Server kiểm tra username/password trong DB (`users` table).  
  Nếu hợp lệ, sinh **session\_token** ngẫu nhiên (ví dụ 32 bytes, hex string):  
   `char token[65];`  
  `generate_random_token(token);`  
* Lưu vào **bảng sessions** (hoặc bộ nhớ cache):

| session\_id | user\_id | token | created\_at | expires\_at |
| ----- | ----- | ----- | ----- | ----- |
| 1 | 2 | "a1b2c3..." | now() | now() \+ 1h |

+ Gửi lại client:

   `{ "status": "ok", "token": "a1b2c3..." }`

#### **2\. Khi client gửi yêu cầu sau**

Mọi request đều có token kèm theo:

 `{`  
  `"action": "upload",`  
  `"token": "a1b2c3...",`  
  `"file": "abc.txt"`  
`}`

* Server tra bảng `sessions` hoặc cache để xác thực:  
  * Nếu token tồn tại và chưa hết hạn → cho phép.  
    Nếu hết hạn hoặc không tồn tại → trả lỗi `"unauthorized"`.

# Luồng Upload file

# Luồng Upload file

Tuyệt vời — đây chính là phần “xương sống” của hệ thống chia sẻ file qua TCP mà bạn đang thiết kế.  
 Một **luồng upload** tốt không chỉ truyền được file mà còn:

* hỗ trợ **resume khi bị ngắt**,  
* xử lý được **file lớn (vài GB)**,  
* và đảm bảo **hash integrity** mà không phải giữ toàn bộ file trong RAM.

## **Mục tiêu thiết kế**

| Yêu cầu | Giải pháp |
| ----- | ----- |
| File lớn (GB) | Chia thành **chunk** |
| Mất kết nối giữa chừng | Có **session upload** để resume |
| Tính hash toàn file | Hash dần theo luồng (incremental hashing) |
| Tránh chiếm RAM | Ghi ra **file tạm** từng chunk |
| Bảo mật | Token \+ checksum từng phần \+ hash tổng |

---

## **Cấu trúc tổng quan luồng upload**

Client gửi lần lượt các request.  
Client: Khởi tạo một TCP conn đến server

* **upload\_init**  // yêu cầu bắt đầu upload session  
* upload\_chunk (chunk1)  
* upload\_chunk (chunk2)  
* …  
* **upload\_complete** // thông báo kết thúc upload

         
Server

* Khởi tạo session upload  
*  Nhận từng chunk  
* Ghi các chunk vào file tạm \+ **hash luỹ tiến**  
* **Bắt đầu tính thời gian timeout (tầm 1p) → hết timeout ko nhận gói tin → hủy session upload này.**  
* Khi hoàn tất → hợp nhất, kiểm tra hash  
* Lưu metadata vào DB \+ storage  
* Gửi thông báo thành công cho client.

## **Giai đoạn chi tiết**

## **Bước 1: Khởi tạo phiên upload**

**Client → Server**

{  
  "action": "upload\_init",  
  "token": "abc123",  
  "filename": "report.pdf",  
  "folder\_id": "fld42",  
  "size": 524288000,  
  "chunk\_size": 10000  
}

**Server xử lý:**

1. Kiểm tra quyền `write` trong thư mục:

Tạo bản ghi trong DB (`upload_sessions`):

 id \= "sess\_7F12",  
status \= "in\_progress",  
current\_offset \= 0,  
hash\_state \= init,  
tmp\_path \= /data/tmp/sess\_7F12/

2. Trả về:

{  
  "status": "ok",  
  "session\_id": "sess\_7F12",  
  "chunk\_size": 1048576  
}

### **🔹 Bước 2: Gửi từng chunk**

**Client → Server**

{  
  "action": "upload\_chunk",  
  "session\_id": "sess\_7F12",  
  "offset": 1048576,  
  "data": "\<base64-encoded\>"  
}

**Server:**

1. Ghi dữ liệu chunk vào file tạm:  
    `/data/tmp/sess_7F12/chunk_0001.part`

2. Cập nhật trạng thái:  
   * `current_offset += chunk_size`

3. Cập nhật hash luỹ tiến.

---

## **Hash khi file được gửi từng phần**

Bạn **không cần** phải đợi toàn bộ file mới tính hash\!  
 Có thể dùng **incremental SHA-256** (hash dần từng chunk).

### **Cách thực hiện trong C (OpenSSL):**

SHA256\_CTX ctx;  
SHA256\_Init(\&ctx);

while (recv\_chunk(...)) {  
    SHA256\_Update(\&ctx, chunk\_data, chunk\_size);  
}  
SHA256\_Final(hash, \&ctx);

* `SHA256_Update()` có thể gọi nhiều lần — mỗi lần thêm dữ liệu mới.  
* `SHA256_Final()` chỉ gọi một lần sau khi file kết thúc.

Như vậy bạn hash song song với quá trình ghi chunk, không cần giữ cả file trong RAM.

## **Hủy upload khi bị ngắt giữa chừng**

**Client phát hiện ngắt** → kết nối lại → gửi:

{  
  "action": "resume\_upload",  
  "session\_id": "sess\_7F12"  
}

**Server kiểm tra DB:**

SELECT current\_offset FROM upload\_sessions WHERE id='sess\_7F12';

**Server trả về:**

{  
  "status": "ok",  
  "next\_offset": 3145728  
}

→ Client tiếp tục gửi từ `offset=3145728`.

---

## **Khi upload hoàn tất**

**Client → Server**

{  
  "action": "upload\_complete",  
  "session\_id": "sess\_7F12"  
}

**Server xử lý:**

1. Merge các chunk `.part` thành 1 file tạm.  
2. Kết thúc hash (`SHA256_Final()`).  
3. Đặt tên file \= hash.  
4. Di chuyển file sang thư mục vật lý `/data/storage/7f/7f3a9f0b8a23...`.

Update DB:

 INSERT INTO files (filename, storage\_path, owner\_id, folder\_id, size, version)

5.   
6. Xoá thư mục `/data/tmp/sess_7F12/`.

**Server → Client**

{  
  "status": "ok",  
  "file\_id": "file\_1024",  
  "hash": "7f3a9f0b8a23..."  
}

## **Cấu trúc bảng upload\_sessions (để resume)**

| cột | ý nghĩa |
| ----- | ----- |
| session\_id | UUID |
| user\_id | ai đang upload |
| filename | tên gốc |
| tmp\_path | thư mục lưu chunk |
| current\_offset | vị trí cuối cùng đã nhận |
| hash\_state | serialize SHA\_CTX |
| status | in\_progress / complete / error |
| created\_at | thời điểm bắt đầu |

---

## **Bảo mật và kiểm tra toàn vẹn**

* **Hash toàn file**: server tính SHA256 cuối cùng và so sánh với client gửi trong `upload_complete`.

* **Token validation**: mỗi request (kể cả `upload_chunk`) đều phải có token.

* **Timeout**: session tự hủy nếu không có chunk mới trong N phút.

---

## **🧩 9️⃣ Ví dụ luồng cụ thể**

Client → Server: upload\_init (file=report.pdf)  
Server → Client: session\_id=sess\_7F12

Client → Server: upload\_chunk offset=0  
Client → Server: upload\_chunk offset=1MB  
...  
(connection lost)

→ Client reconnects  
Client → Server: resume\_upload(sess\_7F12)  
Server → Client: next\_offset=3MB

Client → Server: upload\_chunk offset=3MB  
...  
Client → Server: upload\_complete  
Server → Client: hash=7f3a9f0b8a23..., file\_id=42

---

## **Tổng hợp thiết kế**

| Thành phần | Vai trò |
| ----- | ----- |
| **upload\_init** | Tạo session, xác thực quyền |
| **upload\_chunk** | Nhận và ghi file theo offset, cập nhật hash |
| **resume\_upload** | Cho phép gửi tiếp khi ngắt |
| **upload\_complete** | Merge file, finalize hash, update DB |
| **hash incremental** | Dùng `SHA256_Update` để tính hash song song |
| **temporary directory** | `/data/tmp/session_id/` |
| **final storage** | `/data/storage/<hash_prefix>/<hash>` |

---

Nếu bạn muốn, mình có thể **vẽ sơ đồ luồng (sequence diagram)** giữa Client ↔ Server thể hiện từng bước upload, resume, hash, và commit vào DB — nhìn cực rõ để trình bày trong báo cáo hoặc đồ án.  
 Bạn có muốn mình vẽ sơ đồ đó không?

# Tại sao không gửi trực tiếp file lớn qua TCP?

* **Có thể gửi trực tiếp dữ liệu lớn**, nhưng TCP tự chia nhỏ segment.  
* Ứng dụng nên gửi theo **chunk vừa phải** (vài KB – vài MB) để:  
  * Không chiếm quá nhiều RAM  
  * Quản lý lỗi dễ hơn  
  * Dễ hiển thị tiến độ upload/download

# Phân quyền

## **Phân quyền (Permission System)**

Bao gồm các quyền sau cho một file:

| Permission | Mô tả | Quyền thao tác |
| ----- | ----- | ----- |
| owner | người tạo (chia sẻ, đối tên, xóa, **di chuyển file**) | full (CRUD, share) |
| write | được chỉnh sửa tên, xóa, chia sẻ file cho người khác, xóa chia sẻ, quyền xem | read, update |
| read | có quyền tải về, truy cập trang để tải về | read-only |
| none | không truy cập. Tuy nhiên khi truy cập vào có quyền được request mở quyền. | từ chối |

Quyền tương tự đối với folder:

- owner: toàn quyền: đổi tên, chia sẻ, xóa, di chuyển folder  
- write: có chuyền đổi tên, upload file lên folder và quyền read. Chí có quyền xóa file mà mình có quyền chỉnh sửa trong folder (owner | write)  
- read: được xem folder nhưng chỉ hiện thị những file đã được chia sẻ với mình trong folder đó  
- none: Từ chối truy cập \+ có thể request mở quyền.   
  - Không thể set up **none** nếu người được chia sẻ folder đã upload file lên

---

## **Luồng “Upload file”**

\[Client\]  
  → send UPLOAD file.txt size=100  
\[Server\]  
  → kiểm tra quyền folder, OK  
  → tạo record mới trong DB (metadata)  
  → tính hashcode cho folder   
  → gọi storage.save(local\_tmp, hash) local\_tmp là vị trí file tạm hiện tại  
  → update record file với storage\_path  
  → trả "OK file\_id"

# Xử lý đa người dùng

# Xử lý đa người dùng

# Upload thư mục

# Upload thư mục

# Download thư mục

(Chưa xử lý)  
Ý tưởng: **Download folder đã được nén thành file rar.**

- [ ] Cần tìm hiểu cách nén thư mục bằng C  
- [ ] Nếu sử dụng ngôn ngữ/thư viện ngôn ngữ khác thì cách có thể kết nối nó với C

# Download file

[image1]: <data:image/png;base64,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tlNOOSXee++9k/aPP/7YtauuMGTtkyeXvxk2T2s3uj6dtVnQmjp1arzqqqvGZ5xxRrz99tsn06onT4nd+PMBgMbCd0/zoO3avvvum2yjvv/++6Sudhk/fnzSpjJu3DjXrvqQIUOqbPNUt22htq1mwIABSbu2haZ9+/auzbaVMmXKlHjGjBnxSiutlDyeLFy4MKlLjetpGJyOr9zmZxROf0r7cIoqGj1oyZdfflklLNWljBkzJpx1rUTBm+7Xf/jhB1e3oKVzfJ122mlp04iC1gMPPODqWtEsOWu8Pb+XX345Xm655dLuJzfddFO8YMECV99xxx2Tdk2joHXnnXe6x1f99ttvz7iyvPvuu/GRRx4ZNgNA3mX6TkLTo6Bl77XtljNW17bKOgB0eJC16/bmm292dYWi+++/P+1+ovtOmDChSrvq06dPj08//fR46623dm3aHto055xzTtyjRw9XVxDr379/cl/zq1/9Kj7xxBPD5kphcFKpSThttunjZh60evfu7VYI6datW9IeeW+0erZ82t342muvJdMoaBmlaGv35zFnzpyM7R999JHb9an5ffPNN0l727Ztk12HvhVWWCFt2H8eANDY+P5pHhS0/G3k2muvndTDdUDbyN/97ndp27xFixYl4y+//PKk3Wy88cbx3//+9yrtClGffvqp2/Z98cUXSXv4mCZs79evX7zKKquktVURhiaV8c+GU5ULpyNoZffjjz/Gw4YNc3X/zYgyBC3tHvT3Bds0DRG0tBJrZTIWtIYPH562cvkr91FHHZX3A/oBoCb+9xmaLm2jFFpMpqCl26uuuirp1cq0zZNsQUvbP2NBq1evXslhOWL31TbS589T9YkTJ3pjqxGGpg9TueKsleP4zsGV0yycV36Kh0f/FMf/vq3qfbIouqCVSThNQwUtiSremGxBq3PnzvGbb77p6tOmTUumaYigJT179kzaNY2C1tVXX510i1q73HvvvXGnTp2SdgAoBP/7DE1XrkHL7LnnnhnbpS5Ba/To0fHyyy/v2ubNm5fc96STTopfeuklVx8xYkTcpUsXV9exXdqtmBM/ME3/rLL9/mPSxylgmTlTCVq1oZVH3ZrZgpboOC298fZGK7k3VNDSsVpaSXbbbbe4Xbt2ya7DuXPnui7bP/3pT8n9NI+wAEBj47unecglaP3000/uWGT9gctvD9eR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9lnafUQ9VxdccEGVdgW1sOdr4cKFyXixcNW9e3d3q1Cm3rFJkybFXbp0Sab76KOP3K3ua1e5t14uAACEoIWipN13kRd+VB8/fnwybLsF/Wn84b333jvpWbIQ5I/3aTfgzjvvHDYn05566qnx6aefHu+zzz7xNddcE0xVGQhbt24djgIANHMELRSlRx99NO7Tp08yrNAUBcHLv5VXXnklHjFiRJX2/v37uwuiSteuXZN2c95558Unn3xyMqz7Llq0KAlzHTt2jGfPnu2ej47P8qdTe/v27d1wr1694i+++CIZDwBAkw5ahx12WFxWVlZjGT58eHg3FIFVVlklvvzyy9PatNvvpJNOcrsKbbee/pWo46LU49SyZUu3G1AiL2i1adMmOcB9q622StqNdhVqeoW59ddfPx4wYED86quvxhtssIEbb/N69tln3TFf+pehDqi/9NJL4wcffDDeZptt3Pj3338/XmGFFZL5AgDQZIPWvvvuWyVUVVcy/YsNhXXggQcmocl8++237iD0++67L+2fhPfee6/rsRo4cGDSdvDBByd1zcsodA0bNiwZNppHahWOjzrqKDd8/fXXu4Phxb+/TuOgnq5bbrnFDSv42XFj4k8LAECTDFphkMpW7N9rKD1Rxe67c845J60XCwCAYtDkglYYonIpBK3S9cknn7hjqPbff39OqwAAKDpNLmgBAAAUi6IMWmGPU74LPVoAACAfii5o6QDkMAjluxC0AABAPhRd0Lr22murBKF8F4JW40utLvEjjzxCKeHCCVoBIDuCVhlBqxAi/iFY8nT+MhQ/XfVgwYIFYTOARkLQKssetA455JDk/Eq50PzsTOQ+nUFcl4OpK52wM2oiAaWpvI7mTEGrrhtwXU5pxowZrq4z89tFuX1aR3744Qd3glrWl8zC5aLl6LcpZOkEu+F0xWL69OlVnls4HFp33XXdCY2ld+/eyXVPdcLiXXbZxf372K7qgObtq6++yvjd0tgIWmX1D1off/xxPGHChGQ4DFo6oaUuSBwGLbV9/vnnybDu991338UffPBBxlMVpBaxu6jxf/7zn3BUydFrQWmzoKX1WJ8Bo/VX9JkIP1vasOrLzw9ayy23XNo0Mnfu3Pjiiy9OhrVBfeqpp7wpIOHnKAxaovdDy93YCXY//PDD5IoJeg+r+xGo6XQhds3HphddQF3vu11c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ZowLW3Uxfvz4eMaMGa4+bNiwJHT59JmaP39+PH369Fp/vpqLcLksWbIkrW3DDTd0yzacrli89tprVZ5bOBzq0aNHvN9++7n6PvvsE/ft29fVW7RoEV9xxRXuO7V169b+XQrm2WefTU4nNHTo0Hjq1KmufsABB7jXGX5+1HbccccldaP7aZjtRWkiaOWgpqB16qmnuttwfmr/17/+ldbm0/QLFy508wYag62ruv3iiy9c/dxzz41vvfVWf7L48ssvj88666zkS33OnDlumpEjR8ann3560m5Ba8SIEW56Y4+jHyFvvPFG0i5jx46N77nnnrSg1b59+7RpZPbs2fG1116bDK+zzjrx6NGjvSkg4fdOGLRE78dzzz2XDF9yySXuPfzjH//olrPccccd1X5fabqPPvrIzcemF/XQ/PnPf45PO+20pM3e++qGfQMGDHCn2fGfr55X+PxD4XgbXmONNZI2PWf1ehWaApV+LJgddtghaR8+fHh88sknJ+MmTpzonnemoCU66bZ6wlB6CFo5qClo2Xzsdu7cua7+4osvxkcffXR86aWX+pM72mBtvPHGrl6b5wHUh9Y1nbz3+eefd/VWrVq59XS11VZLQo/a77///viGG25wdfWGaLdF165dXfuNN96YrLMKWupFGDVqVHzhhRcmGzqNV/tTTz3lrsRw3nnnufaVV17Z9bBo/VevhP+YVo444gjXFtI4212PSuH3hx+0Pv/8c1d/6aWX4u222y6+6KKLXHu/fv3iZZddNn755Zfd+BVWWCF+7LHHXP3OO+/0Z+fovTznnHPiJ554Iu3xtC49+eST8V//+tekXbe2i0xhfsUVV0ymD7333nvu1p+ndnHqudn6YMFu3rx58cCBA5PpzOTJk6ssA4X5sK3QtEuzV69eSc+tgpZ+pPjPU+d41A+Z6oKWhunRqh31lvs/DgqFoJUDBS1Nry8cK8bmY7f61aFuXn0gqvt1pjb7wGlDpI0XkG/+ujhkyJCkri8ihaVQ586d40mTJrmg5V8nVO2ioKXjF034WRBtYDSs4078eXz66adJ0PJpw/ynP/0prU33zxQAUPV7zA9auvV7U6xdQcv0798/fv311119woQJyW44n/8Y6r2qbjevvu+0q0xhWvR+h8dQZRK+Bl9N46r7fhW1F0OPlk/X291xxx1dXUFr0aJFactb24IwaPnb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CFo5OOSQQ9z0frnkkkvcOJtPbeYnr732Wq3vAxQTnTZlwYIFYXNOxo8fH8+YMcPV33777bTPlnn88ceTtm+//TZpR6XwO2TJkiVV2jS8zjrrpLUVk/D5LrPMMsn73qJFi7Rxxsa3atUqY/vVV1+d1l5Iu+66a5V1O2Tjn3nmmXBUMk5l3rx5GdtVHnjgAe9eKCYErRwoaB111FFhc5razE+6desWH3fccfHSpUvDUUBJaKigdfjhhwdjy/kbWX2+uLJDVeH3Thi0+vTp4247d+6ctBWTG264ocprCIdDGn/nnXe6+vXXX5+ELbVPmTLF1du3b59MX0gXXnhh3KtXL1f/6aefXIgM+eu21nmdgkjmz59fZVlo+H//+19SD8fZfVFcCFo5qClo2Xz8+elXh4ZPOumkpM2nD5XG63bDDTcMRwN5oXVu8eLF7vaxxx5zG+UVVlgh3mabbZJp5s6dG6+11lpxly5d4lmzZrk29SbtvPPO8V577RW3a9cunj17tmtX0NLGQOen0/RG8//555/dBuaaa65J2uXMM8+M99hjj7Sg1bJly7Rp5Pvvv082prLpppvGI0eO9KaAhN9jYdD64Ycf3PCf//znpK1fv37uO0rnE3z//fdd20477VTt95Wm07LXfN59992k/auvvoo7duwYL7vssm69kvD5hMM+jevfv3/aNPbdGFK7rS+ZerHkgAMOSNr+8Ic/uGVRaFo2/g+ETK9t6623Turbb799fNddd7m6PhffffddMk70mVxxxRVdPZzXjjvuGJ9zzjlpbSgOBK0cKGhtvPHG8fnnn58UY/Ox27PPPjveYIMNXF0fsEy/JNWVPHz4cFevzfMA6kPr2hlnnOHqq6++enzLLbe4+n//+9/42WefdXV/fdWva32xK2j566nVFbS0YQjbdXvZZZe5+o033hh37do1aVeQk+WXXz7ZcKpdv/xvuummjJ8HC4eoSsslUxGFJxVZtGhR0puioPX666+7eps2bVyIlYsuuijefPPNXd3nL3sFI32vKUife+65VaZRcNdhEXLqqafG1157bTJNdfz5v/fee2541KhR8aGHHurWU9HztwDi23fffdN+KFhQ22GHHbypisOrr74aH3PMMWFzQs/72GOPTRuuicbb9uj000+PO3XqFE7S7GkZnXDCCWFzoyNo5UBBa4sttoivvPLKpBibj93qi0u/xB9++GFXMj2O2uxXjn6dDB06NJgCaHj+ujhkyJCkrh4qbdh8P/74o9swT5o0yQUtf5eHhTEFLfU8mfCzINpdouGZM2e6X/fmm2++SYKWTxv68HtE9//ggw/S2lAu/H7xe7R0q4BirF1Byyg4KWjLhAkT4r59+ybjjP8Yp512WpXDHRS67DtN87J5qOcpl16l8DX4ahqnHwGZdsXJ2muvHU+bNi1sLph33nmnxtdiFFSPP/54V/en/+KLL9ywFRtv26NLL73UDdt7iXL6ManvoEIjaOWgNrsOdWu7VjLRF4/C2Morr5yU2jwXoK789ay6oKVp3nzzTbfxVKCyoGW9UuIHLf8YrfCzIBa0pk+fnnbczJw5c5KgpQ28ue2225Ld6WFPGqoKl08YtGyXng1LGLQsxFYXtPxj5SxoKcApSOl4IdU1b/vxaHU7Piwb/zVovbNjkMJxPq1Lyy23XNicZtVVVw2bCuKqq67K2FOYiR12Iv4y9WX6nIk+Y2EbigNBKwe1CVraLajjWcLxRvPxv0gknAbIB389yxS0Pvzww7R/PWn6hgpaYbv+BaegFR6To7odG6Z62HuCdOF3hx+01ltvvfiCCy5Ixll7QwSt3XffPV64cGHSrnlbKNA/srXb0O/trIn/Gm6//fa4R48eGccZ/ZFojTXWCJvTpr3uuuviN954wxtbGOPGjatyTFnIf95HHHFEvNlmm7n6P/7xjyqvf+LEiRk/T6Ld9dolj0paRoMHDw6bGx1BKwe1CVqiA4Y1rKJdML5Mj6tuYR2PAOSTv+5lClpi6602DjrO5emnn26woPX1118n8997772THq3nn38+abfd6C+99FLSZuWee+6x2aJC+H0SHgyv3nNbfhZaGyJoic1X4/3dhBrvP4fwOYbC8R06dEjmbeFNPT0DBw509XC9sPtbCFHJFm4aS/g87bnqjwQ6LlF07JmNC3eFahe7f19tW0w430x/KkFxIGgBABqM/onq79Y78cQTvbFA49EfBPRjsdAIWgCABqHvVRX1xBidkBYoBPWOPvHEE2FzoyNoAQAA5AlBCwAAIE8IWgAAAHlC0AIAAMgTghYAAECeELQAAADyhKAFAACQJ0UZtBqrAAAA5FPRBS0AAICmgqAFAACQJwQtAACAPCFoAQAA5AlBCwAAIE8IWgAAAHlC0AIAAMgTghYAAECeELQAAADyhKAFAACQJwQtAACAPCFoAQAA5AlBCwAAIE8IWgAAAHlC0AIAAMgTghYAAECeELQAAADyhKAFAACQJwQtAACAPCFoAQAA5EmDBq0UzYxCoVAoFAqFUlkAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIHf/DwZtLvyFq5GbAAAAAElFTkSuQmCC>

[image2]: <data:image/png;base64,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>