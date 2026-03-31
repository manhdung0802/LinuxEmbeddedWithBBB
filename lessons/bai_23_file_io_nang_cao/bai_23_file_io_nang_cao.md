# Bài 23 - File I/O Nâng Cao: open, ioctl, poll, select

## 1. Mục tiêu bài học

- Nắm vững `open()`, `read()`, `write()` với flags & modes nâng cao
- Dùng `ioctl()` để giao tiếp với driver (như bài 18)
- Dùng `poll()` và `select()` để monitor nhiều fd cùng lúc
- Đọc device file `/dev/` theo kiểu non-blocking
- Ứng dụng: monitor GPIO interrupt + UART data cùng lúc

---

## 2. open() — Flags Quan Trọng

```c
#include <fcntl.h>
#include <sys/stat.h>

int fd = open(path, flags, mode);
```

| Flag | Ý nghĩa |
|------|---------|
| `O_RDONLY` | Chỉ đọc |
| `O_WRONLY` | Chỉ ghi |
| `O_RDWR` | Đọc và ghi |
| `O_CREAT` | Tạo file nếu chưa có (cần `mode`) |
| `O_TRUNC` | Xóa nội dung cũ khi mở |
| `O_APPEND` | Ghi nối vào cuối file |
| `O_NONBLOCK` | Non-blocking I/O (không block nếu chưa có data) |
| `O_SYNC` | Đảm bảo data ghi xuống hardware trước khi return |
| `O_CLOEXEC` | Đóng fd tự động khi exec() |

```c
/* Mở device file, non-blocking */
int gpio_fd = open("/sys/class/gpio/gpio53/value", O_RDONLY | O_NONBLOCK);

/* Tạo log file, append mode */
int log_fd = open("/var/log/sensor.log",
                  O_WRONLY | O_CREAT | O_APPEND,
                  0644);    /* mode: rw-r--r-- */

/* Luôn đặt O_CLOEXEC cho fd không nên được kế thừa */
int fd = open("/dev/i2c-1", O_RDWR | O_CLOEXEC);
```

---

## 3. Đọc/Ghi Binary Data

```c
/*
 * binary_io.c — Ghi và đọc struct binary vào file
 * Dùng cho data logging trên BBB
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    uint32_t timestamp;    /* unix time */
    uint16_t adc_value;    /* 0-4095 (12-bit ADC) */
    int16_t  temperature;  /* milli-degrees Celsius */
    uint8_t  gpio_state;   /* bitmask */
    uint8_t  padding;      /* align to 4 bytes */
} __attribute__((packed)) SensorRecord;

/* Ghi record vào file */
int log_record(int fd, const SensorRecord *rec)
{
    ssize_t written = write(fd, rec, sizeof(*rec));
    if (written != sizeof(*rec)) {
        perror("write");
        return -1;
    }
    return 0;
}

/* Đọc tất cả records từ file */
int read_log(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open"); return -1; }

    SensorRecord rec;
    int count = 0;
    ssize_t n;

    while ((n = read(fd, &rec, sizeof(rec))) == sizeof(rec)) {
        printf("[%u] ADC=%4u Temp=%5.1f°C GPIO=0x%02x\n",
               rec.timestamp,
               rec.adc_value,
               rec.temperature / 10.0,
               rec.gpio_state);
        count++;
    }

    close(fd);
    return count;
}
```

---

## 4. ioctl() — Device Control

```c
#include <sys/ioctl.h>

int ret = ioctl(fd, request, arg);
/*
 * fd      : file descriptor của device
 * request : command code (thường định nghĩa trong header của driver)
 * arg     : pointer tới data (hoặc value nhỏ cast thành unsigned long)
 */
```

### Ví dụ: ioctl với UART (termios thay thế thường dùng, nhưng có thể dùng ioctl trực tiếp)

