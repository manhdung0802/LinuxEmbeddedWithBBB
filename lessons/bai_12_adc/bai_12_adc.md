# Bài 12 - ADC (TSC_ADC): Đọc Analog và Touchscreen Controller

## 1. Mục tiêu bài học

- Hiểu kiến trúc ADC trên AM335x: TSC_ADC (Touchscreen Controller + ADC)
- Nắm vững các kênh ADC, range điện áp, độ phân giải
- Cấu hình ADC standalone (không dùng touchscreen)
- Đọc giá trị analog từ chân AIN0..AIN7 bằng register-level
- Sử dụng ADC qua Linux IIO subsystem (`/sys/bus/iio/`)

---

## 2. Tổng quan TSC_ADC trên AM335x

AM335x tích hợp một module **TSC_ADC_SS** (TouchScreen Controller & Analog-to-Digital Converter):

- **ADC 12-bit**: 4096 mức (0..4095)
- **8 kênh input**: AIN0..AIN7
- **Reference voltage**: VREFP (thường nối VDD_ADC = 1.8V trên BBB)
- **Sample rate tối đa**: ~200ksps (samples per second)
- Có thể dùng 4 kênh cho touchscreen, còn lại cho generic ADC

> **Nguồn**: spruh73q.pdf, Chapter 12 (TSC_ADC_SS)

### 2.1 Quan trọng: Điện áp Input

**CẢNH BÁO**: AIN pins trên BBB chịu tối đa **1.8V**, KHÔNG phải 3.3V hay 5V.

Nếu đưa tín hiệu > 1.8V vào AIN: **sẽ hỏng SoC vĩnh viễn**.

```
AIN voltage range: 0V .. 1.8V (VREFN=GND, VREFP=1.8V)
Công thức: V_in = (ADC_value / 4095) × 1.8V
```

> **Nguồn**: BBB_SRM_C.pdf, Section 7.1 (Analog Input)

### 2.2 Chân AIN trên BBB

| Kênh | Chân P9 | Ghi chú |
|------|---------|---------|
| AIN0 | P9.39 | |
| AIN1 | P9.40 | |
| AIN2 | P9.37 | |
| AIN3 | P9.38 | |
| AIN4 | P9.33 | |
| AIN5 | P9.36 | |
| AIN6 | P9.35 | |
| AIN7 | VREFN (GND reference) | Không dùng làm input |

> **Nguồn**: BeagleboneBlackP9HeaderTable.pdf

---

## 3. Base Address và Clock

| Module | Base Address |
|--------|-------------|
| TSC_ADC_SS | `0x44E0D000` |

Clock enable:
```
CM_WKUP_ADC_TSC_CLKCTRL = 0x44E004BC
Giá trị: 0x02 (MODULEMODE = ENABLE)
```

> **Nguồn**: spruh73q.pdf, Section 8.1.12.2 (CM_WKUP)

---

## 4. Thanh ghi TSC_ADC quan trọng

| Thanh ghi | Offset | Chức năng |
|-----------|--------|-----------|
| `ADC_REVISION`      | `0x000` | Revision |
| `ADC_SYSCONFIG`     | `0x010` | Idle mode config |
| `ADC_SYSSTATUS`     | `0x014` | Reset Done |
| `ADC_IRQSTATUS`     | `0x028` | Interrupt status |
| `ADC_IRQENABLE_SET` | `0x02C` | Bật interrupt |
| `ADC_CTRL`          | `0x040` | Control: enable, power down, step config |
| `ADC_ADCSTAT`       | `0x044` | FSM state, FIFO busy |
| `ADC_ADCRANGE`      | `0x048` | VREFN, VREFP select |
| `ADC_CLKDIV`        | `0x04C` | Clock divider |
| `ADC_STEPENABLE`    | `0x054` | Bật step (step = 1 lần lấy mẫu 1 kênh) |
| `ADC_STEPCONFIG1`   | `0x064` | Cấu hình step 1: kênh, mode, averaging |
| `ADC_STEPDELAY1`    | `0x068` | Delay cho step 1 |
| `ADC_FIFO0COUNT`    | `0x0E4` | Số sample trong FIFO0 |
| `ADC_FIFO0THRESHOLD`| `0x0E8` | Ngưỡng FIFO0 để trigger interrupt |
| `ADC_FIFO0DATA`     | `0x100` | Đọc dữ liệu ADC từ FIFO0 (12-bit + step ID) |

