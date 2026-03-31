# Bài 10 - SPI: Giao Tiếp với ADC/DAC Bên Ngoài

## 1. Mục tiêu bài học

- Hiểu giao thức SPI: MOSI, MISO, SCLK, CS, 4 mode (CPOL/CPHA)
- Nắm vững module McSPI (Multichannel SPI) trên AM335x
- Hiểu các thanh ghi McSPI và cấu hình clock, frame format
- Viết code C giao tiếp SPI với ADC MCP3204 (12-bit, 4-channel)
- Dùng SPI từ Linux userspace qua `/dev/spidevX.Y`

---

## 2. Giao thức SPI cơ bản

**SPI** (Serial Peripheral Interface) là giao tiếp serial đồng bộ tốc độ cao:

```
Master                     Slave
  ┌─────┐                 ┌─────┐
  │SCLK ├────────────────►│SCLK │
  │MOSI ├────────────────►│MOSI │  (Master Out Slave In)
  │MISO │◄────────────────┤MISO │  (Master In Slave Out)
  │CS   ├────────────────►│CS   │  (Chip Select, active LOW)
  └─────┘                 └─────┘
```

Đặc điểm:
- **Full-duplex**: gửi và nhận đồng thời
- **Không cần ACK**: nhanh hơn I2C
- **CS riêng cho mỗi slave**: dễ mở rộng nhiều thiết bị
- **Tốc độ cao**: thường 1-50 MHz

### 2.1 Bốn Mode SPI (CPOL và CPHA)

| Mode | CPOL | CPHA | Clock idle | Lấy mẫu tại |
|------|------|------|------------|-------------|
| 0    | 0    | 0    | LOW        | Rising edge |
| 1    | 0    | 1    | LOW        | Falling edge|
| 2    | 1    | 0    | HIGH       | Falling edge|
| 3    | 1    | 1    | HIGH       | Rising edge |

Hầu hết ADC/DAC dùng **Mode 0** (CPOL=0, CPHA=0).

---

## 3. McSPI trên AM335x

AM335x dùng module **McSPI** (MultiChannel SPI), hỗ trợ nhiều CS (channel):

| Module | Base Address | Linux device | Header |
|--------|-------------|--------------|--------|
| SPI0   | `0x48030000` | `/dev/spidev0.X` | P9.17~P9.22 |
| SPI1   | `0x481A0000` | `/dev/spidev1.X` | P9.28~P9.32 |

Mỗi McSPI module có thể có tới **4 channel** (CS0..CS3).

> **Nguồn**: spruh73q.pdf, Table 2-4 (McSPI base addresses)

**Pinout SPI0 trên P9 (BBB)**:

| Chân P9 | Tên | SPI0 |
|---------|-----|------|
| P9.22  | spi0_sclk | SCLK |
| P9.21  | spi0_d0   | MISO |
| P9.18  | spi0_d1   | MOSI |
| P9.17  | spi0_cs0  | CS0  |

> **Nguồn**: BeagleboneBlackP9HeaderTable.pdf

---

## 4. Thanh ghi McSPI quan trọng

| Thanh ghi | Offset | Chức năng |
|-----------|--------|-----------|
| `MCSPI_REVISION`   | `0x000` | Revision |
| `MCSPI_SYSCONFIG`  | `0x110` | Module config (idle, wakeup) |
| `MCSPI_SYSSTATUS`  | `0x114` | Status (RESETDONE) |
| `MCSPI_IRQSTATUS`  | `0x118` | Interrupt status |
| `MCSPI_IRQENABLE`  | `0x11C` | Interrupt enable |
| `MCSPI_MODULCTRL`  | `0x128` | Master/Slave, Single/Multi channel |
| `MCSPI_CHxCONF`    | `0x12C` (ch0) | Channel config: CPOL, CPHA, WL, speed, CS |
| `MCSPI_CHxSTAT`    | `0x130` (ch0) | Channel status: TXFFE, RXFFE, TXS, RXS |
| `MCSPI_CHxCTRL`    | `0x134` (ch0) | Channel enable |
| `MCSPI_TX0`        | `0x138` | Transmit data channel 0 |
| `MCSPI_RX0`        | `0x13C` | Receive data channel 0 |

> **Nguồn**: spruh73q.pdf, Table 24-5 (McSPI Register Map)

Với channel 1, 2, 3: offset = `0x12C + channel * 0x14`

### 4.1 MCSPI_CHxCONF - Register cấu hình chính

