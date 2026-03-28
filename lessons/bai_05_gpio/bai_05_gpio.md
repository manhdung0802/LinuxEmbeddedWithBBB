# Bài 5 - GPIO: Control Module, Pad Config và Toggle LED

## 1. Mục tiêu bài học

- Hiểu kiến trúc GPIO trên AM335x: có bao nhiêu module, bao nhiêu chân
- Nắm vững vai trò của **Control Module** trong việc cấu hình pad (pinmux)
- Hiểu từng thanh ghi GPIO quan trọng và bit-field của chúng
- Viết code C blink LED USR0 trên BBB bằng mmap (không dùng thư viện)
- So sánh cách dùng sysfs và mmap để điều khiển GPIO

---

## 2. Kiến trúc GPIO trên AM335x

AM335x có **4 module GPIO**, mỗi module quản lý 32 chân:

| Module | Base Address | Số GPIO Linux |
|--------|-------------|---------------|
| GPIO0  | `0x44E07000` | GPIO 0..31   |
| GPIO1  | `0x4804C000` | GPIO 32..63  |
| GPIO2  | `0x481AC000` | GPIO 64..95  |
| GPIO3  | `0x481AE000` | GPIO 96..127 |

> **Nguồn**: spruh73q.pdf, Table 2-3 (L4_WKUP Peripheral Map) và Table 2-4 (L4_PER Peripheral Map)

Tổng cộng: **128 GPIO vật lý** — nhưng không phải tất cả đều được đưa ra header P8/P9.

---

## 3. Control Module và Pad Configuration

### 3.1 Control Module là gì?

**Control Module** (base `0x44E10000`) là một khối register đặc biệt trên AM335x:
- Quản lý **pinmux**: quyết định chức năng của mỗi chân vật lý
- Cấu hình **pull-up / pull-down resistor**
- Cấu hình **drive strength** (độ mạnh tín hiệu)
- Cấu hình **input enable**

> **Nguồn**: spruh73q.pdf, Section 9.3 (Control Module Registers)

### 3.2 Cấu trúc một Pad Control Register

Mỗi chân vật lý có một **conf_xxx register** (32-bit), nhưng chỉ 7 bit thấp có ý nghĩa:

```
Bit 31           7   6   5   4   3   2   1   0
     ├─────────────┬───┬───┬───┬───┴───┴───┴───┤
     │  Reserved   │ SL│ RX│ PU│     MMODE     │
     └─────────────┴───┴───┴───┴───────────────┘

Bit 2..0 (MMODE): Chọn chức năng (mode 0..7)
Bit 3    (PUTYPESEL): 0 = pull-down, 1 = pull-up
Bit 4    (PUDEN): 0 = enable pull, 1 = disable pull
Bit 5    (RXACTIVE): 0 = output only, 1 = enable input buffer
Bit 6    (SLEWCTRL): 0 = fast slew, 1 = slow slew
```

> **Nguồn**: spruh73q.pdf, Section 9.3.1 (conf_xxx register description)

### 3.3 Ví dụ: LED USR0 trên BBB

LED USR0 được nối với **GPIO1_21** (Linux GPIO = 1×32 + 21 = **53**).

Chân tương ứng trên AM335x: `gpmc_a5` → offset `0x854` trong Control Module.

Để cấu hình làm GPIO output:
```
conf_gpmc_a5 = 0x44E10000 + 0x854
Giá trị cần set: 0x07
  - MMODE = 7 (GPIO mode)
  - PUTYPESEL = 0 (pull-down)
  - PUDEN = 0 (enable pull)
  - RXACTIVE = 0 (output only)
```

> **Nguồn**: spruh73q.pdf, Table 9-7 (conf_gpmc_a5 register), spruh73q.pdf Section 9.3

---

## 4. Thanh ghi GPIO quan trọng

Mỗi GPIO module có layout thanh ghi giống nhau. Offset tính từ base address:

| Thanh ghi         | Offset    | Chức năng |
|-------------------|-----------|-----------|
| `GPIO_REVISION`   | `0x000`   | Version của module |
| `GPIO_SYSCONFIG`  | `0x010`   | Cấu hình module (idle mode, wakeup) |
| `GPIO_IRQSTATUS_0`| `0x02C`   | Trạng thái interrupt (xóa bằng cách ghi 1) |
| `GPIO_IRQENABLE_0`| `0x034`   | Bật/tắt interrupt cho từng bit |
| `GPIO_OE`         | `0x134`   | Output Enable: 0 = output, 1 = input |
| `GPIO_DATAIN`     | `0x138`   | Đọc trạng thái chân vật lý (kể cả output) |
| `GPIO_DATAOUT`    | `0x13C`   | Đọc/ghi giá trị output register |
| `GPIO_CLEARDATAOUT`| `0x190`  | Ghi 1 để xóa bit tương ứng trong DATAOUT |
| `GPIO_SETDATAOUT` | `0x194`   | Ghi 1 để set bit tương ứng trong DATAOUT |

