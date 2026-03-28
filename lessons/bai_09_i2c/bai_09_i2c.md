# Bài 9 - I2C: Master/Slave, Đọc EEPROM và Cảm Biến

## 1. Mục tiêu bài học

- Hiểu giao thức I2C: địa chỉ 7-bit, START/STOP condition, ACK/NACK
- Nắm vững thanh ghi I2C trên AM335x
- Viết code C giao tiếp I2C bằng register-level (mmap)
- Đọc EEPROM 24C32 và cảm biến nhiệt độ TMP102 (thực hành)
- Dùng Linux I2C từ userspace qua `/dev/i2c-*` và `i2c-dev`

---

## 2. Giao thức I2C cơ bản

**I2C** (Inter-Integrated Circuit) là giao tiếp serial đồng bộ 2 dây:
- **SCL**: Serial Clock (do master phát)
- **SDA**: Serial Data (bidirectional)

### 2.1 Trình tự giao tiếp I2C

```
START → [7-bit Addr][R/W] → ACK → [data byte] → ACK → ... → STOP
  │                           │
  └── SDA từ HIGH→LOW khi     └── Slave kéo SDA xuống LOW
      SCL đang HIGH               để báo nhận thành công
```

### 2.2 Write transaction (Master → Slave)

```
S │ Addr(7) │W│ A │ Data0 │ A │ Data1 │ A │ P
  └─────────────┘   └───────┘   └───────┘
  S = START, A = ACK, P = STOP, W = 0 (write)
```

### 2.3 Read transaction (Slave → Master)

```
S │ Addr(7) │R│ A │ Data0 │ A │ Data1 │ NA│ P
                    └───────┘   └───────┘
                    Data từ slave, master phát clock
                    NA = NACK (master không cần byte nữa)
```

---

## 3. I2C trên AM335x

AM335x có **3 module I2C**:

| Module | Base Address | Linux device | Header |
|--------|-------------|--------------|--------|
| I2C0   | `0x44E0B000` | `/dev/i2c-0` | Internal (EEPROM on board) |
| I2C1   | `0x4802A000` | `/dev/i2c-1` | P9.17/P9.18 |
| I2C2   | `0x4819C000` | `/dev/i2c-2` | P9.19/P9.20 |

> **Nguồn**: spruh73q.pdf, Table 2-3 (I2C0 trong L4_WKUP), Table 2-4 (I2C1, I2C2)

**Lưu ý quan trọng**:
- I2C0 được nối với EEPROM 24C256 trên board BBB (chứa board ID)
- I2C1 thường được dùng để kết nối thiết bị ngoài — **đây là I2C chính để thực hành**

---

## 4. Thanh ghi I2C quan trọng

| Thanh ghi | Offset | Chức năng |
|-----------|--------|-----------|
| `I2C_REVNB_LO` | `0x00` | Revision |
| `I2C_SYSC`     | `0x10` | System config (prescaler, idle) |
| `I2C_IRQSTATUS`| `0x28` | Interrupt status (xóa bằng ghi 1) |
| `I2C_IRQENABLE_SET` | `0x2C` | Bật interrupt |
| `I2C_WE`       | `0x34` | Wakeup enable |
| `I2C_DCOUNT`   | `0x98` | Số byte cần truyền/nhận |
| `I2C_DATA`     | `0x9C` | Data register (đọc/ghi 1 byte) |
| `I2C_CON`      | `0xA4` | Control: enable, master/slave, TX/RX, START/STOP |
| `I2C_OA`       | `0xA8` | Own Address (địa chỉ của controller này) |
| `I2C_SA`       | `0xAC` | Slave Address (target device) |
| `I2C_PSC`      | `0xB0` | Prescaler để tính clock |
| `I2C_SCLL`     | `0xB4` | SCL Low time |
| `I2C_SCLH`     | `0xB8` | SCL High time |
| `I2C_SYSTEST`  | `0xBC` | Test (loopback) |
| `I2C_BUFSTAT`  | `0xC0` | Buffer status (TX/RX FIFO count) |
| `I2C_IRQSTATUS_RAW` | `0x24` | Raw interrupt status (trước mask) |

> **Nguồn**: spruh73q.pdf, Table 21-4 (I2C Register Map)

### 4.1 I2C_CON - Register quan trọng nhất

```
Bit 15: I2C_EN  — 1 = enable module
Bit 12: MST     — 1 = Master mode, 0 = Slave
Bit 10: TRX     — 1 = Transmit, 0 = Receive
Bit  9: XA      — 0 = 7-bit addr, 1 = 10-bit addr
Bit  2: STP     — 1 = Generate STOP condition
Bit  1: STT     — 1 = Generate START condition
```

### 4.2 I2C_IRQSTATUS - Bit quan trọng

```
Bit  4: RRDY — Receive data ready (có thể đọc I2C_DATA)
Bit  3: XRDY — Transmit data ready (có thể ghi I2C_DATA)
Bit  2: ARDY — Access ready (transaction hoàn tất)
Bit  1: NACK — No acknowledgment nhận được
Bit  0: AL   — Arbitration lost
```