```c
#include <termios.h>
#include <sys/ioctl.h>

int uart_fd = open("/dev/ttyS1", O_RDWR | O_NOCTTY);

/* Dùng ioctl TIOCMGET để đọc modem status (CTS, RTS, DSR...) */
int modem_bits;
ioctl(uart_fd, TIOCMGET, &modem_bits);
printf("CTS: %s\n", (modem_bits & TIOCM_CTS) ? "SET" : "CLEAR");

/* Dùng ioctl FIONREAD: số byte có trong receive buffer */
int bytes_available;
ioctl(uart_fd, FIONREAD, &bytes_available);
printf("Bytes waiting: %d\n", bytes_available);
```

---

## 5. poll() — Monitor Nhiều fd

`poll()` block cho đến khi **ít nhất một** fd có event, hoặc timeout.

```c
#include <poll.h>

struct pollfd fds[N];
fds[i].fd      = file_descriptor;
fds[i].events  = POLLIN | POLLPRI;  /* sự kiện muốn theo dõi */
fds[i].revents = 0;                 /* kernel điền vào khi có event */

int ret = poll(fds, N, timeout_ms);
/* ret > 0: số fd có event
 * ret = 0: timeout
 * ret < 0: error */
```

### Ví dụ: Monitor GPIO interrupt + UART cùng lúc

```c
/*
 * poll_demo.c — Đọc GPIO edge + UART data không dùng multi-thread
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>

int main(void)
{
    /* Export GPIO53 và set edge detection */
    system("echo 53 > /sys/class/gpio/export");
    system("echo in > /sys/class/gpio/gpio53/direction");
    system("echo both > /sys/class/gpio/gpio53/edge");  /* rising+falling */

    int gpio_fd = open("/sys/class/gpio/gpio53/value", O_RDONLY | O_NONBLOCK);
    int uart_fd = open("/dev/ttyS1", O_RDWR | O_NONBLOCK | O_NOCTTY);

    /* Đọc lần đầu để clear initial state GPIO */
    char dummy[4];
    read(gpio_fd, dummy, sizeof(dummy));
    lseek(gpio_fd, 0, SEEK_SET);

    struct pollfd fds[2];
    fds[0].fd     = gpio_fd;
    fds[0].events = POLLPRI | POLLERR;  /* GPIO sysfs dùng POLLPRI cho edge */

    fds[1].fd     = uart_fd;
    fds[1].events = POLLIN;              /* có dữ liệu đến */

    printf("Monitoring GPIO53 and UART1... (Ctrl+C to stop)\n");

    while (1) {
        int ret = poll(fds, 2, 5000);   /* timeout 5 giây */

        if (ret < 0) { perror("poll"); break; }
        if (ret == 0) { printf("Timeout - no events\n"); continue; }

        /* Kiểm tra GPIO event */
        if (fds[0].revents & POLLPRI) {
            char val[4] = {0};
            lseek(gpio_fd, 0, SEEK_SET);
            read(gpio_fd, val, sizeof(val)-1);
            printf("[GPIO53] Edge detected! value=%c\n", val[0]);
        }

        /* Kiểm tra UART data */
        if (fds[1].revents & POLLIN) {
            char buf[64];
            ssize_t n = read(uart_fd, buf, sizeof(buf)-1);
            if (n > 0) {
                buf[n] = '\0';
                printf("[UART1] Received %zd bytes: %s\n", n, buf);
            }
        }
    }

    close(gpio_fd);
    close(uart_fd);
    return 0;
}
```

### Giải thích chi tiết từng dòng code (poll_demo.c)

a) **`system("echo 53 > /sys/class/gpio/export")`**:
- Export GPIO53 (= GPIO1_21: bank 1 × 32 + 21 = 53) qua sysfs interface.
- `echo both > .../edge` — bật edge detection cả rising và falling, cần thiết để `poll()` nhận event `POLLPRI`.

b) **`open("/sys/.../value", O_RDONLY | O_NONBLOCK)`**:
- `O_NONBLOCK` — `read()` sẽ trả về ngay với `-1` + `EAGAIN` nếu chưa có data, thay vì block.
- GPIO sysfs value file chứa "0" hoặc "1".

c) **Đọc lần đầu để clear initial state**:
```c
read(gpio_fd, dummy, sizeof(dummy));
lseek(gpio_fd, 0, SEEK_SET);
```
- GPIO sysfs luôn có data sẵn (giá trị hiện tại). Phải đọc một lần để clear, nếu không `poll()` sẽ trả về ngay event giả.
- `lseek(gpio_fd, 0, SEEK_SET)` — reset position về đầu file.

