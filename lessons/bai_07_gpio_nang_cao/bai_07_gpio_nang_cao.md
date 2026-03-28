# Bài 7 - GPIO Nâng Cao: Interrupt và Input Debounce

## 1. Mục tiêu bài học

- Hiểu cơ chế interrupt trên GPIO của AM335x
- Nắm vững các thanh ghi interrupt GPIO: enable, status, detect
- Cấu hình GPIO detect edge (rising/falling/both)
- Sử dụng `poll()` / `epoll()` trong Linux userspace để chờ GPIO interrupt
- Hiểu và áp dụng kỹ thuật debounce phần cứng (debounce register)

---

## 2. Tại sao cần GPIO Interrupt?

Cách thông thường để đọc nút nhấn là **polling** (thăm dò liên tục):

```c
/* Cách polling — BAD: tốn CPU 100% */
while (1) {
    uint32_t val = gpio0[GPIO_DATAIN / 4];
    if (!(val & (1 << 7))) {  /* nút được nhấn */
        do_something();
    }
}
```

**Vấn đề**:
- CPU làm việc 100% chỉ để chờ sự kiện
- Không thể làm việc khác khi chờ

**Giải pháp**: Dùng **interrupt** — phần cứng GPIO tự động thông báo CPU khi có sự kiện, CPU được nghỉ giữa các lần nhấn nút.

---

## 3. Kiến trúc Interrupt GPIO trên AM335x

```
Nút nhấn → GPIO pin → GPIO Controller → INTC (Interrupt Controller) → ARM CPU
                           │
                    GPIO_IRQSTATUS
                    GPIO_IRQENABLE
                    GPIO_RISINGDETECT
                    GPIO_FALLINGDETECT
```

> **Nguồn**: spruh73q.pdf, Section 25.3 (GPIO Interrupt)

### 3.1 Thanh ghi Interrupt GPIO

| Thanh ghi | Offset | Chức năng |
|-----------|--------|-----------|
| `GPIO_IRQSTATUS_0`   | `0x02C` | Trạng thái interrupt bank 0 (bit set = có event) |
| `GPIO_IRQSTATUS_1`   | `0x030` | Trạng thái interrupt bank 1 |
| `GPIO_IRQENABLE_0`   | `0x034` | Bật interrupt cho từng GPIO (bank 0) |
| `GPIO_IRQENABLE_1`   | `0x038` | Bật interrupt cho từng GPIO (bank 1) |
| `GPIO_IRQSTATUS_SET_0`| `0x064`| Set interrupt status (software trigger) |
| `GPIO_IRQSTATUS_CLR_0`| `0x06C`| Clear interrupt enable |
| `GPIO_RISINGDETECT`  | `0x148` | Bật phát hiện cạnh lên (rising edge) |
| `GPIO_FALLINGDETECT` | `0x14C` | Bật phát hiện cạnh xuống (falling edge) |
| `GPIO_LEVELDETECT0`  | `0x140` | Phát hiện mức thấp (level 0) |
| `GPIO_LEVELDETECT1`  | `0x144` | Phát hiện mức cao (level 1) |

> **Nguồn**: spruh73q.pdf, Table 25-5 (GPIO Register Map)

### 3.2 Cách hoạt động của Edge Detection

```
Cạnh lên (Rising Edge):
        ┌─────
────────┘
        ↑ Trigger interrupt nếu RISINGDETECT bit = 1

Cạnh xuống (Falling Edge):
────────┐
        └─────
        ↑ Trigger interrupt nếu FALLINGDETECT bit = 1
```

Để phát hiện cả hai cạnh: set cả `RISINGDETECT` và `FALLINGDETECT` cho bit tương ứng.

---

## 4. Input Debounce

### 4.1 Vấn đề Bounce

Khi nhấn nút cơ học, tín hiệu không chuyển trạng thái sạch — bị **rung (bounce)**:

```
Thực tế nhấn nút:
       ┌──┐ ┌┐┌┐┌────────────
───────┘  └─┘└┘└┘
          ^^^^ bounce (~5-20ms)
```

Nếu không debounce, một lần nhấn có thể tạo ra hàng chục interrupt giả.

### 4.2 Debounce bằng hardware register

AM335x GPIO có **debounce hardware** tích hợp sẵn:

| Thanh ghi | Offset | Chức năng |
|-----------|--------|-----------|
| `GPIO_DEBOUNCENABLE` | `0x150` | Bật debounce cho từng bit GPIO |
| `GPIO_DEBOUNCINGTIME`| `0x154` | Thời gian debounce (đơn vị 31µs/step) |

> **Nguồn**: spruh73q.pdf, Table 25-5, Section 25.3.4

```c
/* Bật debounce cho GPIO0_7 (bit 7) */
gpio0[GPIO_DEBOUNCENABLE / 4] = (1 << 7);

/* Set thời gian debounce = 10 × 31µs ≈ 310µs */
/* Giá trị thực tế: T = (DEBOUNCINGTIME + 1) × 31µs */
gpio0[GPIO_DEBOUNCINGTIME / 4] = 0x09;  /* 10 × 31µs */
```

---

## 5. Sử dụng Interrupt từ Linux Userspace

Từ userspace, không thể viết ISR trực tiếp vào INTC. Thay vào đó, Linux kernel export GPIO interrupt ra file sysfs `/sys/class/gpio/gpioN/value`, và ta dùng `poll()` để chờ:

