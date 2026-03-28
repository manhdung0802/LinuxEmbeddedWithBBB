# Bài 11 - Timer/PWM: DMTIMER, eCAP, eHRPWM

## 1. Mục tiêu bài học

- Hiểu cơ chế timer trên AM335x: DMTIMER, eCAP, eHRPWM
- Cấu hình DMTIMER làm bộ đếm thời gian, autoreload interrupt
- Tạo tín hiệu PWM bằng eHRPWM để điều khiển độ sáng LED / tốc độ motor
- Đo tần số tín hiệu ngoài bằng eCAP (Enhanced Capture)
- So sánh ba khối timer và khi nào dùng cái nào

---

## 2. Tổng quan Timer trên AM335x

AM335x có nhiều loại timer khác nhau:

| Module | Số lượng | Mục đích chính |
|--------|----------|----------------|
| DMTIMER | 8 (0..7) | Timer đa năng: delay, autoreload, compare, capture |
| eHRPWM | 3 (0..2) | PWM độ phân giải cao: motor, LED dimming |
| eCAP   | 3 (0..2) | Capture tín hiệu: đo tần số, duty cycle |
| eQEP   | 3 (0..2) | Encoder quadrature: đọc encoder motor |

> **Nguồn**: spruh73q.pdf, Chapter 20 (Industrial Interface), Chapter 15 (DMTIMER)

---

## 3. DMTIMER

### 3.1 Base Address và Clock

| Module | Base Address | Clock enable |
|--------|-------------|--------------|
| DMTIMER0 | `0x44E05000` | CM_WKUP_TIMER0_CLKCTRL `0x44E00410` |
| DMTIMER1_1MS | `0x44E31000` | CM_WKUP_TIMER1_CLKCTRL `0x44E00414` |
| DMTIMER2 | `0x48040000` | CM_PER_TIMER2_CLKCTRL `0x44E00080` |
| DMTIMER3 | `0x48042000` | CM_PER_TIMER3_CLKCTRL `0x44E00084` |
| DMTIMER4 | `0x48044000` | CM_PER_TIMER4_CLKCTRL `0x44E00088` |
| DMTIMER5 | `0x48046000` | CM_PER_TIMER5_CLKCTRL `0x44E000EC` |
| DMTIMER6 | `0x48048000` | CM_PER_TIMER6_CLKCTRL `0x44E000F0` |
| DMTIMER7 | `0x4804A000` | CM_PER_TIMER7_CLKCTRL `0x44E0007C` |

> **Nguồn**: spruh73q.pdf, Table 2-3, 2-4

### 3.2 Thanh ghi DMTIMER

| Thanh ghi | Offset | Chức năng |
|-----------|--------|-----------|
| `DMTIMER_TIDR`     | `0x000` | Revision |
| `DMTIMER_TIOCP_CFG`| `0x010` | Idle mode, wakeup config |
| `DMTIMER_IRQSTATUS`| `0x028` | Interrupt status (OVF, MATCH, CAPTURE) |
| `DMTIMER_IRQENABLE_SET`| `0x02C` | Bật interrupt |
| `DMTIMER_IRQENABLE_CLR`| `0x030` | Tắt interrupt |
| `DMTIMER_TCLR`     | `0x038` | Timer Control: start/stop, autoreload, compare |
| `DMTIMER_TCRR`     | `0x03C` | Timer Counter Register (giá trị đếm hiện tại) |
| `DMTIMER_TLDR`     | `0x040` | Timer Load Register (giá trị nạp lại khi overflow) |
| `DMTIMER_TTGR`     | `0x044` | Trigger reload ngay lập tức |
| `DMTIMER_TWPS`     | `0x048` | Posted write status |
| `DMTIMER_TMAR`     | `0x04C` | Timer Match Register (compare value) |

> **Nguồn**: spruh73q.pdf, Table 20-14 (DMTIMER Register Map — dưới Chapter 20)

### 3.3 TCLR - Timer Control Register

