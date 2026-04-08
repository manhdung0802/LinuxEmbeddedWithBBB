# Bài 6 - Clock Module (CM): PRCM, Clock Gating và Enabling Modules

## 1. Mục tiêu bài học

- Hiểu hệ thống clock và power trên AM335x (PRCM)
- Biết clock domain là gì và tại sao phải bật clock trước khi dùng module
- Nắm vững thanh ghi `CM_PER_xxx_CLKCTRL` và `CM_WKUP_xxx_CLKCTRL`
- Biết cách bật/tắt module theo lập trình register-level
- Giải thích nguyên lý clock gating để tiết kiệm điện
- Bật clock cần có thời gian chờ để clock chuyển qua status Func

---

## 2. PRCM là gì?

**PRCM** (Power, Reset, Clock Management) là hệ thống quản lý nguồn, reset và clock trên AM335x.

```
┌──────────────────────────────────────┐
│              PRCM                    │
│  ┌───────────┐    ┌───────────────┐  │
│  │  CM       │    │    PRM        │  │
│  │ (Clock    │    │  (Power &     │  │
│  │  Mgr)     │    │   Reset Mgr)  │  │
│  └───────────┘    └───────────────┘  │
└──────────────────────────────────────┘
```

- **CM** (Clock Manager): bật/tắt clock đến từng module — đây là phần ta tập trung
- **PRM** (Power & Reset Manager): quản lý power domain và reset

> **Nguồn**: spruh73q.pdf, Chapter 8 (Power, Reset, and Clock Management)

---

## 3. Clock Domain trên AM335x

Các module được nhóm vào **clock domain** (nhóm module dùng chung clock):

| Clock Domain | Base Address | Chứa các module |
|-------------|-------------|-----------------|
| `CM_PER`    | `0x44E00000` | GPIO1..3, UART1..5, SPI0..1, I2C1..2, Timer2..7, EMMC, ... |
| `CM_WKUP`   | `0x44E00400` | GPIO0, UART0, I2C0, ADC, Timer0..1, Watchdog |
| `CM_DPLL`   | `0x44E00500` | PLL configuration |
| `CM_MPU`    | `0x44E00600` | Cortex-A8 clock |
| `CM_DEVICE` | `0x44E00700` | Clock output |
| `CM_RTC`    | `0x44E00800` | RTC module |

> **Nguồn**: spruh73q.pdf, Section 8.1.12 (Clock Management Registers)

---

## 4. Cấu trúc thanh ghi CLKCTRL

Mỗi module có một thanh ghi `CM_xxx_YYY_CLKCTRL` dạng:

```
Bit 31        18  17  16  15           2   1   0
    ├──────────┬───┴───┬───────────────┬───┴───┤
    │ Reserved │IDLEST │   Reserved    │MODULEMODE│
    └──────────┴───────┴───────────────┴───────┘

Bits 1..0 (MODULEMODE):
  0x0 = DISABLED  — module bị tắt hoàn toàn
  0x1 = n/a
  0x2 = ENABLE    — module được bật
  0x3 = n/a

Bits 17..16 (IDLEST) — chỉ đọc:
  0x0 = Functional  — module đang hoạt động bình thường
  0x1 = Transition  — đang chuyển trạng thái
  0x2 = Idle        — module đang ở trạng thái idle
  0x3 = Disabled    — module bị tắt
```

> **Nguồn**: spruh73q.pdf, Section 8.1.12.x (từng CLKCTRL register)

---

## 5. Bảng thanh ghi CLKCTRL thường dùng

| Module | Thanh ghi | Địa chỉ tuyệt đối |
|--------|-----------|-------------------|
| GPIO0  | `CM_WKUP_GPIO0_CLKCTRL`  | `0x44E00408` |
| GPIO1  | `CM_PER_GPIO1_CLKCTRL`   | `0x44E000AC` |
| GPIO2  | `CM_PER_GPIO2_CLKCTRL`   | `0x44E000B0` |
| GPIO3  | `CM_PER_GPIO3_CLKCTRL`   | `0x44E000B4` |
| UART0  | `CM_WKUP_UART0_CLKCTRL`  | `0x44E004B4` |
| UART1  | `CM_PER_UART1_CLKCTRL`   | `0x44E0006C` |
| I2C0   | `CM_WKUP_I2C0_CLKCTRL`   | `0x44E004B8` |
| I2C1   | `CM_PER_I2C1_CLKCTRL`    | `0x44E004A0` |
| SPI0   | `CM_PER_SPI0_CLKCTRL`    | `0x44E0004C` |
| Timer2 | `CM_PER_TIMER2_CLKCTRL`  | `0x44E00080` |
| ADC    | `CM_WKUP_ADC_TSC_CLKCTRL`| `0x44E004BC` |