```
Bit 31..28: CLKD   — Clock divider (F_clk / 2^CLKD)
Bit 27:     FORCE  — 1 = giữ CS active (dùng cho multi-word transfer)
Bit 25..21: WL     — Word length - 1 (ví dụ WL=7 → 8-bit, WL=15 → 16-bit)
Bit 20:     TRM    — 0=TX+RX, 1=RX only, 2=TX only
Bit 18:     DPE1   — Data line selection (MISO)
Bit 17:     DPE0   — Data line selection (MOSI)
Bit 16:     IS     — Input select
Bit  6:     EPOL   — CS polarity: 0=active LOW, 1=active HIGH
Bit  1:     POL    — CPOL
Bit  0:     PHA    — CPHA
```

### 4.2 MCSPI_CHxSTAT - Status

```
Bit 3: RXFFE — RX FIFO full
Bit 2: RXFFE — RX FIFO empty (cần đợi = 0 để đọc)
Bit 1: TXFFE — TX FIFO empty
Bit 0: RXS   — RX register full (có dữ liệu để đọc)
```

---

## 5. Tính SPI Clock

$$F_{SPI} = \frac{F_{module}}{2^{CLKD+1}}$$

với $F_{module}$ = 48MHz (từ DPLL_PER):

| CLKD | F_SPI |
|------|-------|
| 0    | 24 MHz |
| 1    | 12 MHz |
| 2    | 6 MHz  |
| 3    | 3 MHz  |
| 4    | 1.5 MHz|

> **Nguồn**: spruh73q.pdf, Section 24.3.7 (Clock Generation)

---

## 6. Code C - SPI với ADC MCP3204

ADC **MCP3204**: 12-bit, 4-channel, SPI Mode 0, tốc độ tối đa 2 MHz.