> **Nguồn**: spruh73q.pdf, Table 25-5 (GPIO Register Map)

### 4.1 Tại sao có cả SETDATAOUT và CLEARDATAOUT?

Nếu chỉ có `GPIO_DATAOUT`, để toggle bit 21 bạn phải:
```c
uint32_t val = *(gpio1 + GPIO_DATAOUT/4);
val |= (1 << 21);       // phải đọc trước
*(gpio1 + GPIO_DATAOUT/4) = val;  // rồi mới ghi
```
Thao tác Read-Modify-Write này có thể có **race condition** trong môi trường đa luồng hoặc interrupt.

Với `GPIO_SETDATAOUT`/`GPIO_CLEARDATAOUT`:
```c
*(gpio1 + GPIO_SETDATAOUT/4) = (1 << 21);  // atomic, không cần read trước
```
Đây là **atomic bit operation** — an toàn hơn.

---

## 5. Trình tự bật GPIO

Trước khi dùng GPIO, phải thực hiện **đúng thứ tự** sau:

```
1. Bật clock cho GPIO module (CM_PER / CM_WKUP)
           ↓
2. Cấu hình pinmux trong Control Module (conf_xxx register)
           ↓
3. Cấu hình direction (GPIO_OE): output hay input
           ↓
4. Ghi/đọc dữ liệu (GPIO_SETDATAOUT / GPIO_DATAIN)
```

Nếu bỏ qua bước 1 hoặc 2, GPIO sẽ không hoạt động dù set đúng thanh ghi data.

---

## 6. Code C - Blink LED USR0 (GPIO1_21)

```c
/*
 * blink_led_usr0.c
 * Blink LED USR0 trên BeagleBone Black bằng mmap /dev/mem
 *
 * LED USR0 = GPIO1_21 (Linux GPIO 53)
 * Biên dịch: gcc -o blink blink_led_usr0.c
 * Chạy: sudo ./blink
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

/* === Địa chỉ base (spruh73q.pdf, Table 2-3, 2-4) === */
#define GPIO1_BASE          0x4804C000UL
#define CTRL_MODULE_BASE    0x44E10000UL
#define CM_PER_BASE         0x44E00000UL

/* === Offset thanh ghi GPIO (spruh73q.pdf, Table 25-5) === */
#define GPIO_OE             0x134
#define GPIO_SETDATAOUT     0x194
#define GPIO_CLEARDATAOUT   0x190

/* === Offset thanh ghi Clock (spruh73q.pdf, Table 8-29) === */
#define CM_PER_GPIO1_CLKCTRL  0xAC

/* === Pad config offset (spruh73q.pdf, Table 9-7) === */
#define CONF_GPMC_A5        0x854   /* GPIO1_21 */

/* Kích thước page và mask */
#define PAGE_SIZE           4096
#define PAGE_MASK           (~(PAGE_SIZE - 1))

/* Bit GPIO1_21 */
#define USR0_BIT            (1U << 21)

int main(void)
{
    int fd;
    volatile uint32_t *gpio1;
    volatile uint32_t *ctrl;
    volatile uint32_t *cm_per;

    /* Mở /dev/mem để ánh xạ bộ nhớ vật lý */
    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open /dev/mem");
        return 1;
    }

    /* Ánh xạ GPIO1 */
    gpio1 = (volatile uint32_t *)mmap(NULL, PAGE_SIZE,
                PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                GPIO1_BASE & PAGE_MASK);
    if (gpio1 == MAP_FAILED) {
        perror("mmap GPIO1");
        close(fd);
        return 1;
    }

    /* Ánh xạ Control Module */
    ctrl = (volatile uint32_t *)mmap(NULL, PAGE_SIZE * 16,
                PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                CTRL_MODULE_BASE & PAGE_MASK);
    if (ctrl == MAP_FAILED) {
        perror("mmap CTRL");
        munmap((void *)gpio1, PAGE_SIZE);
        close(fd);
        return 1;
    }

    /* Ánh xạ CM_PER để bật clock */
    cm_per = (volatile uint32_t *)mmap(NULL, PAGE_SIZE,
                PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                CM_PER_BASE & PAGE_MASK);
    if (cm_per == MAP_FAILED) {
        perror("mmap CM_PER");
        munmap((void *)gpio1, PAGE_SIZE);
        munmap((void *)ctrl, PAGE_SIZE * 16);
        close(fd);
        return 1;
    }

    /* === Bước 1: Bật clock GPIO1 (spruh73q.pdf, Section 8.1.12.1.28) === */
    /* CM_PER_GPIO1_CLKCTRL[1:0] = 0x2 (MODULEMODE = ENABLE) */
    cm_per[CM_PER_GPIO1_CLKCTRL / 4] = 0x40002;
    /* Bit 18 (OPTFCLKEN_GPIO_1_GDBCLK) = 1: bật functional clock */

    /* === Bước 2: Cấu hình pinmux GPIO1_21 (spruh73q.pdf, Table 9-7) === */
    /* conf_gpmc_a5: Mode 7 = GPIO, output, no pull */
    ctrl[CONF_GPMC_A5 / 4] = 0x07;

    /* === Bước 3: Set GPIO1_21 là OUTPUT (GPIO_OE bit 21 = 0) === */
    gpio1[GPIO_OE / 4] &= ~USR0_BIT;

    printf("Bắt đầu blink LED USR0. Nhấn Ctrl+C để dừng.\n");

    /* === Bước 4: Toggle LED mỗi 500ms === */
    while (1) {
        /* Bật LED: set bit 21 trong SETDATAOUT */
        gpio1[GPIO_SETDATAOUT / 4] = USR0_BIT;
        usleep(500000);  /* 500ms */

        /* Tắt LED: set bit 21 trong CLEARDATAOUT */
        gpio1[GPIO_CLEARDATAOUT / 4] = USR0_BIT;
        usleep(500000);
    }

    /* Dọn dẹp (không bao giờ tới đây trong ví dụ này) */
    munmap((void *)gpio1, PAGE_SIZE);
    munmap((void *)ctrl, PAGE_SIZE * 16);
    munmap((void *)cm_per, PAGE_SIZE);
    close(fd);
    return 0;
}
```

