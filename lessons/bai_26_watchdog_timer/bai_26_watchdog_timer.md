# Bài 26 - Watchdog Timer: Hardware Watchdog AM335x và Linux API

## 1. Mục tiêu bài học

- Hiểu tại sao cần watchdog trong embedded system
- Nắm watchdog hardware của AM335x (WDT1)
- Dùng Linux watchdog API (`/dev/watchdog`)
- Viết watchdog daemon chống lại hang/deadlock
- Hiểu WDIOF flags và ioctl watchdog commands

---

## 2. Tại Sao Cần Watchdog?

Trong embedded system, hệ thống có thể **trở nên không phản hồi** do:
- Infinite loop / deadlock trong software
- Stack overflow
- Memory corruption
- Kernel hang
- Mất kết nối mạng trong remote device

**Hardware watchdog** là timer độc lập với CPU:
- Phải được "kick" (pet) định kỳ bởi software
- Nếu software hang và không kick → timer expire → hardware **reset board**

```
Software hoạt động bình thường:
──────┬──────┬──────┬──────┬──────
      kick   kick   kick   kick    (mỗi N giây)

Software hang:
──────┬──────────────────────────────── RESET!
      kick   (không kick trong timeout)
                           ↑ WDT expire
```

---

## 3. Hardware Watchdog AM335x

AM335x có **WDT1** (Watchdog Timer 1):
- WDT0 dùng cho cortex-m3 power management — **không** được dùng
- WDT1 dùng cho application processor (ARM Cortex-A8) — đây là WDT chúng ta dùng
- Default timeout sau boot: thường 60 giây
- Phải được enabled trong kernel: `CONFIG_OMAP_WATCHDOG=y`

```
WDT1 Physical Base Address: 0x44E35000  (spruh73q.pdf, Table 2-3)
```

### WDT1 Register Map (spruh73q.pdf, Section 20.4)

| Offset | Register | Mô tả |
|--------|----------|-------|
| 0x010 | WDTWIDR | IP Revision |
| 0x014 | WDTSYSCONFIG | Configuration |
| 0x034 | WDTWISR | Interrupt Status |
| 0x038 | WDTWIER | Interrupt Enable |
| 0x03C | WDTWWER | Write Enable |
| 0x040 | WDTCLR | Control Register |
| 0x044 | WDTCRR | Counter Register |
| 0x048 | WDTLDR | Load Register |
| 0x04C | WDTTGR | Trigger Register |
| 0x050 | WDTWPS | Write Pending Status |
| 0x058 | WDTDLY | Delay Register |
| 0x05C | WDTWDLY | Watchdog Delay Register |
| 0x060 | WDTWSPR | Start/Stop Register |

---

## 4. Linux Watchdog API (/dev/watchdog)

**Cách đúng** để dùng watchdog trên Linux là qua `/dev/watchdog` — không nên truy cập thanh ghi trực tiếp khi kernel driver đang quản lý.

```c
#include <linux/watchdog.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
```

### 4.1 Mở và sử dụng /dev/watchdog