```c
/*
 * spi_mcp3204.c
 * Đọc giá trị ADC từ MCP3204 (12-bit, 4-channel) qua SPI0
 * bằng mmap /dev/mem
 *
 * Wiring: MCP3204 nối với P9.22(SCLK), P9.21(MISO), P9.18(MOSI), P9.17(CS0)
 * Biên dịch: gcc -o spi_adc spi_mcp3204.c
 * Chạy: sudo ./spi_adc
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>

/* === Base addresses (spruh73q.pdf, Table 2-4) === */
#define SPI0_BASE           0x48030000UL
#define CTRL_MODULE_BASE    0x44E10000UL
#define CM_PER_BASE         0x44E00000UL

/* === McSPI register offsets (spruh73q.pdf, Table 24-5) === */
#define MCSPI_SYSCONFIG 0x110
#define MCSPI_SYSSTATUS 0x114
#define MCSPI_MODULCTRL 0x128
#define MCSPI_CH0CONF   0x12C
#define MCSPI_CH0STAT   0x130
#define MCSPI_CH0CTRL   0x134
#define MCSPI_TX0       0x138
#define MCSPI_RX0       0x13C

/* === MCSPI_CHxCONF bits === */
/* Word length 8-bit: WL = 7, bits [25:21] */
#define CHCONF_WL8      (7 << 21)
/* CLKD = 4 → 1.5MHz (< 2MHz max của MCP3204), bits [31:28] */
#define CHCONF_CLKD4    (4 << 28)
/* DPE0=0: d0 = MOSI; DPE1=1: d1 không dùng (MISO) */
#define CHCONF_DPE0     (0 << 17)
#define CHCONF_DPE1     (1 << 18)
/* IS=0: MISO từ SPI0_D0 (P9.21) */
#define CHCONF_IS       (0 << 16)
/* CS active LOW: EPOL=0 */
#define CHCONF_EPOL     (0 << 6)
/* Mode 0: CPOL=0, CPHA=0 */
#define CHCONF_MODE0    (0)

/* === CHxSTAT bits === */
#define STAT_TXS    (1 << 1)   /* TX register đã ghi xong */
#define STAT_RXS    (1 << 0)   /* RX register có dữ liệu */

/* === Clock/Pinmux offsets === */
#define CM_PER_SPI0_CLKCTRL  0x04C  /* spruh73q.pdf, Section 8.1.12.1.12 */
#define CONF_SPI0_SCLK  0x950    /* P9.22 */
#define CONF_SPI0_D0    0x954    /* P9.21 = MISO */
#define CONF_SPI0_D1    0x958    /* P9.18 = MOSI */
#define CONF_SPI0_CS0   0x95C    /* P9.17 */

#define PAGE_SIZE 4096

static volatile uint32_t *spi0;
static volatile uint32_t *ctrl;
static volatile uint32_t *cm_per;

int spi_init(int fd)
{
    spi0  = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, SPI0_BASE);
    ctrl  = mmap(NULL, PAGE_SIZE*16, PROT_READ|PROT_WRITE, MAP_SHARED, fd, CTRL_MODULE_BASE);
    cm_per= mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, CM_PER_BASE);
    if (spi0==MAP_FAILED || ctrl==MAP_FAILED || cm_per==MAP_FAILED) {
        perror("mmap"); return -1;
    }

    /* 1. Bật clock SPI0 */
    cm_per[CM_PER_SPI0_CLKCTRL / 4] = 0x2;
    usleep(10000);

    /* 2. Pinmux: Mode 0 = SPI0 cho tất cả 4 chân */
    ctrl[CONF_SPI0_SCLK / 4] = 0x00;   /* mode 0 */
    ctrl[CONF_SPI0_D0   / 4] = 0x20;   /* mode 0, RXACTIVE=1 (MISO) */
    ctrl[CONF_SPI0_D1   / 4] = 0x00;   /* mode 0 */
    ctrl[CONF_SPI0_CS0  / 4] = 0x00;   /* mode 0 */

    /* 3. Reset module */
    spi0[MCSPI_SYSCONFIG / 4] = 0x02;  /* SOFTRESET */
    while (!(spi0[MCSPI_SYSSTATUS / 4] & 0x01))  /* đợi RESETDONE */
        ;

    /* 4. Master mode, single channel */
    spi0[MCSPI_MODULCTRL / 4] = 0x00;  /* Master, Multiple channels */

    /* 5. Cấu hình channel 0: 8-bit, Mode 0, ~1.5MHz */
    spi0[MCSPI_CH0CONF / 4] = CHCONF_WL8 | CHCONF_CLKD4 |
                               CHCONF_DPE0 | CHCONF_DPE1 | CHCONF_IS |
                               CHCONF_EPOL | CHCONF_MODE0;

    /* 6. Enable channel 0 */
    spi0[MCSPI_CH0CTRL / 4] = 0x01;

    return 0;
}

/* Truyền/nhận 1 byte */
uint8_t spi_transfer(uint8_t byte)
{
    /* Đợi TX register rỗng */
    while (!(spi0[MCSPI_CH0STAT / 4] & STAT_TXS))
        ;
    spi0[MCSPI_TX0 / 4] = byte;

    /* Đợi RX register có dữ liệu */
    while (!(spi0[MCSPI_CH0STAT / 4] & STAT_RXS))
        ;
    return (uint8_t)(spi0[MCSPI_RX0 / 4] & 0xFF);
}

/*
 * Đọc 1 channel ADC từ MCP3204
 * channel: 0..3
 * Trả về: giá trị 12-bit (0..4095)
 *
 * MCP3204 protocol:
 *   Byte 1: 0x06 | (channel >> 2)    [start bit + SGL/DIFF + D2]
 *   Byte 2: (channel & 0x03) << 6    [D1, D0, padding]
 *   Byte 3: 0x00                      [clock out để nhận data]
 *   Result: (byte2 & 0x0F) << 8 | byte3
 */
uint16_t mcp3204_read(uint8_t channel)
{
    uint8_t cmd0 = 0x06 | (channel >> 2);     /* Start + SGL + D2 */
    uint8_t cmd1 = (channel & 0x03) << 6;    /* D1, D0 */

    /* CS manual: FORCE=1 (giữ CS active suốt transaction) */
    spi0[MCSPI_CH0CONF / 4] |= (1 << 27);   /* FORCE = 1 */

    uint8_t b0 = spi_transfer(cmd0);
    uint8_t b1 = spi_transfer(cmd1);
    uint8_t b2 = spi_transfer(0x00);

    /* CS manual: FORCE=0 (release CS) */
    spi0[MCSPI_CH0CONF / 4] &= ~(1 << 27);

    (void)b0; /* byte đầu không chứa data */
    return (uint16_t)((b1 & 0x0F) << 8) | b2;
}

int main(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }

    if (spi_init(fd) != 0) return 1;

    /* Đọc tất cả 4 channel MCP3204 */
    for (int ch = 0; ch < 4; ch++) {
        uint16_t val = mcp3204_read(ch);
        float voltage = val * 3.3f / 4096.0f;  /* Vref = 3.3V */
        printf("CH%d: %4d (%.3f V)\n", ch, val, voltage);
    }

    return 0;
}
```

### Giải thích chi tiết từng dòng code `spi_mcp3204.c`

#### a) Cấu hình channel — CHCONF bits

```c
#define CHCONF_WL8   (7 << 21)   // Word Length = 7+1 = 8 bit
                                  // Bit [25:21] = 0b00111 = 7 → 8-bit transfer
#define CHCONF_CLKD4 (4 << 28)   // Clock Divider = 4 → F_SPI = 48MHz / 2^(4+1) = 1.5MHz
                                  // Bit [31:28] = CLKD
```

```c
#define CHCONF_DPE0  (0 << 17)   // Data Pin 0 (MOSI) enabled for TX
#define CHCONF_DPE1  (1 << 18)   // Data Pin 1 disabled for TX (dùng làm MISO)
#define CHCONF_IS    (0 << 16)   // Input Select: dùng D0 làm input (MISO)
```

#### b) `spi_transfer()` — truyền/nhận 1 byte

