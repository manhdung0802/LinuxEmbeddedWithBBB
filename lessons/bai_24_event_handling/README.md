# Bài 24 - Event Handling: Tham Khảo Nhanh

## epoll lifecycle
```c
int epfd = epoll_create1(EPOLL_CLOEXEC);

// Thêm fd
struct epoll_event ev = { .events = EPOLLIN, .data.fd = fd };
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

// Wait
struct epoll_event evs[16];
int n = epoll_wait(epfd, evs, 16, timeout_ms); // -1 = infinite

// Xử lý
for (int i = 0; i < n; i++)
    if (evs[i].data.fd == my_fd) { /* handle */ }

// Remove
epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
close(epfd);
```

## signalfd pattern
```c
sigset_t mask;
sigemptyset(&mask); sigaddset(&mask, SIGINT); sigaddset(&mask, SIGTERM);
sigprocmask(SIG_BLOCK, &mask, NULL);            // block TRƯỚC
int sfd = signalfd(-1, &mask, SFD_NONBLOCK|SFD_CLOEXEC);
// khi EPOLLIN: read signalfd_siginfo
struct signalfd_siginfo info;
read(sfd, &info, sizeof(info));
```

## timerfd pattern
```c
int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK|TFD_CLOEXEC);
struct itimerspec ts = {
    .it_value    = {1, 0},          // first fire: 1s
    .it_interval = {0, 500000000},  // repeat: 500ms
};
timerfd_settime(tfd, 0, &ts, NULL);
// khi EPOLLIN: PHẢI đọc 8 bytes
uint64_t exp; read(tfd, &exp, 8);
```

## eventfd pattern
```c
int efd = eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC);
// Signal:
uint64_t v = 1; write(efd, &v, 8);
// Wait:
uint64_t count; read(efd, &count, 8);  // blocks until > 0
```

## sigaction (safe signal handling)
```c
struct sigaction sa;
sa.sa_handler = my_handler;
sigemptyset(&sa.sa_mask);
sa.sa_flags = SA_RESTART;
sigaction(SIGINT, &sa, NULL);
```

## References
- [bai_24_event_handling.md](bai_24_event_handling.md)
