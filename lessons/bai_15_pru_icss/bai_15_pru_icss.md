# Bài 15 - PRU-ICSS: Lập Trình Real-Time với PRU

## 1. Mục tiêu bài học

- Hiểu kiến trúc PRU-ICSS (Programmable Real-time Unit Industrial Communication SubSystem)
- Biết PRU có thể làm gì mà ARM CPU không làm tốt
- Nắm cấu trúc bộ nhớ PRU, tập lệnh và I/O
- Viết chương trình PRU đơn giản bằng C (TI PRU C compiler)
- Giao tiếp giữa PRU và Linux userspace qua RemoteProc + RPMsg

---

## 2. PRU-ICSS là gì?

**PRU-ICSS** (Programmable Real-time Unit & Industrial Communication Subsystem) là một subsystem độc lập bên trong AM335x, bao gồm:

- **2 PRU cores** (PRU0 và PRU1): bộ xử lý RISC 32-bit, 200MHz
- **Instruction RAM**: 8KB mỗi PRU (IRAM)
- **Data RAM**: 8KB mỗi PRU (DRAM)
- **Shared RAM**: 12KB dùng chung 2 PRU
- **Enhanced GPIO**: 16 input + 16 output trực tiếp per PRU (không qua ARM)

> **Nguồn**: spruh73q.pdf, Chapter 4 (PRU-ICSS)

### 2.1 Tại sao dùng PRU?

| Tình huống | ARM + Linux | PRU |
|-----------|-------------|-----|
| Toggle GPIO mỗi 1µs chính xác | ❌ Không ổn (OS jitter) | ✅ Cycle-accurate (5ns/cycle) |
| Giao tiếp protocol custom (1-wire, WS2812) | ❌ Khó | ✅ Bit-bang chính xác |
| Real-time motor control | ❌ Latency cao | ✅ Deterministic |
| Industrial Ethernet (EtherCAT) | ❌ Không đủ nhanh | ✅ Được thiết kế cho điều này |

**Điểm mạnh PRU**: mỗi instruction = 1 cycle = **5ns** ở 200MHz. Không có cache miss, không có OS scheduler.

---

## 3. Kiến trúc Bộ Nhớ PRU

```
PRU-ICSS Memory Map (spruh73q.pdf, Table 4-1):
  PRU0 IRAM:     0x4A334000 (8KB)
  PRU0 DRAM:     0x4A300000 (8KB)
  PRU1 IRAM:     0x4A338000 (8KB)
  PRU1 DRAM:     0x4A302000 (8KB)
  Shared DRAM:   0x4A310000 (12KB)
  PRU-ICSS CFG:  0x4A326000
  PRU-ICSS IEP:  0x4A32E000 (Industrial Ethernet Peripheral)
  PRU-ICSS MII:  0x4A332000 (Media Independent Interface)
```

> **Nguồn**: spruh73q.pdf, Table 4-1 (PRU-ICSS Register Map)

---

## 4. Tập Lệnh PRU (Instruction Set)

PRU có tập lệnh RISC đơn giản:

| Nhóm | Ví dụ | Chú thích |
|------|-------|----------|
| Data move | `MOV R0, 0x1234` | Load immediate |
| Load/Store | `LBBO &R0, R5, 0, 4` | Load 4 byte từ địa chỉ R5+0 |
| Arithmetic | `ADD R0, R0, R1` | Cộng R0 + R1 |
| Logic | `AND R0, R0, 0xFF` | AND với mask |
| Branch | `QBA LABEL` | Jump |
| I/O trực tiếp | `CLR R30, R30, 15` | Clear bit 15 của R30 (output GPIO) |
| Delay cycle | `NOP` / vòng lặp | Tạo delay chính xác |

**R30** = PRU output register (ghi → GPIO output thay đổi ngay)  
**R31** = PRU input register (đọc → giá trị GPIO input hiện tại)

---

## 5. Lập Trình PRU bằng C

TI cung cấp **PRU C compiler** (pru-cgt) trong TI SDK. Trên BBB, có thể dùng `clpru`.

### 5.1 Ví dụ C: Blink LED qua PRU