```
Bit 14..12: PT      — Pulse/Toggle mode
Bit 11:     CAPT_MODE — Capture mode
Bit  7:     GPO_CFG — GPIO output control
Bit  6:     CE      — Compare Enable (1 = bật so sánh TCRR vs TMAR)
Bit  5:     PRE     — Prescaler enable
Bit  4..2:  PTV     — Prescaler Timer Value (chia clock 2^(PTV+1))
Bit  1:     AR      — Autoreload (1 = khi overflow, nạp lại từ TLDR)
Bit  0:     ST      — Start (1 = chạy timer)
```

### 3.4 Tính toán thời gian

DMTIMER đếm lên từ `TLDR` đến `0xFFFFFFFF` rồi overflow.

$$T_{overflow} = \frac{(2^{32} - \text{TLDR})}{F_{timer}}$$

Với clock 24MHz (DMTIMER2 default):

```c
/* Muốn overflow mỗi 1ms = 0.001s */
/* Số tick cần đếm = 24,000,000 × 0.001 = 24,000 */
/* TLDR = 0xFFFFFFFF - 24000 + 1 = 0xFFFFA1C1 */
#define TIMER_1MS_LOAD  (0xFFFFFFFFUL - 24000UL + 1UL)
```

---

## 4. Code C - DMTIMER2 Delay 1ms

```c
/*
 * dmtimer_delay.c
 * Dùng DMTIMER2 để tạo delay chính xác bằng mmap /dev/mem
 *
 * Biên dịch: gcc -o timer_delay dmtimer_delay.c
 * Chạy: sudo ./timer_delay
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>

/* === Base addresses (spruh73q.pdf, Table 2-4) === */
#define DMTIMER2_BASE       0x48040000UL
#define CM_PER_BASE         0x44E00000UL

/* === DMTIMER offsets (spruh73q.pdf, Table 20-14) === */
#define TIOCP_CFG   0x010
#define IRQSTATUS   0x028
#define IRQENABLE_SET 0x02C
#define TCLR        0x038
#define TCRR        0x03C
#define TLDR        0x040

/* === Clock (spruh73q.pdf, Section 8.1.12.1.19) === */
#define CM_PER_TIMER2_CLKCTRL  0x080

/* === IRQSTATUS bits === */
#define IRQ_OVF (1 << 1)   /* Overflow interrupt */

/* Timer clock = 24MHz (DMTIMER2 default clock source) */
#define TIMER_HZ    24000000UL

/* Load value cho overflow mỗi 1ms */
#define LOAD_1MS    (0xFFFFFFFFUL - (TIMER_HZ / 1000) + 1UL)

#define PAGE_SIZE 4096

static volatile uint32_t *timer2;
static volatile uint32_t *cm_per;

int timer_init(int fd)
{
    timer2 = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, DMTIMER2_BASE);
    cm_per = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, CM_PER_BASE);
    if (timer2 == MAP_FAILED || cm_per == MAP_FAILED) {
        perror("mmap"); return -1;
    }

    /* Bật clock DMTIMER2 */
    cm_per[CM_PER_TIMER2_CLKCTRL / 4] = 0x2;
    usleep(1000);

    /* Dừng timer trước khi cấu hình */
    timer2[TCLR / 4] = 0;

    /* Soft reset */
    timer2[TIOCP_CFG / 4] = 0x01;
    while (timer2[TIOCP_CFG / 4] & 0x01)  /* đợi reset xong */
        ;

    return 0;
}

/* Delay N milli-giây bằng polling OVF flag */
void delay_ms(uint32_t ms)
{
    while (ms--) {
        /* Nạp giá trị, bật autoreload để không cần re-set */
        timer2[TLDR / 4] = LOAD_1MS;
        timer2[TCRR / 4] = LOAD_1MS;  /* nạp ngay vào counter */

        /* Xóa flag overflow cũ */
        timer2[IRQSTATUS / 4] = IRQ_OVF;

        /* Chạy timer (AR=1 autoreload, ST=1 start) */
        timer2[TCLR / 4] = (1 << 1) | (1 << 0);

        /* Đợi overflow */
        while (!(timer2[IRQSTATUS / 4] & IRQ_OVF))
            ;

        /* Dừng timer */
        timer2[TCLR / 4] = 0;
    }
}

int main(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open /dev/mem"); return 1; }

    if (timer_init(fd) != 0) return 1;

    printf("Bắt đầu đếm với delay 1s...\n");
    for (int i = 1; i <= 5; i++) {
        delay_ms(1000);
        printf("Tick %d\n", i);
    }

    return 0;
}
```