```c
/*
 * watchdog_basic.c — Sử dụng Linux watchdog API
 *
 * QUAN TRỌNG: Khi mở /dev/watchdog, timer BẮT ĐẦU chạy ngay!
 * Phải kick (ghi bất kỳ byte) trước khi timeout.
 * Khi đóng file (close()), watchdog sẽ RESET BOARD trừ khi ghi ký tự 'V' trước.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>

int main(void)
{
    /* Mở watchdog device */
    int wfd = open("/dev/watchdog", O_RDWR);
    if (wfd < 0) {
        perror("open /dev/watchdog");
        return 1;
    }
    printf("Watchdog opened — timer started!\n");

    /* Đọc thông tin watchdog */
    struct watchdog_info info;
    ioctl(wfd, WDIOC_GETSUPPORT, &info);
    printf("Identity: %s\n", info.identity);
    printf("Firmware: %u\n", info.firmware_version);
    printf("Options:  0x%08x\n", info.options);

    /* Lấy/set timeout */
    int timeout;
    ioctl(wfd, WDIOC_GETTIMEOUT, &timeout);
    printf("Timeout: %d seconds\n", timeout);

    /* Set timeout mới (nếu driver hỗ trợ WDIOF_SETTIMEOUT) */
    int new_timeout = 30;
    ioctl(wfd, WDIOC_SETTIMEOUT, &new_timeout);
    ioctl(wfd, WDIOC_GETTIMEOUT, &timeout);
    printf("New timeout: %d seconds\n", timeout);

    /* Main loop: kick watchdog mỗi 10 giây */
    printf("Starting main loop, kicking every 10s...\n");
    for (int i = 0; i < 5; i++) {
        sleep(10);

        /* Kick watchdog: ghi bất kỳ ký tự (trừ 'V') */
        write(wfd, "\0", 1);
        printf("Watchdog kicked at iteration %d\n", i + 1);
    }

    /*
     * Dừng watchdog an toàn:
     * Ghi ký tự 'V' để báo cho driver biết đây là intentional close
     * Sau đó close() sẽ KHÔNG trigger reset
     * (chỉ hoạt động nếu driver có WDIOF_MAGICCLOSE flag)
     */
    write(wfd, "V", 1);
    close(wfd);
    printf("Watchdog stopped safely\n");
    return 0;
}
```

### 4.2 ioctl Commands

```c
ioctl(wfd, WDIOC_GETSUPPORT, &info);        /* lấy info struct */
ioctl(wfd, WDIOC_GETSTATUS,  &status);      /* trạng thái hiện tại */
ioctl(wfd, WDIOC_GETBOOTSTATUS, &bootstatus); /* reset do WDT không? */
ioctl(wfd, WDIOC_SETOPTIONS, &options);     /* WDIOS_DISABLECARD / WDIOS_ENABLECARD */
ioctl(wfd, WDIOC_SETTIMEOUT, &new_timeout); /* set timeout */
ioctl(wfd, WDIOC_GETTIMEOUT, &timeout);     /* đọc timeout hiện tại */
ioctl(wfd, WDIOC_GETTIMELEFT, &timeleft);   /* còn bao nhiêu giây */
ioctl(wfd, WDIOC_KEEPALIVE,  NULL);         /* kick (alternative to write) */
```

### 4.3 watchdog_info.options Flags

```c
WDIOF_OVERHEAT     /* reset do overheating */
WDIOF_FANFAULT     /* reset do fan failure */
WDIOF_EXTERN1      /* external relay 1 */
WDIOF_EXTERN2      /* external relay 2 */
WDIOF_POWERUNDER   /* power under voltage */
WDIOF_CARDRESET    /* previous reset was by watchdog */
WDIOF_POWEROVER    /* power over voltage */
WDIOF_SETTIMEOUT   /* driver hỗ trợ set timeout */
WDIOF_MAGICCLOSE   /* ghi 'V' để disable WDT khi close */
WDIOF_PRETIMEOUT   /* hỗ trợ pre-timeout interrupt */
WDIOF_KEEPALIVEPING /* driver nhận ioctl KEEPALIVE */
```

---

## 5. Watchdog Daemon Hoàn Chỉnh