```c
while (!(spi0[MCSPI_CH0STAT / 4] & STAT_TXS))  // Đợi TX register rỗng
    ;
spi0[MCSPI_TX0 / 4] = byte;                     // Ghi byte vào TX FIFO

while (!(spi0[MCSPI_CH0STAT / 4] & STAT_RXS))  // Đợi RX có dữ liệu
    ;                                            // (SPI là full-duplex: TX và RX đồng thời)
return (uint8_t)(spi0[MCSPI_RX0 / 4] & 0xFF);  // Đọc byte nhận được
```

> **SPI full-duplex**: Mỗi byte gửi đi đồng thời nhận về 1 byte. Vì vậy luôn phải gửi (dù là 0x00) để nhận dữ liệu.

#### c) `mcp3204_read()` — giao thức MCP3204

```c
uint8_t cmd0 = 0x06 | (channel >> 2);  // Byte 1: Start(1) + SGL/DIFF(1) + D2
// 0x06 = 0b0000_0110 → Start=1, SGL=1 (single-ended), D2 = channel bit 2

uint8_t cmd1 = (channel & 0x03) << 6;  // Byte 2: D1, D0 (2 bit thấp của channel)
// Ví dụ channel=3: cmd0=0x07, cmd1=0xC0
```

```c
spi0[MCSPI_CH0CONF / 4] |= (1 << 27);   // FORCE=1 → giữ CS LOW suốt 3 byte
// Nếu không FORCE, CS sẽ nâng lên giữa các byte → MCP3204 mất dữ liệu

uint8_t b0 = spi_transfer(cmd0);  // Gửi lệnh, nhận garbage
uint8_t b1 = spi_transfer(cmd1);  // Gửi D1/D0, nhận 4 bit cao của ADC
uint8_t b2 = spi_transfer(0x00);  // Gửi dummy, nhận 8 bit thấp của ADC

spi0[MCSPI_CH0CONF / 4] &= ~(1 << 27);  // FORCE=0 → CS lên HIGH (kết thúc)

return (uint16_t)((b1 & 0x0F) << 8) | b2;  // Ghép 12-bit: b1[3:0] + b2[7:0]
```

#### d) Tính điện áp

```c
float voltage = val * 3.3f / 4096.0f;  // Vref = 3.3V, 12-bit → 4096 mức
// Độ phân giải = 3.3V / 4096 ≈ 0.8mV/LSB
```

---

## 7. SPI từ Linux Userspace (`/dev/spidev0.0`)

```c
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    int fd = open("/dev/spidev0.0", O_RDWR);
    if (fd < 0) { perror("open spidev"); return 1; }

    /* Cấu hình SPI */
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = 1500000;  /* 1.5 MHz */

    ioctl(fd, SPI_IOC_WR_MODE, &mode);
    ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    /* Full-duplex transfer dùng struct spi_ioc_transfer */
    uint8_t tx[3] = {0x06, 0x00, 0x00};  /* channel 0 */
    uint8_t rx[3] = {0};
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = 3,
        .speed_hz = speed,
        .bits_per_word = bits,
    };

    ioctl(fd, SPI_IOC_MESSAGE(1), &tr);

    uint16_t val = ((rx[1] & 0x0F) << 8) | rx[2];
    printf("ADC CH0 = %d\n", val);

    close(fd);
    return 0;
}
```

**Bật spidev trên BBB**:
```bash
# Cần cấu hình device tree overlay
config-pin P9.17 spi_cs
config-pin P9.18 spi
config-pin P9.21 spi
config-pin P9.22 spi_sclk
ls /dev/spidev*
```

---

## 8. Câu hỏi kiểm tra

1. SPI khác I2C ở điểm gì về phương thức kết nối và số dây?
2. Mode SPI nào có CPOL=1 và CPHA=0? Clock idle ở trạng thái nào?
3. CLKD=3 tương ứng với SPI clock bao nhiêu MHz (F_module = 48MHz)?
4. Bit `FORCE` trong `MCSPI_CHxCONF` có tác dụng gì?
5. Tại sao cần đọc `MCSPI_CH0STAT` trước khi đọc `MCSPI_RX0`?

---

## 9. Tài liệu tham khảo

| Nội dung | Tài liệu | Section |
|----------|----------|---------|
| McSPI module overview | spruh73q.pdf | Chapter 24, Section 24.1 |
| McSPI register map | spruh73q.pdf | Table 24-5 |
| SPI clock generation | spruh73q.pdf | Section 24.3.7 |
| McSPI base addresses | spruh73q.pdf | Table 2-4 |
| CM_PER_SPI0_CLKCTRL | spruh73q.pdf | Section 8.1.12.1.12 |
| SPI0 pins P9.17..P9.22 | BeagleboneBlackP9HeaderTable.pdf | — |