### 5.1 Thiết lập GPIO interrupt qua sysfs

```bash
# Export GPIO
echo 7 > /sys/class/gpio/export

# Cấu hình direction input
echo in > /sys/class/gpio/gpio7/direction

# Cấu hình edge detect: "rising", "falling", hoặc "both"
echo falling > /sys/class/gpio/gpio7/edge
```

### 5.2 Code C dùng poll() chờ interrupt

```c
/*
 * gpio_interrupt_poll.c
 * Chờ sự kiện GPIO interrupt bằng poll() trong Linux userspace
 *
 * Ví dụ: chờ nút nhấn trên GPIO0_7 (P9.42, Linux GPIO 7)
 * Biên dịch: gcc -o gpio_irq gpio_interrupt_poll.c
 * Chạy: sudo ./gpio_irq
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

#define GPIO_NUM    "7"
#define GPIO_PATH   "/sys/class/gpio/gpio" GPIO_NUM "/value"
#define EXPORT_PATH "/sys/class/gpio/export"
#define EDGE_PATH   "/sys/class/gpio/gpio" GPIO_NUM "/edge"
#define DIR_PATH    "/sys/class/gpio/gpio" GPIO_NUM "/direction"

/* Hàm ghi chuỗi vào file sysfs */
static int sysfs_write(const char *path, const char *val)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) { perror(path); return -1; }
    write(fd, val, strlen(val));
    close(fd);
    return 0;
}

int main(void)
{
    struct pollfd pfd;
    char buf[4];
    int count = 0;

    /* Export GPIO7 */
    sysfs_write(EXPORT_PATH, GPIO_NUM);
    usleep(100000);  /* đợi sysfs tạo thư mục */

    /* Cấu hình: input, falling edge */
    sysfs_write(DIR_PATH, "in");
    sysfs_write(EDGE_PATH, "falling");

    /* Mở file value để poll */
    pfd.fd = open(GPIO_PATH, O_RDONLY);
    if (pfd.fd < 0) { perror("open value"); return 1; }
    pfd.events = POLLPRI | POLLERR;  /* chờ priority read (interrupt event) */

    /* Đọc một lần để clear trạng thái ban đầu */
    read(pfd.fd, buf, sizeof(buf));

    printf("Chờ nút nhấn GPIO%s (falling edge)...\n", GPIO_NUM);
    printf("Nhấn Ctrl+C để thoát\n\n");

    while (1) {
        /* Chờ vô thời hạn (-1) cho đến khi có interrupt */
        int ret = poll(&pfd, 1, -1);
        if (ret < 0) {
            perror("poll");
            break;
        }
        if (pfd.revents & POLLPRI) {
            /* Đọc lại giá trị */
            lseek(pfd.fd, 0, SEEK_SET);
            read(pfd.fd, buf, sizeof(buf));
            buf[1] = '\0';
            count++;
            printf("[Event #%d] GPIO%s = %s\n", count, GPIO_NUM, buf);
        }
    }

    close(pfd.fd);
    return 0;
}
```

---

## 6. Debounce bằng software (khi không dùng hardware)

Khi không dùng GPIO debounce hardware (ví dụ dùng driver khác), có thể debounce bằng software:

```c
/* Trong interrupt handler / poll handler: */
struct timespec last_event = {0, 0};
#define DEBOUNCE_MS 50  /* 50ms debounce window */

void on_gpio_event(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    /* Tính delta time (ms) */
    long delta_ms = (now.tv_sec - last_event.tv_sec) * 1000
                  + (now.tv_nsec - last_event.tv_nsec) / 1000000;

    if (delta_ms < DEBOUNCE_MS) {
        return;  /* Bỏ qua — đây là bounce */
    }

    last_event = now;
    /* Xử lý sự kiện thật sự */
    printf("Nút được nhấn!\n");
}
```

---

## 7. So sánh: Polling vs Interrupt

| Tiêu chí | Polling | Interrupt (poll/epoll) |
|----------|---------|------------------------|
| CPU usage | 100% (busy-wait) | ~0% (blocked) |
| Độ trễ | Thấp (phụ thuộc loop) | Có overhead syscall |
| Độ phức tạp | Đơn giản | Trung bình |
| Cocurrency | Khó | Dễ dùng với epoll |
| Phù hợp | Timing-critical bare-metal | Linux userspace |

---

## 8. Câu hỏi kiểm tra

1. Muốn GPIO phát hiện **cả hai cạnh**, cần set những thanh ghi nào?
2. `GPIO_DEBOUNCINGTIME = 0x1F` tương đương bao nhiêu micro-giây?
3. Khi dùng `poll()`, bit nào trong `revents` báo hiệu có GPIO interrupt?
4. Tại sao phải đọc file `value` một lần trước khi vào vòng `poll()` loop?
5. Giải thích tại sao hardware debounce tốt hơn software debounce trong embedded real-time?

---

## 9. Tài liệu tham khảo

| Nội dung | Tài liệu | Section |
|----------|----------|---------|
| GPIO interrupt registers | spruh73q.pdf | Table 25-5, Section 25.3 |
| GPIO debounce | spruh73q.pdf | Section 25.3.4, Table 25-5 |
| RISINGDETECT / FALLINGDETECT | spruh73q.pdf | Section 25.4.1 (offset 0x148, 0x14C) |
| Linux GPIO sysfs interface | linux kernel docs | Documentation/gpio/sysfs.txt |
