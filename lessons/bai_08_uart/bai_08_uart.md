# Bài 8 - UART: Thanh Ghi, Baud Rate và Giao Tiếp Serial

## 1. Mục tiêu bài học

- Hiểu giao thức UART/serial: frame format, baud rate, parity
- Nắm vững các thanh ghi UART trên AM335x
- Tính toán và cấu hình baud rate từ clock source
- Viết code C giao tiếp UART bằng register-level (mmap)
- Sử dụng UART từ Linux userspace qua `/dev/ttyO*`

---

## 2. Giao thức UART cơ bản

**UART** (Universal Asynchronous Receiver/Transmitter) là giao tiếp serial không đồng bộ.

### 2.1 Frame UART

```
Idle  Start  D0  D1  D2  D3  D4  D5  D6  D7  Parity  Stop
─────┐                                              ┌────
     │ 1 bit│ 8 bit data                    │1bit │1-2bit
     └──────┴──┴──┴──┴──┴──┴──┴──┴──┴──────┴─────┘
```

- **Start bit**: 1 bit LOW báo hiệu bắt đầu
- **Data bits**: 5, 6, 7 hoặc 8 bit
- **Parity bit** (tùy chọn): kiểm tra lỗi (even/odd/none)
- **Stop bit**: 1 hoặc 2 bit HIGH kết thúc frame

### 2.2 Baud Rate

Baud rate = số bit truyền/nhận trong 1 giây (bps).

Phổ biến: 9600, 19200, 38400, 57600, **115200**, 230400 bps.

Ở 115200 baud: mỗi bit kéo dài $\frac{1}{115200} \approx 8.68\mu s$

---

## 3. UART trên AM335x

AM335x có **6 module UART**:

| Module | Base Address | Linux device | Thường dùng cho |
|--------|-------------|--------------|-----------------|
| UART0  | `0x44E09000` | `/dev/ttyO0` | Console Linux (debug) |
| UART1  | `0x48022000` | `/dev/ttyO1` | P9.24/P9.26 |
| UART2  | `0x48024000` | `/dev/ttyO2` | P9.21/P9.22 |
| UART3  | `0x481A6000` | `/dev/ttyO3` | Internal use |
| UART4  | `0x481A8000` | `/dev/ttyO4` | P9.11/P9.13 |
| UART5  | `0x481AA000` | `/dev/ttyO5` | P8.37/P8.38 |

> **Nguồn**: spruh73q.pdf, Table 2-3, 2-4; BBB_SRM_C.pdf, Section 7

---

## 4. Thanh ghi UART quan trọng

UART trên AM335x dùng chuẩn **16550-compatible**. Các thanh ghi có hai mode: Normal và Config (tùy theo bit LCR).

### 4.1 Thanh ghi thường dùng

| Thanh ghi | Offset | Chức năng |
|-----------|--------|-----------|
| `UART_RHR` | `0x000` | Receive Holding Register (đọc dữ liệu vào) |
| `UART_THR` | `0x000` | Transmit Holding Register (ghi dữ liệu ra) — cùng offset RHR, phân biệt qua LCR |
| `UART_IER` | `0x004` | Interrupt Enable Register |
| `UART_IIR` | `0x008` | Interrupt Identification Register (đọc) |
| `UART_FCR` | `0x008` | FIFO Control Register (ghi) |
| `UART_LCR` | `0x00C` | Line Control Register (frame format, baud divisor mode) |
| `UART_MCR` | `0x010` | Modem Control Register (RTS, DTR, loopback) |
| `UART_LSR` | `0x014` | Line Status Register (tx empty, rx ready, error flags) |
| `UART_MDR1`| `0x020` | Mode Definition Register 1 (16x, 13x oversample) |
| `UART_DLL` | `0x000` | Divisor Latch Low (khi LCR[7]=1 = DLAB mode) |
| `UART_DLH` | `0x004` | Divisor Latch High (khi LCR[7]=1 = DLAB mode) |

> **Nguồn**: spruh73q.pdf, Table 19-66 (UART Register Map)

### 4.2 Thanh ghi LSR quan trọng

```
Bit 7: RXFIFOE — có lỗi trong RX FIFO
Bit 6: TXSRE   — TX shift register rỗng (transmit hoàn tất)
Bit 5: TXFIFOE — TX FIFO rỗng (sẵn sàng ghi tiếp)
Bit 1: OVERRUN — overflow error
Bit 0: RXFIFOE — có dữ liệu trong RX FIFO (ready to read)
```

---

## 5. Tính toán Baud Rate Divisor

AM335x UART dùng **clock 48MHz** từ DPLL_PER.

Công thức:
$$\text{Divisor} = \frac{F_{clk}}{16 \times \text{Baud Rate}}$$

