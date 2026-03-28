# Bài Tập - Bài 26: Watchdog Timer

## Bài tập 1: Watchdog với Health Check

Viết watchdog daemon kiểm tra nhiều tiêu chí trước khi kick:

```c
typedef struct {
    const char *name;
    int (*check)(void);
    int weight;    /* 1 = must pass, 0 = optional */
} HealthCheck;
```

Implement 3 health checks:
1. **CPU load**: `check_cpu_load()` — đọc `/proc/loadavg`, fail nếu 1-min load > 4.0
2. **Memory**: `check_memory()` — đọc `/proc/meminfo`, fail nếu available < 10MB
3. **Disk**: `check_disk()` — dùng `statvfs("/")`, fail nếu free < 5%

**Yêu cầu**: Chỉ kick WDT nếu tất cả checks trả về OK. Log lý do nếu không kick.

---

## Bài tập 2: Pre-timeout Notification

AM335x WDT hỗ trợ pre-timeout interrupt: cảnh báo trước khi reset.

Dùng `WDIOC_SETPRETIMEOUT` và xử lý `SIGALRM` (hoặc `/dev/watchdog` event):

```c
/* Set pre-timeout: cảnh báo 5 giây trước khi reset */
int pretimeout = 5;
ioctl(wfd, WDIOC_SETPRETIMEOUT, &pretimeout);

/* Khi pre-timeout xảy ra, kernel gửi signal cho process hoặc gọi notifier */
/* Dùng watchdog_info.options & WDIOF_PRETIMEOUT để kiểm tra support */
```

**Yêu cầu**:
1. Kiểm tra driver có hỗ trợ PRETIMEOUT không
2. Nếu có: set pre-timeout = 5s, khi nhận signal → log "CRITICAL: WDT pre-timeout! Attempting recovery..."
3. Thử "recover" (kick lại) trong handler

---

## Bài tập 3: Simulate Hung Process Detection

Viết 2 process:
- **Monitor**: daemon kick WDT
- **Worker**: thực hiện job, cập nhật "heartbeat" vào shared memory hoặc file
- **Monitor** logic: nếu worker heartbeat không được cập nhật trong 5 giây → STOP kicking WDT → board reset

```c
/* Shared state file hoặc POSIX shared memory */
#define HEARTBEAT_FILE "/tmp/worker_heartbeat"

/* Worker: cập nhật timestamp mỗi 2s */
time_t now = time(NULL);
write(hb_fd, &now, sizeof(now));

/* Monitor: kiểm tra timestamp */
time_t last_hb;
read(hb_fd, &last_hb, sizeof(last_hb));
if (time(NULL) - last_hb > 5) {
    /* Worker hung! Do NOT kick WDT */
}
```

Giả lập worker hang bằng cách cho nó `sleep(30)` đột ngột.