> **Nguồn**: spruh73q.pdf, Table 12-6 (TSC_ADC Register Map)

### 4.1 ADC_STEPCONFIG - Cấu hình mỗi step

```
Bit 27:     FIFO_SELECT  — 0=FIFO0, 1=FIFO1
Bit 26:     DIFF_CNTRL   — 0=single-ended, 1=differential
Bit 25..19: RFP          — Positive reference select
Bit 18..15: INP          — Input channel select (0..7 = AIN0..AIN7)
Bit 14..12: INM          — Negative input (thường = VREFN = 8)
Bit 5..2:   AVERAGING    — 0=no avg, 1=2 samples, ..., 4=16 samples
Bit 1..0:   MODE         — 0=SW enabled, 1=HW sync, 2=continuous
```

---

## 5. Trình tự đọc ADC

```
1. Bật clock TSC_ADC (CM_WKUP_ADC_TSC_CLKCTRL)
2. Disable module (ADC_CTRL[0] = 0) để cấu hình
3. Set step idle config (ADC_IDLECONFIG)
4. Cấu hình ADC_STEPCONFIGx: kênh, mode SW, no averaging
5. Cấu hình ADC_STEPDELAYx: open delay, sample delay
6. Enable module (ADC_CTRL[0] = 1, TSC mode = 0x7)
7. Enable step tương ứng (ADC_STEPENABLE)
8. Đợi FIFO0COUNT > 0
9. Đọc từ ADC_FIFO0DATA (bit 11..0 = ADC value, bit 19..16 = step ID)
```

---

## 6. Code C - Đọc ADC AIN0 bằng mmap