```c
/*
 * watchdog_daemon.c
 * Daemon giám sát hệ thống, kick watchdog nếu hệ thống healthy
 * Dừng kick nếu phát hiện problem → WDT reset board
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>
#include <time.h>
#include <errno.h>

#define WDT_TIMEOUT_SEC   30
#define KICK_INTERVAL_SEC 10   /* kick mỗi 10s, timeout là 30s */

static volatile int g_running = 1;
static int g_wfd = -1;

void sigterm_handler(int sig)
{
    g_running = 0;
}

/* Kiểm tra hệ thống có healthy không */
int check_system_health(void)
{
    /* Kiểm tra 1: /proc/loadavg — CPU load không quá cao */
    FILE *f = fopen("/proc/loadavg", "r");
    if (!f) return 0;   /* không đọc được → unhealthy */

    float load1;
    fscanf(f, "%f", &load1);
    fclose(f);
    if (load1 > 8.0) {   /* load average > 8 trên single-core = overloaded */
        fprintf(stderr, "System overloaded! load=%.2f\n", load1);
        return 0;
    }

    /* Kiểm tra 2: filesystem không đầy */
    f = fopen("/proc/mounts", "r");
    if (!f) return 0;
    fclose(f);

    /* Có thể thêm kiểm tra: network connectivity, sensor timeout, etc. */

    return 1;   /* healthy */
}

int main(void)
{
    struct sigaction sa = { .sa_handler = sigterm_handler };
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);

    /* Mở watchdog */
    g_wfd = open("/dev/watchdog", O_RDWR);
    if (g_wfd < 0) {
        perror("Cannot open /dev/watchdog");
        return 1;
    }

    /* Set timeout */
    int timeout = WDT_TIMEOUT_SEC;
    if (ioctl(g_wfd, WDIOC_SETTIMEOUT, &timeout) == 0)
        printf("WDT timeout set to %d seconds\n", timeout);

    /* Kiểm tra last boot cause */
    int bootstatus;
    if (ioctl(g_wfd, WDIOC_GETBOOTSTATUS, &bootstatus) == 0) {
        if (bootstatus & WDIOF_CARDRESET)
            printf("WARNING: Last reset was caused by watchdog!\n");
        else
            printf("Last reset: normal\n");
    }

    printf("Watchdog daemon started (timeout=%ds, kick_interval=%ds)\n",
           WDT_TIMEOUT_SEC, KICK_INTERVAL_SEC);

    while (g_running) {
        if (check_system_health()) {
            /* Kick watchdog */
            write(g_wfd, "\0", 1);

            int timeleft;
            ioctl(g_wfd, WDIOC_GETTIMELEFT, &timeleft);
            printf("WDT kicked. Time left: %d s\n", timeleft);
        } else {
            /* KHÔNG kick → sau timeout, WDT sẽ reset board */
            fprintf(stderr, "System unhealthy! Not kicking WDT — reset imminent!\n");
        }

        sleep(KICK_INTERVAL_SEC);
    }

    /* Graceful shutdown: dừng watchdog với magic close */
    printf("Stopping watchdog daemon gracefully...\n");
    write(g_wfd, "V", 1);
    close(g_wfd);
    return 0;
}
```

---

## 6. Kiểm tra Watchdog qua /sys

```bash
# Xem watchdog devices
ls /dev/watchdog*
# /dev/watchdog   /dev/watchdog0

# Xem thông tin qua sysfs
ls /sys/class/watchdog/watchdog0/
cat /sys/class/watchdog/watchdog0/identity
cat /sys/class/watchdog/watchdog0/timeout
cat /sys/class/watchdog/watchdog0/timeleft
cat /sys/class/watchdog/watchdog0/state       # active | inactive

# Test: triggers reset sau 5s (nguy hiểm, chỉ test trong lab!)
# echo 0 > /proc/sys/kernel/panic   # Không panic trên WDT reset
```

---

## 7. Câu hỏi kiểm tra

1. Tại sao không được đóng `/dev/watchdog` mà không ghi ký tự `'V'` trước?
2. `WDIOF_CARDRESET` trong bootstatus có ý nghĩa gì?
3. Nếu watchdog timeout = 30s và kick interval = 25s, hệ thống có an toàn không? Tại sao nên để khoảng cách lớn hơn?
4. Tại sao HAL watchdog (trực tiếp register) không an toàn khi kernel đang chạy?
5. `WDIOC_KEEPALIVE` vs `write(wfd, "\0", 1)` — cái nào nên dùng?

---

## 8. Tài liệu tham khảo

| Nội dung | Tài liệu | Ghi chú |
|----------|----------|---------|
| WDT1 registers | spruh73q.pdf, Section 20 | WDT base 0x44E35000 |
| WDT1 base address | spruh73q.pdf, Table 2-3 | 0x44E35000 |
| Linux watchdog API | linux/watchdog.h | WDIOC_*, WDIOF_* |
| Watchdog daemon | Documentation/watchdog/watchdog-api.rst | Kernel docs |