d) **`fds[0].events = POLLPRI | POLLERR`**:
- GPIO sysfs dùng `POLLPRI` (**không phải** `POLLIN`) cho edge interrupt. Đây là convention đặc biệt của kernel GPIO sysfs driver.
- `POLLERR` — bắt error trên fd.

e) **`poll(fds, 2, 5000)`**:
- Tham số: mảng fd, số fd, timeout 5000ms.
- Trả về: `> 0` = số fd có event, `0` = timeout, `< 0` = error.
- Block cho đến khi có event hoặc hết timeout → **không tốn CPU** (khác với busy loop).

f) **Xử lý GPIO event**:
```c
lseek(gpio_fd, 0, SEEK_SET);
read(gpio_fd, val, sizeof(val)-1);
```
- **Phải `lseek` trước `read`** với GPIO sysfs — đây là quirk của kernel sysfs, không giống file thông thường.

> **Bài học**: `poll()` và `select()` cho phép monitor nhiều fd **không cần multi-thread**. Đây là pattern event-driven I/O cơ bản, sẽ được nâng cấp thành `epoll` ở bài 24.

---

## 6. select() — Classic Multiplexing

`select()` cũ hơn `poll()` nhưng vẫn phổ biến. Giới hạn FD_SETSIZE (thường 1024 fd).

```c
#include <sys/select.h>

fd_set readfds;
FD_ZERO(&readfds);
FD_SET(gpio_fd, &readfds);
FD_SET(uart_fd, &readfds);
int maxfd = (gpio_fd > uart_fd ? gpio_fd : uart_fd) + 1;

struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };  /* 5s timeout */

int ret = select(maxfd, &readfds, NULL, NULL, &tv);
/*                       ↑read   ↑write ↑except  */

if (FD_ISSET(gpio_fd, &readfds))
    /* gpio_fd có data */

if (FD_ISSET(uart_fd, &readfds))
    /* uart_fd có data */
```

### So sánh poll vs select

| | select | poll |
|---|---|---|
| Max fd | FD_SETSIZE (1024) | Không giới hạn |
| Cú pháp | Phức tạp hơn | Đơn giản hơn |
| Portability | POSIX, rất cũ | POSIX |
| Hiệu năng nhiều fd | Kém (scan toàn bộ set) | Tốt hơn (scan array) |
| Khuyến nghị | Legacy code | Code mới |

> **epoll** (bài 24) tốt hơn cả hai cho số lượng fd lớn.

---

## 7. fcntl() — File Descriptor Control

```c
#include <fcntl.h>

/* Đặt non-blocking sau khi open */
int flags = fcntl(fd, F_GETFL);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);

/* Lấy/set close-on-exec flag */
fcntl(fd, F_SETFD, FD_CLOEXEC);

/* Duplicate fd */
int fd2 = fcntl(fd, F_DUPFD, 0);  /* mfd nhỏ nhất ≥ 0 */
```

---

## 8. Câu hỏi kiểm tra

1. `O_NONBLOCK` ảnh hưởng đến `read()` như thế nào khi chưa có data?
2. Tại sao GPIO sysfs interrupt dùng `POLLPRI` thay vì `POLLIN`?
3. `select()` yêu cầu tính `maxfd + 1` — `maxfd` là gì?
4. `ioctl(fd, FIONREAD, &n)` trả về thông tin gì?
5. Sự khác nhau giữa `close(fd)` và `shutdown(fd, SHUT_RDWR)`?

---

## 9. Tài liệu tham khảo

| API | Man page | Ghi chú |
|-----|----------|---------|
| open | man 2 open | flags, modes |
| poll | man 2 poll | POLLIN, POLLPRI, POLLERR |
| select | man 2 select | FD_SET, FD_ISSET, tv |
| ioctl | man 2 ioctl | device-specific |
| fcntl | man 2 fcntl | F_GETFL, F_SETFL, F_SETFD |