---

## 5. Tính toán I2C Clock

Công thức (spruh73q.pdf, Section 21.3.1.3):

$$F_{SCL} = \frac{F_{I2C}}{(SCLL + 7) + (SCLH + 5)}$$

với $F_{I2C} = \frac{F_{sys}}{PSC + 1}$

**Ví dụ cho 100kHz (Standard Mode), F_sys = 48MHz**:

$F_{I2C} = \frac{48\text{MHz}}{PSC+1}$ → chọn PSC = 3 → $F_{I2C} = 12\text{MHz}$

$\text{Tổng divisor} = \frac{12\text{MHz}}{100\text{kHz}} = 120$

Chia đều: SCLL = 53, SCLH = 55 (cùng ≈ 60, trừ offset 7 và 5)

**Cho 400kHz (Fast Mode)**:
- PSC = 1 → $F_{I2C} = 24\text{MHz}$
- Tổng divisor = 60 → SCLL = 23, SCLH = 25

> **Nguồn**: spruh73q.pdf, Section 21.3.1.3

---

## 6. Code C - I2C Register Level (mmap)

```c
/*
 * i2c_read_tmp102.c
 * Đọc nhiệt độ từ cảm biến TMP102 (addr 0x48) qua I2C1
 * qua mmap /dev/mem
 *
 * TMP102 trên P9.17 (SCL) và P9.18 (SDA)
 * Biên dịch: gcc -o i2c_tmp102 i2c_read_tmp102.c
 * Chạy: sudo ./i2c_tmp102
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>

/* === Base addresses (spruh73q.pdf, Table 2-4) === */
#define I2C1_BASE           0x4802A000UL
#define CTRL_MODULE_BASE    0x44E10000UL
#define CM_PER_BASE         0x44E00000UL

/* === I2C register offsets (spruh73q.pdf, Table 21-4) === */
#define I2C_SYSC    0x10
#define I2C_IRQSTATUS 0x28
#define I2C_IRQENABLE_SET 0x2C
#define I2C_DCOUNT  0x98
#define I2C_DATA    0x9C
#define I2C_CON     0xA4
#define I2C_SA      0xAC
#define I2C_PSC     0xB0
#define I2C_SCLL    0xB4
#define I2C_SCLH    0xB8

/* === I2C_CON bits === */
#define I2C_CON_EN  (1 << 15)
#define I2C_CON_MST (1 << 12)
#define I2C_CON_TRX (1 << 10)
#define I2C_CON_STP (1 << 2)
#define I2C_CON_STT (1 << 1)

/* === I2C_IRQSTATUS bits === */
#define I2C_IRQSTATUS_RRDY  (1 << 4)
#define I2C_IRQSTATUS_XRDY  (1 << 3)
#define I2C_IRQSTATUS_ARDY  (1 << 2)
#define I2C_IRQSTATUS_NACK  (1 << 1)

/* === Clock/Pinmux offsets === */
#define CM_PER_I2C1_CLKCTRL 0x048  /* spruh73q.pdf, Section 8.1.12.1.14 */
#define CONF_SPI0_D1        0x958  /* I2C1_SDA = P9.18 */
#define CONF_SPI0_CS0       0x95C  /* I2C1_SCL = P9.17 */

#define PAGE_SIZE 4096
#define TMP102_ADDR 0x48  /* địa chỉ TMP102 mặc định */

static volatile uint32_t *i2c1;
static volatile uint32_t *ctrl;
static volatile uint32_t *cm_per;

int i2c_init(int fd)
{
    i2c1  = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, I2C1_BASE);
    ctrl  = mmap(NULL, PAGE_SIZE*16, PROT_READ|PROT_WRITE, MAP_SHARED, fd, CTRL_MODULE_BASE);
    cm_per= mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, CM_PER_BASE);
    if (i2c1==MAP_FAILED || ctrl==MAP_FAILED || cm_per==MAP_FAILED) {
        perror("mmap"); return -1;
    }

    /* 1. Bật clock I2C1 */
    cm_per[CM_PER_I2C1_CLKCTRL / 4] = 0x2;
    usleep(10000);

    /* 2. Pinmux: SDA và SCL ở Mode 2 = I2C1 */
    ctrl[CONF_SPI0_D1  / 4] = 0x32;   /* Mode 2, RXACTIVE, pull-up */
    ctrl[CONF_SPI0_CS0 / 4] = 0x32;

    /* 3. Reset và disable I2C trước khi cấu hình */
    i2c1[I2C_CON / 4] = 0;            /* disable */
    i2c1[I2C_SYSC / 4] = 0x2;         /* SRST=1 (soft reset) */
    usleep(1000);

    /* 4. Cấu hình clock: PSC=3, SCLL=53, SCLH=55 → ~100kHz */
    i2c1[I2C_PSC  / 4] = 3;
    i2c1[I2C_SCLL / 4] = 53;
    i2c1[I2C_SCLH / 4] = 55;

    /* 5. Enable I2C, Master mode */
    i2c1[I2C_CON / 4] = I2C_CON_EN;

    return 0;
}

/* Đọc N byte từ slave_addr */
int i2c_read(uint8_t slave_addr, uint8_t *buf, uint8_t len)
{
    uint32_t irqstatus;
    int i;

    /* Set slave address và số byte cần đọc */
    i2c1[I2C_SA     / 4] = slave_addr;
    i2c1[I2C_DCOUNT / 4] = len;

    /* START + Master + Receive (TRX=0) + Repeat START sẽ được set khi có data */
    i2c1[I2C_CON / 4] = I2C_CON_EN | I2C_CON_MST | I2C_CON_STT;
    /* Không set TRX → đây là receive */

    for (i = 0; i < len; i++) {
        /* Đợi RRDY hoặc NACK */
        do {
            irqstatus = i2c1[I2C_IRQSTATUS / 4];
            if (irqstatus & I2C_IRQSTATUS_NACK) {
                printf("NACK từ slave 0x%02X\n", slave_addr);
                i2c1[I2C_CON / 4] |= I2C_CON_STP;
                return -1;
            }
        } while (!(irqstatus & I2C_IRQSTATUS_RRDY));

        /* Đọc 1 byte */
        buf[i] = (uint8_t)(i2c1[I2C_DATA / 4] & 0xFF);
        /* Clear RRDY */
        i2c1[I2C_IRQSTATUS / 4] = I2C_IRQSTATUS_RRDY;
    }

    /* Phát STOP */
    i2c1[I2C_CON / 4] |= I2C_CON_STP;
    /* Đợi ARDY */
    while (!(i2c1[I2C_IRQSTATUS / 4] & I2C_IRQSTATUS_ARDY))
        ;
    i2c1[I2C_IRQSTATUS / 4] = I2C_IRQSTATUS_ARDY;

    return len;
}

int main(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }

    if (i2c_init(fd) != 0) return 1;

    /* TMP102: đọc 2 byte từ Temperature Register (pointer = 0x00) */
    uint8_t buf[2];
    if (i2c_read(TMP102_ADDR, buf, 2) == 2) {
        /* Chuyển đổi raw data → nhiệt độ Celsius */
        int16_t raw = ((int16_t)(buf[0] << 8) | buf[1]) >> 4;
        float temp = raw * 0.0625f;
        printf("Nhiệt độ TMP102: %.2f °C\n", temp);
    }

    return 0;
}
```

