# Bài 24 - Xử Lý Event: epoll, Signal Handling, eventfd, timerfd

## 1. Mục tiêu bài học

- Dùng `epoll` cho event-driven I/O hiệu quả (thay thế poll/select)
- Xử lý Unix signal đúng cách: `sigaction`, `signalfd`
- Dùng `eventfd` để đồng bộ thread/process
- Dùng `timerfd` cho timer event trong event loop
- Xây dựng **event loop** hoàn chỉnh cho embedded application

---

## 2. epoll — Scalable I/O Event Notification

`epoll` tốt hơn `poll`/`select` khi có nhiều fd (hàng trăm đến hàng nghìn):
- `poll`/`select`: scan toàn bộ list mỗi lần → O(n)
- `epoll`: kernel duy trì list active fd → O(1)

```c
#include <sys/epoll.h>

/* Tạo epoll instance */
int epfd = epoll_create1(EPOLL_CLOEXEC);

/* Thêm fd vào epoll */
struct epoll_event ev;
ev.events  = EPOLLIN;          /* sự kiện quan tâm */
ev.data.fd = target_fd;        /* fd để nhận dạng sau */
epoll_ctl(epfd, EPOLL_CTL_ADD, target_fd, &ev);

/* Chờ events */
struct epoll_event events[16];
int n = epoll_wait(epfd, events, 16, timeout_ms);
for (int i = 0; i < n; i++) {
    if (events[i].data.fd == target_fd)
        /* xử lý event */
}

/* Xóa fd khỏi epoll */
epoll_ctl(epfd, EPOLL_CTL_DEL, target_fd, NULL);
close(epfd);
```

### epoll events

| Event | Ý nghĩa |
|-------|---------|
| `EPOLLIN` | Có data để đọc |
| `EPOLLOUT` | Có thể ghi mà không block |
| `EPOLLPRI` | Urgent data (GPIO edge) |
| `EPOLLERR` | Lỗi trên fd |
| `EPOLLHUP` | Đối phương đóng kết nối |
| `EPOLLET` | Edge-triggered (chỉ báo khi trạng thái đổi) |
| `EPOLLONESHOT` | Chỉ fire một lần, cần re-arm bằng EPOLL_CTL_MOD |

---

## 3. Signal Handling với sigaction

`signal()` cũ không đáng tin trong multi-thread. Dùng `sigaction()` thay thế.

```c
/*
 * Xử lý SIGINT (Ctrl+C) và SIGTERM để shutdown gracefully
 */
#include <signal.h>

static volatile sig_atomic_t g_running = 1;

void signal_handler(int sig)
{
    /*
     * CẢNH BÁO: Trong signal handler chỉ được gọi async-signal-safe functions!
     * Không được: printf, malloc, pthread_mutex_lock, ...
     * Được: write(), _exit(), sig_atomic_t assignment
     */
    g_running = 0;
}

int setup_signals(void)
{
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);       /* không block thêm signal nào */
    sa.sa_flags = SA_RESTART;       /* tự động restart syscalls bị interrupt */

    if (sigaction(SIGINT,  &sa, NULL) < 0) return -1;
    if (sigaction(SIGTERM, &sa, NULL) < 0) return -1;

    /* Ignore SIGPIPE (quan trọng với socket/pipe programming) */
    signal(SIGPIPE, SIG_IGN);

    return 0;
}
```

### signalfd — Xử lý signal như fd

`signalfd` cho phép nhận signal qua fd → integrate vào epoll loop.

```c
#include <sys/signalfd.h>

sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGINT);
sigaddset(&mask, SIGTERM);

/* Block signals (quan trọng: phải block trước khi tạo signalfd) */
sigprocmask(SIG_BLOCK, &mask, NULL);

/* Tạo signal fd */
int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);

/* Đọc signal info */
struct signalfd_siginfo info;
read(sfd, &info, sizeof(info));
printf("Received signal: %d from PID: %d\n",
       info.ssi_signo, info.ssi_pid);
```

---

## 4. eventfd — Thread/Process Synchronization

`eventfd` tạo fd dùng như **counter** để wake-up thread/process khác.

```c
#include <sys/eventfd.h>

int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
/*                ↑ initial value */

/* Ghi (increment counter, wake up reader) */
uint64_t value = 1;
write(efd, &value, 8);

/* Đọc (block đến khi counter > 0, lấy và reset về 0) */
uint64_t count;
read(efd, &count, 8);
printf("Woken up %llu times\n", (unsigned long long)count);
```

---

## 5. timerfd — Timer trong Event Loop

```c
#include <sys/timerfd.h>
#include <time.h>

/* Tạo timer */
int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

/* Cấu hình: fire sau 1s, repeat mỗi 500ms */
struct itimerspec ts = {
    .it_value    = { .tv_sec = 1,   .tv_nsec = 0 },      /* first fire */
    .it_interval = { .tv_sec = 0,   .tv_nsec = 500000000 }, /* repeat */
};
timerfd_settime(tfd, 0, &ts, NULL);

/* Khi epoll báo EPOLLIN trên tfd: */
uint64_t expirations;
read(tfd, &expirations, 8);   /* PHẢI đọc để clear */
printf("Timer fired %llu times\n", (unsigned long long)expirations);
```

---