---

## 5. eHRPWM - High Resolution PWM

### 5.1 Tổng quan eHRPWM

eHRPWM tạo tín hiệu PWM chất lượng cao:
- Hai kênh output độc lập: **EPWMxA** và **EPWMxB**
- Độ phân giải lên đến 16-bit
- Hỗ trợ dead-band (bảo vệ H-bridge)
- Trip zone (bảo vệ quá dòng)

| Module | Base Address | Output A | Output B |
|--------|-------------|----------|----------|
| eHRPWM0 | `0x48300000` | P9.29 | P9.31 |
| eHRPWM1 | `0x48302000` | P9.14 | P9.16 |
| eHRPWM2 | `0x48304000` | P8.19 | P8.13 |

> **Nguồn**: spruh73q.pdf, Section 15.2.1; BeagleboneBlackP9HeaderTable.pdf

### 5.2 Thanh ghi eHRPWM cốt lõi

| Thanh ghi | Offset | Chức năng |
|-----------|--------|-----------|
| `TBPRD`   | `0x0A` | Time-Base Period (quyết định tần số PWM) |
| `TBCNT`   | `0x08` | Time-Base Counter (giá trị đếm hiện tại) |
| `TBCTL`   | `0x00` | Time-Base Control (mode, prescaler, direction) |
| `CMPA`    | `0x12` | Compare A (điều chỉnh duty cycle kênh A) |
| `CMPB`    | `0x14` | Compare B (điều chỉnh duty cycle kênh B) |
| `AQCTLA`  | `0x16` | Action Qualifier A: hành động khi match |
| `AQCTLB`  | `0x18` | Action Qualifier B |

> **Nguồn**: spruh73q.pdf, Table 15-2 (ePWM Register Map)

### 5.3 Tính toán PWM

$$F_{PWM} = \frac{F_{TBCLK}}{2 \times \text{TBPRD}}$$  (up-down mode)

$$F_{PWM} = \frac{F_{TBCLK}}{\text{TBPRD} + 1}$$  (up mode)

$$\text{Duty Cycle} = \frac{\text{CMPA}}{\text{TBPRD}} \times 100\%$$

Ví dụ: F_TBCLK = 100MHz, TBPRD = 1000 → F_PWM = 50kHz

### 5.4 Code C - eHRPWM1 tạo PWM 50Hz 50% duty