> **Nguồn**: spruh73q.pdf, Tables 8-56..8-60 (Clock Domain Registers)

---

## 6. Clock Gating là gì và tại sao cần?

**Clock Gating** là kỹ thuật tắt clock đến module không dùng để tiết kiệm điện:

```
Clock nguồn ──► [GATE] ──► Module
                  ↑
           MODULEMODE[1:0]
           
- MODULEMODE = 2 → Gate mở → clock chạy
- MODULEMODE = 0 → Gate đóng → module không nhận clock → tiết kiệm điện
```

**Lợi ích**:
- Tiết kiệm điện năng đáng kể trong hệ thống nhúng chạy pin
- Giảm nhiễu điện (EMI) từ các module không dùng
- Tránh xung đột bus khi module chưa được khởi tạo

---

## 7. GPIO1 có clock đặc biệt

GPIO1 cần **2 clock** để hoạt động đầy đủ:

```c
/* spruh73q.pdf, Section 8.1.12.1.28 */
/* Bit 17..16: IDLEST (read-only)
   Bit 18: OPTFCLKEN_GPIO_1_GDBCLK (optional functional clock) */

CM_PER_GPIO1_CLKCTRL = 0x40002;
/*
  Bit 1..0 = 0x2 (MODULEMODE = ENABLE)
  Bit 18   = 0x1 (OPTFCLKEN_GPIO_1_GDBCLK = enable)
*/
```

Nếu không bật bit 18, GPIO1 vẫn nhận interface clock nhưng **debounce và functional clock sẽ thiếu**.

---

## 8. Code C - Hàm enable module

```c
/*
 * enable_module.c
 * Hàm tiện ích bật clock cho một module bất kỳ
 * Nguồn thanh ghi: spruh73q.pdf, Section 8.1.12
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>

/* Base address vùng PRCM (CM_PER + CM_WKUP nằm trong vùng này) */
#define PRCM_BASE       0x44E00000UL
#define PRCM_SIZE       0x2000      /* 8KB mapping để bao quát cả CM_PER và CM_WKUP */

/* Offset của từng CLKCTRL so với PRCM_BASE (spruh73q.pdf, Table 8-56..8-60) */
#define CM_PER_GPIO1_CLKCTRL_OFF    0x0AC
#define CM_PER_GPIO2_CLKCTRL_OFF    0x0B0
#define CM_PER_UART1_CLKCTRL_OFF    0x06C
#define CM_WKUP_GPIO0_CLKCTRL_OFF   0x408
#define CM_WKUP_UART0_CLKCTRL_OFF   0x4B4

#define MODULEMODE_ENABLE   0x2
#define IDLEST_FUNCTIONAL   0x0
#define IDLEST_SHIFT        16
#define IDLEST_MASK         (0x3 << IDLEST_SHIFT)

static volatile uint32_t *prcm_base = NULL;
static int mem_fd = -1;

int prcm_init(void)
{
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("open /dev/mem");
        return -1;
    }
    prcm_base = mmap(NULL, PRCM_SIZE, PROT_READ | PROT_WRITE,
                     MAP_SHARED, mem_fd, PRCM_BASE);
    if (prcm_base == MAP_FAILED) {
        perror("mmap PRCM");
        close(mem_fd);
        return -1;
    }
    return 0;
}

void prcm_cleanup(void)
{
    if (prcm_base) munmap((void*)prcm_base, PRCM_SIZE);
    if (mem_fd >= 0) close(mem_fd);
}

/*
 * Bật module và đợi IDLEST = Functional
 * offset: offset của CLKCTRL register so với PRCM_BASE
 * extra_bits: các bit thêm cần set (ví dụ bit 18 cho GPIO1)
 */
int enable_module(uint32_t offset, uint32_t extra_bits)
{
    volatile uint32_t *reg = &prcm_base[offset / 4];
    int timeout = 1000;

    /* Set MODULEMODE = ENABLE */
    *reg = MODULEMODE_ENABLE | extra_bits;

    /* Đợi IDLEST = Functional (0x0) */
    while (((*reg & IDLEST_MASK) >> IDLEST_SHIFT) != IDLEST_FUNCTIONAL) {
        if (--timeout == 0) {
            printf("Timeout: module tại offset 0x%X không trở về Functional!\n", offset);
            return -1;
        }
        usleep(1000);
    }
    printf("Module 0x%X đã bật, IDLEST = Functional\n", offset);
    return 0;
}

int main(void)
{
    if (prcm_init() != 0) return 1;

    /* Bật GPIO1 (cần thêm bit 18 cho optional functional clock) */
    enable_module(CM_PER_GPIO1_CLKCTRL_OFF, (1 << 18));

    /* Bật UART1 */
    enable_module(CM_PER_UART1_CLKCTRL_OFF, 0);

    prcm_cleanup();
    return 0;
}
```