## 6. Event Loop Hoàn Chỉnh cho BBB

Kết hợp tất cả: GPIO, UART, timer, signal.

```c
/*
 * event_loop_bbb.c
 * Event loop đồng thời monitor:
 *   - GPIO53 edge (button press)
 *   - UART1 data
 *   - 1-second heartbeat timer
 *   - SIGINT/SIGTERM để shutdown gracefully
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <string.h>
#include <stdint.h>

#define MAX_EVENTS 8

typedef struct {
    int epfd;
    int gpio_fd;
    int uart_fd;
    int timer_fd;
    int signal_fd;
} AppContext;

static void handle_gpio(AppContext *ctx)
{
    char val[4] = {0};
    lseek(ctx->gpio_fd, 0, SEEK_SET);
    read(ctx->gpio_fd, val, 3);
    printf("[EVENT] Button GPIO53=%c\n", val[0]);
}

static void handle_uart(AppContext *ctx)
{
    char buf[64];
    ssize_t n = read(ctx->uart_fd, buf, sizeof(buf)-1);
    if (n > 0) {
        buf[n] = 0;
        printf("[EVENT] UART recv (%zd bytes): %s\n", n, buf);
    }
}

static void handle_timer(AppContext *ctx)
{
    uint64_t exp;
    read(ctx->timer_fd, &exp, 8);
    printf("[EVENT] Timer heartbeat #%llu\n", (unsigned long long)exp);
}

static int handle_signal(AppContext *ctx)
{
    struct signalfd_siginfo info;
    read(ctx->signal_fd, &info, sizeof(info));
    printf("[EVENT] Signal %d received — shutting down\n", info.ssi_signo);
    return 0;   /* signal to stop loop */
}

static void add_to_epoll(int epfd, int fd, uint32_t events)
{
    struct epoll_event ev = { .events = events, .data.fd = fd };
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

int main(void)
{
    AppContext ctx;

    /* Setup epoll */
    ctx.epfd = epoll_create1(EPOLL_CLOEXEC);

    /* GPIO */
    system("echo 53 > /sys/class/gpio/export 2>/dev/null");
    system("echo in > /sys/class/gpio/gpio53/direction");
    system("echo both > /sys/class/gpio/gpio53/edge");
    ctx.gpio_fd = open("/sys/class/gpio/gpio53/value", O_RDONLY | O_NONBLOCK);
    char dummy[4]; read(ctx.gpio_fd, dummy, sizeof(dummy));  /* clear initial */
    add_to_epoll(ctx.epfd, ctx.gpio_fd, EPOLLPRI | EPOLLERR);

    /* UART */
    ctx.uart_fd = open("/dev/ttyS1", O_RDWR | O_NONBLOCK | O_NOCTTY);
    if (ctx.uart_fd >= 0)
        add_to_epoll(ctx.epfd, ctx.uart_fd, EPOLLIN);

    /* Timer: 1s repeat */
    ctx.timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    struct itimerspec ts = {
        .it_value    = {1, 0},
        .it_interval = {1, 0},
    };
    timerfd_settime(ctx.timer_fd, 0, &ts, NULL);
    add_to_epoll(ctx.epfd, ctx.timer_fd, EPOLLIN);

    /* Signal fd */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    ctx.signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    add_to_epoll(ctx.epfd, ctx.signal_fd, EPOLLIN);

    printf("Event loop started. Press button, send UART, or Ctrl+C\n");

    struct epoll_event events[MAX_EVENTS];
    int running = 1;

    while (running) {
        int n = epoll_wait(ctx.epfd, events, MAX_EVENTS, -1);
        if (n < 0) { perror("epoll_wait"); break; }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            if      (fd == ctx.gpio_fd)   handle_gpio(&ctx);
            else if (fd == ctx.uart_fd)   handle_uart(&ctx);
            else if (fd == ctx.timer_fd)  handle_timer(&ctx);
            else if (fd == ctx.signal_fd) running = handle_signal(&ctx);
        }
    }

    /* Cleanup */
    close(ctx.gpio_fd); close(ctx.uart_fd);
    close(ctx.timer_fd); close(ctx.signal_fd);
    close(ctx.epfd);
    printf("Event loop stopped cleanly\n");
    return 0;
}
```

---

## 7. Câu hỏi kiểm tra

1. Tại sao `epoll` nhanh hơn `poll` khi số fd tăng?
2. `EPOLLET` (edge-triggered) khác gì `EPOLLIN` (level-triggered)?
3. Tại sao phải `sigprocmask(SIG_BLOCK, ...)` **trước** khi `signalfd()`?
4. Khi đọc `timerfd`, tại sao phải đọc đúng 8 byte?
5. `sig_atomic_t` là gì và tại sao cần nó trong signal handler?

---

## 8. Tài liệu tham khảo

| API | Man page | Ghi chú |
|-----|----------|---------|
| epoll | man 7 epoll | epoll_create1, epoll_ctl, epoll_wait |
| sigaction | man 2 sigaction | SA_RESTART, sa_mask |
| signalfd | man 2 signalfd | SFD_NONBLOCK, SFD_CLOEXEC |
| eventfd | man 2 eventfd | EFD_NONBLOCK |
| timerfd_create | man 2 timerfd_create | CLOCK_MONOTONIC, itimerspec |
