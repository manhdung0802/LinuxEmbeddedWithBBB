# Bài Tập - Bài 24: Event Handling

## Bài tập 1: epoll vs poll Benchmark

So sánh hiệu năng `epoll` vs `poll` khi monitor nhiều pipe fd:

```c
/* Tạo N pipes, ghi vào tất cả sau 100ms, đo thời gian detect */
#define N 100  // thử 100, 500, 1000

int pipes[N][2];
for (int i = 0; i < N; i++) pipe(pipes[i]);

// Đo với poll()
// Đo với epoll()
// So sánh thời gian
```

**Yêu cầu**: Chạy với N=100, N=500, N=1000. Ghi kết quả thời gian vào bảng. Giải thích sự khác biệt.

---

## Bài tập 2: Graceful Shutdown Pattern

Viết daemon đọc sensor liên tục với graceful shutdown:
- Khi nhận `SIGTERM` hoặc `SIGINT`: dừng đọc, flush buffer ra file, thoát
- Khi nhận `SIGHUP`: reload config (in "Config reloaded")
- Dùng `signalfd` integrate vào epoll loop

```
daemon: Reading sensor...
^C → received SIGINT → flushing 25 records to log.bin → exit 0
```

**Yêu cầu**: Đảm bảo file log không bị corrupt khi nhận signal giữa chừng ghi.

---

## Bài tập 3: Cron-like Scheduler với timerfd

Viết scheduler đơn giản chạy các task theo lịch:

```c
typedef struct {
    const char *name;
    int interval_sec;
    void (*task)(void);
    int timerfd;
} ScheduledTask;

ScheduledTask tasks[] = {
    { "read_temperature", 2,  task_read_temp,  -1 },
    { "blink_led",        1,  task_blink,      -1 },
    { "log_status",       5,  task_log,        -1 },
};
```

**Yêu cầu**:
- Tạo `timerfd` riêng cho mỗi task với interval khác nhau
- Một epoll loop xử lý tất cả
- Mỗi task in `"[HH:MM:SS] task_name executed"`
- Chạy 30 giây rồi tự thoát