```c
/*
 * pru_blink.c
 * Blink GPIO qua PRU0 — chạy trên PRU, không phải ARM
 *
 * Compile: clpru -v3 -O2 --include_path=/usr/lib/ti/pru-software-support-package/include
 *          pru_blink.c -o pru_blink.out
 *
 * Deploy: xem Section 6 (RemoteProc)
 */

#include <stdint.h>
#include <pru_cfg.h>
#include <pru_ctrl.h>

/* Register R30 = PRU output register */
volatile register uint32_t __R30;
/* Register R31 = PRU input register */
volatile register uint32_t __R31;

/* PRU0 output bit 0 = pr1_pru0_pru_r30_0 
 * Xem BBB device tree để biết mapping ra header nào */
#define PRU_OUTPUT_BIT  0

/* Delay loop: 200MHz → 1 cycle = 5ns
 * 200,000 cycles = 1ms */
static void delay_ms(uint32_t ms)
{
    volatile uint32_t i;
    while (ms--) {
        for (i = 0; i < 200000; i++)
            ; /* mỗi vòng ≈ 1 cycle nếu tối ưu */
    }
}

void main(void)
{
    /* Disable STANDBY, bật OCP master port */
    CT_CFG.SYSCFG_bit.STANDBY_INIT = 0;

    while (1) {
        /* Bật output bit */
        __R30 |= (1U << PRU_OUTPUT_BIT);
        delay_ms(500);

        /* Tắt output bit */
        __R30 &= ~(1U << PRU_OUTPUT_BIT);
        delay_ms(500);
    }
}
```

---

## 6. RemoteProc và RPMsg: Giao Tiếp PRU ↔ Linux

Linux kernel quản lý PRU qua **RemoteProc** framework:

```
Linux userspace
      │
  /dev/rpmsg0         ← giao tiếp hai chiều
      │
  RPMsg (kernel)
      │
  remoteproc
      │
  PRU core            ← chạy firmware .out
```

### 6.1 Nạp firmware PRU từ Linux

```bash
# Firmware đặt tại /lib/firmware/
cp pru_blink.out /lib/firmware/am335x-pru0-fw

# Stop PRU
echo stop > /sys/bus/platform/drivers/pru-rproc/4a334000.pru/state
# Nạp firmware mới
echo start > /sys/bus/platform/drivers/pru-rproc/4a334000.pru/state

# Hoặc dùng sysfs
ls /sys/class/remoteproc/
echo am335x-pru0-fw > /sys/class/remoteproc/remoteproc1/firmware
echo start > /sys/class/remoteproc/remoteproc1/state
```

### 6.2 Giao tiếp qua RPMsg (userspace ↔ PRU)

**PRU side** (firmware):
```c
#include <rsc_types.h>
#include <pru_rpmsg.h>

/* Gửi dữ liệu đến Linux */
pru_rpmsg_send(&transport, dst, src, payload, len);

/* Nhận dữ liệu từ Linux */
pru_rpmsg_receive(&transport, &src, &dst, payload, &len);
```

**Linux userspace**:
```c
int fd = open("/dev/rpmsg0", O_RDWR);
write(fd, "hello_pru", 9);     /* gửi lên PRU */
read(fd, buf, sizeof(buf));    /* nhận từ PRU */
```

---

## 7. Ứng Dụng Thực Tế PRU

### 7.1 WS2812 LED Strip (NeoPixel)

WS2812 dùng giao thức 1-wire với timing chặt: bit 0 = 0.4µs HIGH + 0.85µs LOW, bit 1 = 0.8µs HIGH + 0.45µs LOW.

Không thể làm ổn định bằng ARM + Linux. PRU làm được hoàn hảo:

```c
/* PRU: gửi 1 bit cho WS2812 */
static inline void send_bit(uint8_t bit)
{
    __R30 |= PIN;       /* HIGH */
    if (bit) {
        __delay_cycles(160);  /* 0.8µs */
        __R30 &= ~PIN;
        __delay_cycles(90);   /* 0.45µs */
    } else {
        __delay_cycles(80);   /* 0.4µs */
        __R30 &= ~PIN;
        __delay_cycles(170);  /* 0.85µs */
    }
}
```

### 7.2 Stepper Motor Control

Điều khiển bước động cơ với timing chính xác từng µs.

### 7.3 Đọc Encoder Tốc Độ Cao

Đọc encoder 10kHz mà không bỏ sót xung, gửi kết quả về ARM qua shared memory.

---

## 8. Câu hỏi kiểm tra

1. PRU chạy ở tần số bao nhiêu? Mỗi instruction mất bao nhiêu ns?
2. Register R30 và R31 có ý nghĩa đặc biệt gì trong PRU?
3. Tại sao dùng PRU để điều khiển WS2812 tốt hơn là dùng ARM + Linux?
4. RemoteProc framework dùng để làm gì?
5. Shared DRAM của PRU-ICSS có kích thước bao nhiêu và nằm ở địa chỉ nào?

---

## 9. Tài liệu tham khảo

| Nội dung | Tài liệu | Section |
|----------|----------|---------|
| PRU-ICSS overview | spruh73q.pdf | Chapter 4, Section 4.1 |
| PRU memory map | spruh73q.pdf | Table 4-1 |
| PRU instruction set | spruh73q.pdf | Section 4.5 |
| PRU C compiler guide | TI SPRUHV7 (TI website) | — |
| RemoteProc/RPMsg | Linux kernel docs | Documentation/staging/remoteproc.txt |