```c
/*
 * adc_read.c
 * Đọc giá trị ADC kênh AIN0 (P9.39) bằng mmap /dev/mem
 * Biên dịch: gcc -o adc adc_read.c
 * Chạy: sudo ./adc
 *
 * CẢNH BÁO: Input tối đa 1.8V trên các chân AIN!
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>

/* === Base address (spruh73q.pdf, Table 2-3) === */
#define ADC_BASE        0x44E0D000UL
#define CM_WKUP_BASE    0x44E00400UL

/* === ADC register offsets (spruh73q.pdf, Table 12-6) === */
#define ADC_SYSCONFIG       0x010
#define ADC_IRQSTATUS       0x028
#define ADC_IRQENABLE_SET   0x02C
#define ADC_CTRL            0x040
#define ADC_CLKDIV          0x04C
#define ADC_STEPENABLE      0x054
#define ADC_IDLECONFIG      0x058
#define ADC_STEPCONFIG1     0x064
#define ADC_STEPDELAY1      0x068
#define ADC_FIFO0COUNT      0x0E4
#define ADC_FIFO0DATA       0x100

/* === CM_WKUP offset (spruh73q.pdf, Section 8.1.12.2.22) === */
#define CM_WKUP_ADC_TSC_CLKCTRL  0x0BC  /* offset trong CM_WKUP */

/* === ADC_CTRL bits === */
#define ADC_CTRL_ENABLE     (1 << 0)
#define ADC_CTRL_STEPCONFIG_WRITEPROTECT_N (1 << 2)
/* Bits [5:4] = TSC_MODE: 0x7 = ADC only (no touchscreen) */
#define ADC_CTRL_ADC_BIAS_SELECT (1 << 1)

/* === STEPCONFIG bits === */
/* Mode=0 (SW enabled), FIFO0, single-ended, INP=AIN0 (=0) */
#define STEP1_CONFIG    0x00000000U  /* INP=0(AIN0), mode=SW, FIFO0 */
#define STEP1_DELAY     0x00000F00U  /* sample delay = 15 ADC clocks */

#define PAGE_SIZE 4096
#define ADC_CLK_DIV 3   /* ADCCLK = sysclk/CLKdiv (sysclk=24MHz → 6MHz) */

int main(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }

    volatile uint32_t *adc = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE,
                                  MAP_SHARED, fd, ADC_BASE);
    volatile uint32_t *cm_wkup = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE,
                                      MAP_SHARED, fd, CM_WKUP_BASE);

    if (adc == MAP_FAILED || cm_wkup == MAP_FAILED) {
        perror("mmap"); return 1;
    }

    /* 1. Bật clock ADC */
    cm_wkup[CM_WKUP_ADC_TSC_CLKCTRL / 4] = 0x02;
    usleep(10000);

    /* 2. Disable để cấu hình (xóa ENABLE bit) */
    adc[ADC_CTRL / 4] = ADC_CTRL_STEPCONFIG_WRITEPROTECT_N;

    /* 3. Clock divider */
    adc[ADC_CLKDIV / 4] = ADC_CLK_DIV;

    /* 4. Cấu hình step 1: AIN0, software trigger, FIFO0 */
    adc[ADC_STEPCONFIG1 / 4] = STEP1_CONFIG;
    adc[ADC_STEPDELAY1  / 4] = STEP1_DELAY;

    /* 5. Enable module */
    adc[ADC_CTRL / 4] = ADC_CTRL_ENABLE | ADC_CTRL_STEPCONFIG_WRITEPROTECT_N;

    /* 6. Đọc 10 lần */
    for (int i = 0; i < 10; i++) {
        /* Trigger step 1 (bit 1 = step1) */
        adc[ADC_STEPENABLE / 4] = (1 << 1);

        /* Đợi FIFO0 có dữ liệu */
        while (adc[ADC_FIFO0COUNT / 4] == 0)
            ;

        /* Đọc raw value (bit 11..0) */
        uint32_t raw = adc[ADC_FIFO0DATA / 4] & 0xFFF;
        float voltage = raw * 1.8f / 4095.0f;
        printf("AIN0 = %4u  (%.4f V)\n", raw, voltage);
        usleep(100000);  /* 100ms */
    }

    munmap((void*)adc, PAGE_SIZE);
    munmap((void*)cm_wkup, PAGE_SIZE);
    close(fd);
    return 0;
}
```

### Giải thích chi tiết từng dòng code `adc_read.c`

#### a) ADC nằm ở Wakeup domain

```c
#define ADC_BASE     0x44E0D000UL  // TSC_ADC base — thuộc L4_WKUP (không phải L4_PER)
#define CM_WKUP_BASE 0x44E00400UL  // Vì vậy clock cũng thuộc CM_WKUP chứ không phải CM_PER
```

#### b) Cấu hình ADC module

```c
adc[ADC_CTRL / 4] = ADC_CTRL_STEPCONFIG_WRITEPROTECT_N;
// Bit 2 = 1 → cho phép ghi vào STEPCONFIG (bỏ write-protect)
// Bit 0 = 0 → module vẫn disable — BẮT BUỘC disable trước khi cấu hình
```

```c
adc[ADC_CLKDIV / 4] = ADC_CLK_DIV;  // = 3 → ADC clock = 24MHz / (3+1) = 6MHz
                                     // ADC clock tối đa = 13MHz (datasheet)
```

```c
adc[ADC_STEPCONFIG1 / 4] = STEP1_CONFIG;  // = 0x00000000
// Bit [1:0] = 0 → Mode SW-enabled (trigger bằng phần mềm)
// Bit [18:15] = 0 → INP = AIN0 (kênh input 0)
// Bit 27 = 0 → FIFO0 (ghi kết quả vào FIFO 0)
// Bit 26 = 0 → Single-ended (không phải differential)

adc[ADC_STEPDELAY1 / 4] = STEP1_DELAY;   // = 0x00000F00
// Bit [17:8] = 0x0F = 15 → sample delay = 15 ADC clock cycles
// Delay để tụ điện ổn định trước khi lấy mẫu
```