---

## 7. I2C từ Linux Userspace (`/dev/i2c-1`)

Cách đơn giản hơn dùng `i2c-dev` kernel module:

```c
#include <stdio.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdint.h>

#define I2C_BUS     "/dev/i2c-1"
#define TMP102_ADDR 0x48

int main(void)
{
    int fd = open(I2C_BUS, O_RDWR);
    if (fd < 0) { perror("open i2c"); return 1; }

    /* Đặt địa chỉ slave */
    if (ioctl(fd, I2C_SLAVE, TMP102_ADDR) < 0) {
        perror("ioctl I2C_SLAVE"); return 1;
    }

    /* Đọc 2 byte */
    uint8_t buf[2];
    if (read(fd, buf, 2) == 2) {
        int16_t raw = ((int16_t)(buf[0] << 8) | buf[1]) >> 4;
        float temp = raw * 0.0625f;
        printf("Nhiệt độ: %.2f °C\n", temp);
    }

    close(fd);
    return 0;
}
```

**Bật module i2c-dev trên BBB**:
```bash
modprobe i2c-dev
ls /dev/i2c-*
```

**Công cụ hữu ích**:
```bash
i2cdetect -y -r 1    # scan I2C bus 1
i2cget -y 1 0x48 0x00 w  # đọc word từ device 0x48, register 0x00
```

---

## 8. Câu hỏi kiểm tra

1. I2C1 trên BBB ở chân nào của header P9? (Tra bảng P9)
2. Nếu slave không trả lời, bit nào trong `I2C_IRQSTATUS` báo hiệu?
3. Với PSC=1, SCLL=23, SCLH=25, tần số SCL là bao nhiêu kHz?
4. Tại sao I2C cần điện trở pull-up trên SDA và SCL?
5. Sự khác biệt giữa `I2C_IRQSTATUS` và `I2C_IRQSTATUS_RAW` là gì?

---

## 9. Tài liệu tham khảo

| Nội dung | Tài liệu | Section |
|----------|----------|---------|
| I2C module overview | spruh73q.pdf | Chapter 21, Section 21.1 |
| I2C register map | spruh73q.pdf | Table 21-4 |
| I2C clock calculation | spruh73q.pdf | Section 21.3.1.3 |
| I2C base addresses | spruh73q.pdf | Table 2-3, 2-4 |
| CM_PER_I2C1_CLKCTRL | spruh73q.pdf | Section 8.1.12.1.14 |
| I2C1 pins P9.17/P9.18 | BeagleboneBlackP9HeaderTable.pdf | — |
| BBB board EEPROM | BBB_SRM_C.pdf | Section 6.1 |