### Giải thích chi tiết từng dòng code

#### a) Các hằng số địa chỉ

```c
#define PRCM_BASE  0x44E00000UL  // Base address vùng PRCM (Power, Reset, Clock Management)
                                 // Bao gồm cả CM_PER (0x44E00000) và CM_WKUP (0x44E00400)
#define PRCM_SIZE  0x2000        // 8KB — đủ bao quát cả CM_PER và CM_WKUP
                                 // CM_WKUP bắt đầu từ offset 0x400 trong vùng này
```

```c
#define CM_PER_GPIO1_CLKCTRL_OFF  0x0AC  // Offset thanh ghi điều khiển clock GPIO1
#define CM_WKUP_GPIO0_CLKCTRL_OFF 0x408  // GPIO0 thuộc CM_WKUP (wakeup domain), không phải CM_PER
```

```c
#define MODULEMODE_ENABLE  0x2          // Giá trị ghi vào bit [1:0] để bật module
                                        // 0x0 = Disabled, 0x2 = Enabled
#define IDLEST_FUNCTIONAL  0x0          // Trạng thái bit [17:16] khi module hoạt động bình thường
                                        // 0x0 = Functional, 0x1 = Transition, 0x2 = Idle, 0x3 = Disabled
#define IDLEST_SHIFT       16           // IDLEST nằm ở bit 16-17
#define IDLEST_MASK (0x3 << IDLEST_SHIFT)  // = 0x00030000 — mặt nạ để trích xuất 2 bit IDLEST
```

#### b) Hàm `prcm_init()` — ánh xạ vùng PRCM

```c
prcm_base = mmap(NULL, PRCM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, mem_fd, PRCM_BASE);
// Ánh xạ 8KB từ physical address 0x44E00000
// Sau lệnh này, prcm_base[0] = thanh ghi tại 0x44E00000
//                prcm_base[0x0AC/4] = CM_PER_GPIO1_CLKCTRL
//                prcm_base[0x408/4] = CM_WKUP_GPIO0_CLKCTRL
```

#### c) Hàm `enable_module()` — bật clock và chờ IDLEST

```c
volatile uint32_t *reg = &prcm_base[offset / 4];
// Tính virtual address của thanh ghi CLKCTRL cần bật
// Ví dụ: offset = 0x0AC → &prcm_base[43] → trỏ tới CM_PER_GPIO1_CLKCTRL
```

```c
*reg = MODULEMODE_ENABLE | extra_bits;
// Ví dụ GPIO1: MODULEMODE_ENABLE | (1 << 18) = 0x2 | 0x40000 = 0x40002
//   Bit [1:0] = 0b10 = ENABLE — bật interface clock cho module
//   Bit 18 = 1 = OPTFCLKEN — bật optional functional clock (debounce clock cho GPIO)
// Ví dụ UART1: MODULEMODE_ENABLE | 0 = 0x2 — chỉ cần bật MODULEMODE
```

```c
while (((*reg & IDLEST_MASK) >> IDLEST_SHIFT) != IDLEST_FUNCTIONAL) {
```

Phân tích phép toán từng bước (ví dụ `*reg = 0x00060002`):
```
*reg             = 0x00060002
IDLEST_MASK      = 0x00030000
*reg & MASK      = 0x00020000   ← trích bit 16-17
>> IDLEST_SHIFT  = 0x00000002   ← dịch phải 16 bit → giá trị IDLEST = 2 (Idle)
So với FUNCTIONAL = 0           → chưa bằng → tiếp tục chờ
```