```c
/*
 * ehrpwm1_pwm.c
 * Tạo PWM 50Hz, duty cycle 50% trên P9.14 (eHRPWM1A)
 * Dùng cho servo motor hoặc LED dimming
 * Biên dịch: gcc -o pwm ehrpwm1_pwm.c
 * Chạy: sudo ./pwm
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>

#define EHRPWM1_BASE    0x48302000UL
#define CM_PER_BASE     0x44E00000UL
#define CTRL_BASE       0x44E10000UL

/* ePWM register offsets (16-bit registers!) (spruh73q.pdf, Table 15-2) */
#define TBCTL   0x00
#define TBCNT   0x08
#define TBPRD   0x0A
#define CMPA    0x12
#define CMPB    0x14
#define AQCTLA  0x16
#define AQCTLB  0x18

/* CM_PER_EPWMSS1_CLKCTRL (spruh73q.pdf, Section 8.1.12.1.16) */
#define CM_PER_EPWMSS1_CLKCTRL  0x0CC

/* Pinmux cho P9.14 = ehrpwm1A (spruh73q.pdf, Table 9-7) */
#define CONF_GPMC_A2  0x848   /* Mode 6 = eHRPWM1A */

#define PAGE_SIZE 4096

int main(void)
{
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open"); return 1; }

    volatile uint16_t *pwm = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE,
                                  MAP_SHARED, fd, EHRPWM1_BASE);
    volatile uint32_t *cm  = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE,
                                  MAP_SHARED, fd, CM_PER_BASE);
    volatile uint32_t *ctrl= mmap(NULL, PAGE_SIZE*16, PROT_READ|PROT_WRITE,
                                  MAP_SHARED, fd, CTRL_BASE);

    /* Bật clock EPWMSS1 */
    cm[CM_PER_EPWMSS1_CLKCTRL / 4] = 0x02;

    /* Pinmux P9.14: Mode 6 = eHRPWM1A */
    ctrl[CONF_GPMC_A2 / 4] = 0x06;

    /*
     * TBCTL: up-count mode, no prescaler
     * CLKDIV=0, HSPCLKDIV=0 → TBCLK = SYSCLKOUT = 100MHz
     */
    pwm[TBCTL / 2] = 0x0000;  /* up-count */

    /*
     * TBPRD = 2000 → F_PWM = 100MHz / 2001 ≈ 50kHz
     * Để 50Hz (servo): TBPRD = 2,000,000 cần prescaler
     * Với HSPCLKDIV=5, CLKDIV=6 → TBCLK = 100M/14/64 ≈ 111kHz
     * TBPRD = 2222 → F_PWM ≈ 50Hz
     */
    /* Sử dụng HSPCLKDIV=5 (÷14), CLKDIV=6 (÷64): TBCLK ≈ 111kHz */
    /* TBCTL[9:7]=HSPCLKDIV=5, TBCTL[14:12]=CLKDIV=6 */
    pwm[TBCTL / 2] = (6 << 10) | (5 << 7) | 0x0000;
    pwm[TBPRD / 2] = 2222;   /* ≈ 50Hz */
    pwm[TBCNT / 2] = 0;

    /* CMPA = 50% duty = 1111 */
    pwm[CMPA / 2] = 1111;

    /*
     * AQCTLA: Set khi TBCNT=0 (bit3:2=10), Clear khi TBCNT=CMPA (bit7:6=01)
     * → Active HIGH PWM
     */
    pwm[AQCTLA / 2] = 0x0042;  /* ZRO: set, CAU: clear */

    /* Start timer (ST bit đã set khi ghi TBCTL) */
    pwm[TBCTL / 2] |= 0x0000;  /* up-count đã run */

    printf("PWM 50Hz đang chạy trên P9.14. Nhấn Enter để thoát.\n");
    getchar();

    /* Tắt PWM */
    pwm[TBCTL / 2] |= (1 << 4);  /* TBCLKSYNC = force stop */

    return 0;
}
```

---

## 6. eCAP - Enhanced Capture

eCAP dùng để **đo tần số** và **duty cycle** của tín hiệu ngoài:

```
Tín hiệu ngoài → CAP pin → eCAP counter → capture timestamp vào CAP1..4
```

| Module | Base | Pin input |
|--------|------|-----------|
| eCAP0  | `0x48300100` | P9.42 (ecap0_in_apwm_out) |
| eCAP1  | `0x48302100` | P8.15 |
| eCAP2  | `0x48304100` | P9.28 |

> **Nguồn**: spruh73q.pdf, Section 15.3

---

## 7. Câu hỏi kiểm tra

1. DMTIMER2 clock là 24MHz. Muốn overflow mỗi 500µs, TLDR cần gán giá trị bao nhiêu?
2. Sự khác biệt giữa DMTIMER và eHRPWM là gì? Khi nào dùng eHRPWM thay cho toggle GPIO?
3. Trong TCLR, bit AR có ý nghĩa gì? Nếu AR=0 thì điều gì xảy ra sau khi timer overflow?
4. TBPRD quyết định thông số nào trong tín hiệu PWM? CMPA quyết định thông số nào?
5. eCAP khác eHRPWM ở chỗ nào?

---

## 8. Tài liệu tham khảo

| Nội dung | Tài liệu | Section |
|----------|----------|---------|
| DMTIMER overview | spruh73q.pdf | Chapter 20 (DMTIMER section) |
| DMTIMER register map | spruh73q.pdf | Table 20-14 |
| DMTIMER base addresses | spruh73q.pdf | Table 2-3, 2-4 |
| eHRPWM overview | spruh73q.pdf | Section 15.2 |
| eHRPWM register map | spruh73q.pdf | Table 15-2 |
| eCAP overview | spruh73q.pdf | Section 15.3 |
| CM_PER Timer/EPWMSS clocks | spruh73q.pdf | Section 8.1.12.1 |