Ví dụ với 115200 baud:
$$\text{Divisor} = \frac{48{,}000{,}000}{16 \times 115200} = \frac{48{,}000{,}000}{1{,}843{,}200} \approx 26$$

Divisor 16-bit: `DLL = 26`, `DLH = 0`.

| Baud Rate | Divisor | DLL | DLH | Lỗi (%) |
|-----------|---------|-----|-----|---------|
| 9600      | 312     | 0x38| 0x01| 0.0%    |
| 57600     | 52      | 0x34| 0x00| 0.0%    |
| 115200    | 26      | 0x1A| 0x00| 0.0%    |
| 230400    | 13      | 0x0D| 0x00| 0.0%    |

> **Nguồn**: spruh73q.pdf, Section 19.3.7 (Baud Rate Generation)

---

## 6. Trình tự khởi tạo UART

```
1. Bật clock UART module (CM_PER/CM_WKUP)
2. Cấu hình pinmux (conf_xxx = Mode 0 cho UART)
3. Disable UART (UART_MDR1[2:0] = 0x7)
4. Set DLAB mode (LCR[7] = 1) để ghi DLL/DLH
5. Ghi DLL, DLH (baud rate divisor)
6. Clear DLAB (LCR[7] = 0)
7. Set frame format trong LCR (data bits, stop, parity)
8. Enable/reset FIFO qua FCR
9. Enable UART (UART_MDR1[2:0] = 0x0 = 16x mode)
```

---

## 7. Code C - UART Register Level (mmap)

```c
/*
 * uart_init.c
 * Khởi tạo UART1 bằng mmap /dev/mem, 115200-8N1
 *
 * UART1: P9.24 (TX) = uart1_txd, P9.26 (RX) = uart1_rxd
 * Biên dịch: gcc -o uart_init uart_init.c
 * Chạy: sudo ./uart_init
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>

/* === Base addresses (spruh73q.pdf, Table 2-4) === */
#define UART1_BASE          0x48022000UL
#define CTRL_MODULE_BASE    0x44E10000UL
#define CM_PER_BASE         0x44E00000UL

/* === UART register offsets (spruh73q.pdf, Table 19-66) === */
#define UART_RHR    0x00
#define UART_THR    0x00
#define UART_IER    0x04
#define UART_FCR    0x08
#define UART_LCR    0x0C
#define UART_MCR    0x10
#define UART_LSR    0x14
#define UART_MDR1   0x20
#define UART_DLL    0x00   /* khi LCR[7]=1 */
#define UART_DLH    0x04   /* khi LCR[7]=1 */

/* === Clock offset (spruh73q.pdf, Section 8.1.12.1.24) === */
#define CM_PER_UART1_CLKCTRL    0x06C

/* === Pinmux offsets cho UART1 (spruh73q.pdf, Table 9-7) === */
#define CONF_UART1_RXD  0x970   /* P9.26 */
#define CONF_UART1_TXD  0x974   /* P9.24 */

/* === LCR bit-field === */
#define LCR_8BIT        0x03    /* 8 data bits */
#define LCR_1STOP       0x00    /* 1 stop bit */
#define LCR_NOPARITY    0x00    /* no parity */
#define LCR_DLAB        0x80    /* Divisor Latch Access Bit */

/* === LSR bit-field === */
#define LSR_TX_EMPTY    (1 << 5)  /* TX FIFO rỗng */
#define LSR_RX_READY    (1 << 0)  /* RX có dữ liệu */

/* === MDR1 mode === */
#define MDR1_16X_MODE   0x00    /* 16x oversample (UART mode) */
#define MDR1_DISABLE    0x07    /* disable UART */

/* === Baud rate divisor cho 115200 (48MHz / 16 / 115200 = 26) === */
#define BAUD_DLL    26
#define BAUD_DLH    0

#define PAGE_SIZE 4096

static volatile uint32_t *uart1;
static volatile uint32_t *ctrl;
static volatile uint32_t *cm_per;

int uart_init(int fd)
{
    /* Ánh xạ bộ nhớ */
    uart1 = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, UART1_BASE);
    ctrl  = mmap(NULL, PAGE_SIZE*16, PROT_READ|PROT_WRITE, MAP_SHARED, fd, CTRL_MODULE_BASE);
    cm_per= mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, CM_PER_BASE);
    if (uart1==MAP_FAILED || ctrl==MAP_FAILED || cm_per==MAP_FAILED) {
        perror("mmap"); return -1;
    }

    /* 1. Bật clock UART1 */
    cm_per[CM_PER_UART1_CLKCTRL / 4] = 0x2;

    /* 2. Cấu hình pinmux: Mode 0 = UART, input enable cho RX */
    ctrl[CONF_UART1_TXD / 4] = 0x00;           /* Mode 0, output */
    ctrl[CONF_UART1_RXD / 4] = 0x20;           /* Mode 0, RXACTIVE=1 */

    /* 3. Disable UART trước khi cấu hình */
    uart1[UART_MDR1 / 4] = MDR1_DISABLE;

    /* 4. Set DLAB mode (LCR[7]=1) để access DLL/DLH */
    uart1[UART_LCR / 4] = LCR_DLAB;

    /* 5. Ghi divisor */
    uart1[UART_DLL / 4] = BAUD_DLL;   /* = 26 = 0x1A */
    uart1[UART_DLH / 4] = BAUD_DLH;

    /* 6. Clear DLAB, set 8N1 */
    uart1[UART_LCR / 4] = LCR_8BIT | LCR_1STOP | LCR_NOPARITY;

    /* 7. Enable và reset FIFO (FCR) */
    uart1[UART_FCR / 4] = 0x07;   /* bit0=FIFO enable, bit1=RX reset, bit2=TX reset */

    /* 8. Enable UART ở 16x mode */
    uart1[UART_MDR1 / 4] = MDR1_16X_MODE;

    return 0;
}

/* Gửi 1 byte, đợi TX FIFO rỗng trước khi ghi */
void uart_putc(char c)
{
    while (!(uart1[UART_LSR / 4] & LSR_TX_EMPTY))
        ;   /* busy-wait */
    uart1[UART_THR / 4] = c;
}

/* Gửi chuỗi */
void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

/* Đọc 1 byte, đợi có dữ liệu */
char uart_getc(void)
{
    while (!(uart1[UART_LSR / 4] & LSR_RX_READY))
        ;   /* busy-wait */
    return (char)(uart1[UART_RHR / 4] & 0xFF);
}

int main(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }

    if (uart_init(fd) != 0) return 1;

    uart_puts("Hello from UART1! 115200-8N1\r\n");

    /* Echo loop */
    printf("UART1 sẵn sàng. Gõ ký tự để echo.\n");
    while (1) {
        char c = uart_getc();
        uart_putc(c);   /* echo lại ký tự nhận được */
    }

    return 0;
}
```