Vòng lặp kết thúc khi IDLEST = 0 (Functional), nghĩa là module đã bật xong và sẵn sàng sử dụng.

```c
if (--timeout == 0) { ... return -1; }
usleep(1000);  // Chờ 1ms giữa mỗi lần kiểm tra
               // Tổng thời gian timeout tối đa = 1000 × 1ms = 1 giây
```

> **Bài học**: Luôn có timeout khi polling thanh ghi phần cứng. Nếu module bị lỗi hoặc clock source không khả dụng, vòng lặp vô hạn sẽ treo chương trình.

---


## 9. Câu hỏi kiểm tra

1. Tại sao phải bật clock module trước khi ghi vào thanh ghi GPIO?
2. `CM_PER` và `CM_WKUP` khác nhau ở điểm nào? Module nào thuộc CM_WKUP?
3. Đọc thanh ghi `CM_PER_GPIO1_CLKCTRL` thấy giá trị `0x00040002`. Bit IDLEST đang ở trạng thái gì?
4. Clock gating giúp ích gì trong hệ thống nhúng chạy pin?
5. GPIO1 cần bật thêm bit 18. Bit đó tên gì và có tác dụng gì?

---

## 10. Tài liệu tham khảo

| Nội dung | Tài liệu | Section |
|----------|----------|---------|
| Tổng quan PRCM | spruh73q.pdf | Chapter 8, Section 8.1 |
| CM_PER registers | spruh73q.pdf | Section 8.1.12.1 |
| CM_WKUP registers | spruh73q.pdf | Section 8.1.12.2 |
| CM_PER_GPIO1_CLKCTRL | spruh73q.pdf | Section 8.1.12.1.28 |
| IDLEST bit definition | spruh73q.pdf | Section 8.1.12 (Register Description) |


## 11. Ghi chú thực hành (tóm tắt)

- `PRCM_SIZE = 0x2000` (8KB) là lựa chọn an toàn và tiện lợi: hệ thống page thường 4KB, nhưng ánh xạ 8KB từ `0x44E00000` cho phép truy cập đồng thời `CM_PER` và `CM_WKUP` (và một vài CM lân cận) bằng một lần `mmap()`. Một mapping 4KB có thể đủ trong nhiều trường hợp, nhưng 8KB giảm nhu cầu remap.
- Bật clock thực hiện bằng cách ghi vào một **CLKCTRL register** của module (ví dụ `CM_PER_GPIO1_CLKCTRL`); thao tác này thiết lập `MODULEMODE` và có thể kèm các bit tùy chọn (ví dụ `OPTFCLKEN` cho `GPIO1`). Không nhầm với việc thao tác bit dữ liệu của GPIO (SET/CLEAR DATAOUT).
- Để kiểm tra trạng thái dùng `IDLEST_MASK = (0x3 << IDLEST_SHIFT)` (tức `0x00030000`) rồi dịch phải `IDLEST_SHIFT` để lấy giá trị 0..3 tương ứng Functional/Transition/Idle/Disabled.
- Dùng `volatile` cho con trỏ MMIO (ví dụ `volatile uint32_t *`) để bắt buộc compiler thực hiện đọc/ghi thực tế tới địa chỉ phần cứng, tránh việc tối ưu hóa làm mất truy cập.
- PRCM có cơ chế tự động gate/open clocks theo idle/standby protocol nếu module được cấu hình (smart-idle), nhưng phần mềm vẫn phải enable module (gọi `MODULEMODE = ENABLE`), cấu hình idle modes hoặc bật optional clocks. Trên Linux, kernel drivers / runtime-PM thường xử lý việc này cho userspace.
- Một số ví dụ (ví dụ blink) không hiển thị chờ `IDLEST` vì bootloader/kernel có thể đã bật clock trước — khi thao tác trực tiếp với PRCM qua `/dev/mem`, nên poll `IDLEST` với timeout để an toàn.
- `GPIO1` có `OPTFCLKEN_GPIO_1_GDBCLK` (bit 18) — optional functional clock dùng cho debounce/functional features; bật bằng cách đặt bit này trong `CM_PER_GPIO1_CLKCTRL` (ví dụ `0x40002`).