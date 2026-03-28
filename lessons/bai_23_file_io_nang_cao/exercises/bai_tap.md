# Bài Tập - Bài 23: File I/O Nâng Cao

## Bài tập 1: Data Logger Binary

Viết chương trình data logger cho BBB:
1. Đọc giá trị "giả lập" từ `/dev/urandom` (2 bytes = 16-bit ADC value)
2. Mỗi sample: record `{timestamp_ms, adc_value, sequence_num}`
3. Ghi 100 sample vào file binary `sensor_data.bin`
4. Viết tool đọc lại file và in dạng CSV

```c
typedef struct {
    uint64_t timestamp_ms;
    uint16_t adc_value;
    uint32_t seq;
} __attribute__((packed)) Sample;
```

**Yêu cầu**: Dùng `O_SYNC` khi ghi để đảm bảo data an toàn. Đo thời gian ghi 100 record với và không có `O_SYNC`.

---

## Bài tập 2: Multi-source Poll

Viết chương trình monitor 3 nguồn đồng thời bằng `poll()`:

1. **stdin** (fd=0): đọc command từ người dùng
   - `on` → bật LED
   - `off` → tắt LED
   - `quit` → thoát
2. **timer** (`timerfd_create`): 500ms tick, in trạng thái LED
3. **GPIO53** (`/sys/class/gpio/gpio53/value`): phát hiện edge → in "Button pressed!"

**Yêu cầu**: Một vòng `poll()` xử lý cả 3. Không dùng multi-thread.

**Hint timerfd**:
```c
#include <sys/timerfd.h>
int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
struct itimerspec ts = {
    .it_interval = {0, 500000000},  // 500ms repeat
    .it_value    = {0, 500000000},  // first fire
};
timerfd_settime(tfd, 0, &ts, NULL);
// poll với POLLIN, sau đó read 8 bytes để clear
uint64_t exp; read(tfd, &exp, 8);
```

---

## Bài tập 3: Non-blocking UART Read

Cấu hình `/dev/ttyS1` và thực hiện:
1. Mở với `O_NONBLOCK`
2. Gửi chuỗi test: `write(fd, "PING\r\n", 6)`
3. Dùng `poll()` với timeout 2 giây để chờ echo/response
4. Nếu timeout → in "No response from UART device"
5. Nếu có data → in "Response: <data>"

**Bonus**: Thử đọc không qua poll (trực tiếp `read()`) khi chưa có data với `O_NONBLOCK` — lỗi gì trả về? (`errno = EAGAIN`)