### Giải thích chi tiết từng dòng code

#### a) Các thanh ghi UART — tại sao cùng offset?

```c
#define UART_RHR  0x00   // Receive Holding Register — đọc byte nhận được
#define UART_THR  0x00   // Transmit Holding Register — ghi byte cần gửi
// Cùng offset 0x00 nhưng: đọc → lấy RHR, ghi → ghi THR
// Đây là thiết kế phổ biến trên UART 16550-compatible

#define UART_DLL  0x00   // Divisor Latch Low — 8 bit thấp của baud rate divisor
#define UART_DLH  0x04   // Divisor Latch High — 8 bit cao
// Cũng dùng offset 0x00 và 0x04, nhưng chỉ truy cập được khi LCR[7]=1 (DLAB mode)
```

#### b) Cấu hình baud rate — công thức

$$\text{Divisor} = \frac{\text{UART\_CLK}}{16 \times \text{BaudRate}} = \frac{48{,}000{,}000}{16 \times 115200} = 26$$

```c
#define BAUD_DLL  26    // = 0x1A — byte thấp của divisor
#define BAUD_DLH  0     // = 0x00 — byte cao (divisor < 256 nên DLH = 0)
```

#### c) Hàm `uart_init()` — trình tự khởi tạo

```c
cm_per[CM_PER_UART1_CLKCTRL / 4] = 0x2;  // MODULEMODE = ENABLE → bật clock UART1
```

```c
ctrl[CONF_UART1_TXD / 4] = 0x00;  // Mode 0 = chức năng UART TX
                                   // Bit [2:0]=0 → Mode 0, Bit 5=0 → output
ctrl[CONF_UART1_RXD / 4] = 0x20;  // Mode 0 + RXACTIVE=1 (bit 5)
                                   // RXACTIVE bật receiver — bắt buộc cho chân RX
```

```c
uart1[UART_MDR1 / 4] = MDR1_DISABLE;  // = 0x07 → disable UART trước khi cấu hình
// BẮT BUỘC: nếu thay đổi baud rate khi UART đang chạy → dữ liệu bị hỏng
```

```c
uart1[UART_LCR / 4] = LCR_DLAB;       // = 0x80 → set bit 7 = 1 (DLAB mode)
// Khi DLAB=1: offset 0x00 trở thành DLL, offset 0x04 trở thành DLH
// Khi DLAB=0: offset 0x00 trở lại là RHR/THR bình thường

uart1[UART_DLL / 4] = BAUD_DLL;        // = 26 → ghi divisor byte thấp
uart1[UART_DLH / 4] = BAUD_DLH;        // = 0  → ghi divisor byte cao
```