#### c) Enable và trigger

```c
adc[ADC_CTRL / 4] = ADC_CTRL_ENABLE | ADC_CTRL_STEPCONFIG_WRITEPROTECT_N;
// Bit 0 = 1 → Enable module
// Bit 2 = 1 → giữ write-protect off (để có thể trigger lại)
```

```c
adc[ADC_STEPENABLE / 4] = (1 << 1);  // Bật Step 1 (bit 1)
// Bit 0 = charge step, Bit 1 = step 1, Bit 2 = step 2, ...
// Khi set, ADC thực hiện 1 lần lấy mẫu theo cấu hình STEPCONFIG1
// Sau khi xong, bit tự clear về 0
```

#### d) Đọc kết quả từ FIFO

```c
while (adc[ADC_FIFO0COUNT / 4] == 0)  // Chờ FIFO có dữ liệu
    ;  // ADC mất vài µs để chuyển đổi

uint32_t raw = adc[ADC_FIFO0DATA / 4] & 0xFFF;
// FIFO0DATA format: bit [19:16] = Step ID, bit [11:0] = ADC value
// & 0xFFF: chỉ lấy 12 bit thấp (giá trị 0..4095)

float voltage = raw * 1.8f / 4095.0f;
// Vref = 1.8V (cố định trên BBB — KHÔNG phải 3.3V!)
// Độ phân giải = 1.8V / 4095 ≈ 0.44 mV/LSB
```

> **Cảnh báo**: Chân AIN trên BBB chỉ chịu được tối đa **1.8V**. Đưa điện áp > 1.8V sẽ **hư hỏng vĩnh viễn** ADC và có thể hư board.

---

## 7. ADC qua Linux IIO Subsystem

Linux kernel export ADC qua **Industrial I/O (IIO)** subsystem:

```bash
# Load ADC driver
modprobe ti_am335x_adc

# Đường dẫn IIO
ls /sys/bus/iio/devices/iio:device0/

# Đọc kênh AIN0
cat /sys/bus/iio/devices/iio:device0/in_voltage0_raw

# Đọc tất cả kênh
for i in 0 1 2 3 4 5 6; do
    echo -n "AIN$i = "
    cat /sys/bus/iio/devices/iio:device0/in_voltage${i}_raw
done
```

**Chú ý**: Khi dùng IIO driver, không thể đồng thời dùng mmap ADC registers (xung đột với kernel driver).

---

## 8. Câu hỏi kiểm tra

1. Điện áp tối đa cho phép trên chân AIN của BBB là bao nhiêu? Tại sao?
2. ADC đọc giá trị 2048. Điện áp input tương ứng là bao nhiêu?
3. ADC 12-bit nghĩa là gì? Độ phân giải điện áp tối thiểu là bao nhiêu mV?
4. Tại sao phải disable module trước khi ghi `ADC_STEPCONFIG`?
5. Sự khác biệt giữa đọc ADC bằng mmap và IIO subsystem?

---

## 9. Tài liệu tham khảo

| Nội dung | Tài liệu | Section |
|----------|----------|---------|
| TSC_ADC overview | spruh73q.pdf | Chapter 12, Section 12.1 |
| TSC_ADC register map | spruh73q.pdf | Table 12-6 |
| ADC base address | spruh73q.pdf | Table 2-3 |
| CM_WKUP_ADC_TSC_CLKCTRL | spruh73q.pdf | Section 8.1.12.2.22 |
| AIN pins P9.33..P9.40 | BeagleboneBlackP9HeaderTable.pdf | — |
| ADC voltage limits | BBB_SRM_C.pdf | Section 7.1 |