### Cách biên dịch và chạy trên BBB:
```bash
gcc -o blink blink_led_usr0.c
sudo ./blink
```

---

## 7. So sánh: sysfs vs mmap

| Tiêu chí | sysfs (`/sys/class/gpio`) | mmap (`/dev/mem`) |
|----------|--------------------------|-------------------|
| Cách truy cập | Đọc/ghi file text | Con trỏ C trực tiếp |
| Tốc độ | Chậm (syscall overhead) | Nhanh (~µs) |
| Độ phức tạp code | Đơn giản | Trung bình |
| Cần quyền root | Không (sau export) | Có |
| Phù hợp cho | Debug, prototype nhanh | Production, timing critical |
| Nguy hiểm | Ít | Cao nếu viết sai địa chỉ |

---

## 8. Câu hỏi kiểm tra

1. GPIO1_17 ứng với Linux GPIO number bao nhiêu?
2. Muốn đặt chân GPIO là **input**, phải set bit tương ứng trong `GPIO_OE` là 0 hay 1?
3. Tại sao nên dùng `GPIO_SETDATAOUT`/`GPIO_CLEARDATAOUT` thay vì đọc-sửa-ghi `GPIO_DATAOUT`?
4. Nếu quên bật clock module GPIO1, điều gì xảy ra khi ghi vào thanh ghi GPIO_OE?
5. Trong `conf_gpmc_a5 = 0x07`, bit nào quyết định đây là GPIO mode?

---

## 9. Tài liệu tham khảo

| Nội dung | Tài liệu | Trang/Section |
|----------|----------|---------------|
| GPIO module base address | spruh73q.pdf | Table 2-3, 2-4 |
| GPIO Register Map | spruh73q.pdf | Table 25-5 (Section 25.4.1) |
| conf_gpmc_a5 pad register | spruh73q.pdf | Table 9-7, Section 9.3 |
| Pad control bit-field | spruh73q.pdf | Section 9.3.1 |
| CM_PER_GPIO1_CLKCTRL | spruh73q.pdf | Section 8.1.12.1.28 |
| LED USR0 schematic | BBB_SRM_C.pdf | Section 3.4.3 |