```c
uart1[UART_LCR / 4] = LCR_8BIT | LCR_1STOP | LCR_NOPARITY;
// = 0x03 | 0x00 | 0x00 = 0x03
// Bit [1:0] = 0b11 → 8 data bits
// Bit 2 = 0 → 1 stop bit
// Bit 3 = 0 → parity disabled
// Bit 7 = 0 → clear DLAB, quay về chế độ RHR/THR bình thường
```

```c
uart1[UART_FCR / 4] = 0x07;
// Bit 0 = 1 → FIFO enable
// Bit 1 = 1 → Reset RX FIFO (tự clear sau khi reset)
// Bit 2 = 1 → Reset TX FIFO (tự clear sau khi reset)
```

```c
uart1[UART_MDR1 / 4] = MDR1_16X_MODE;  // = 0x00 → bật UART ở chế độ 16x oversampling
// 16x oversampling: mỗi bit được sample 16 lần → chính xác hơn
```

#### d) Hàm `uart_putc()` — gửi 1 byte

```c
while (!(uart1[UART_LSR / 4] & LSR_TX_EMPTY))
    ;  // Busy-wait: đọc LSR liên tục cho đến khi bit TX_EMPTY = 1
       // LSR_TX_EMPTY = (1 << 5) = 0x20
       // Khi bit 5 = 1: TX FIFO rỗng → an toàn để ghi byte mới

uart1[UART_THR / 4] = c;  // Ghi byte vào THR → UART hardware tự gửi ra chân TX
```

#### e) Hàm `uart_getc()` — nhận 1 byte

```c
while (!(uart1[UART_LSR / 4] & LSR_RX_READY))
    ;  // Busy-wait: chờ đến khi RX FIFO có dữ liệu
       // LSR_RX_READY = (1 << 0) = 0x01

return (char)(uart1[UART_RHR / 4] & 0xFF);  // Đọc byte từ RHR
// & 0xFF: chỉ lấy 8 bit thấp (thanh ghi 32-bit nhưng UART data chỉ 8 bit)
```

> **So sánh register-level vs termios**: Code mmap trên cho thấy chính xác từng bước phần cứng làm gì. Trong production, nên dùng `/dev/ttyO1` + `termios` vì kernel lo FIFO, interrupt, DMA, error handling — an toàn và portable hơn nhiều.

---

## 8. UART qua Linux devfile (`/dev/ttyO1`)

Cách Linux userspace thông thường — không cần mmap, dùng `termios`:

```c
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

int main(void)
{
    int fd = open("/dev/ttyO1", O_RDWR | O_NOCTTY);
    if (fd < 0) { perror("open"); return 1; }

    struct termios tty;
    tcgetattr(fd, &tty);

    /* Cấu hình 115200 baud, 8N1 */
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    tty.c_cflag = CS8 | CREAD | CLOCAL;    /* 8 bit, read, no modem */
    tty.c_iflag = 0;
    tty.c_oflag = 0;
    tty.c_lflag = 0;    /* raw mode */

    tcsetattr(fd, TCSANOW, &tty);

    write(fd, "Hello!\r\n", 8);

    char buf[64];
    int n = read(fd, buf, sizeof(buf));
    buf[n] = 0;
    printf("Nhận: %s\n", buf);

    close(fd);
    return 0;
}
```

---

## 9. Câu hỏi kiểm tra

1. Baud rate divisor cho 9600 baud (clock 48MHz) là bao nhiêu? DLL và DLH là gì?
2. Tại sao phải set `LCR[7] = 1` trước khi ghi DLL/DLH?
3. Trong LSR, bit nào báo TX FIFO rỗng và có thể ghi byte tiếp theo?
4. `UART_MDR1 = 0x07` có nghĩa là gì? Tại sao phải set trước khi cấu hình?
5. So sánh cách dùng mmap và `/dev/ttyO1 + termios`: cách nào nên dùng trong production?

---

## 10. Tài liệu tham khảo

| Nội dung | Tài liệu | Section |
|----------|----------|---------|
| UART module overview | spruh73q.pdf | Section 19.1 (Chapter 19) |
| UART register map | spruh73q.pdf | Table 19-66 |
| Baud rate generation | spruh73q.pdf | Section 19.3.7 |
| UART base addresses | spruh73q.pdf | Table 2-4 |
| CM_PER_UART1_CLKCTRL | spruh73q.pdf | Section 8.1.12.1.24 |
| UART pins P9.24/P9.26 | BeagleboneBlackP9HeaderTable.pdf | — |
