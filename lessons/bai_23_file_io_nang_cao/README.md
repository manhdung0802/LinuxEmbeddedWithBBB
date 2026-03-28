# Bài 23 - File I/O Nâng Cao: Tham Khảo Nhanh

## open() flags hay dùng
```c
O_RDONLY | O_WRONLY | O_RDWR     // access mode (bắt buộc)
O_CREAT  | O_TRUNC | O_APPEND   // file creation
O_NONBLOCK                       // non-blocking I/O
O_CLOEXEC                        // close on exec (luôn nên dùng)
O_SYNC                           // sync writes to hardware
```

## poll() pattern
```c
struct pollfd fds[] = {
    { .fd = gpio_fd, .events = POLLPRI },
    { .fd = uart_fd, .events = POLLIN  },
};
int ret = poll(fds, 2, 1000);   // 1000ms timeout
if (ret > 0) {
    if (fds[0].revents & POLLPRI) { /* GPIO event */ }
    if (fds[1].revents & POLLIN)  { /* UART data  */ }
}
```

## GPIO sysfs interrupt với poll
```bash
echo 53 > /sys/class/gpio/export
echo in > /sys/class/gpio/gpio53/direction
echo both > /sys/class/gpio/gpio53/edge   # rising | falling | both | none
```
```c
fd = open("/sys/class/gpio/gpio53/value", O_RDONLY | O_NONBLOCK);
// events = POLLPRI (bắt buộc với GPIO sysfs)
// Sau event: lseek(fd,0,SEEK_SET) rồi read()
```

## fcntl — set non-blocking sau khi open
```c
int fl = fcntl(fd, F_GETFL);
fcntl(fd, F_SETFL, fl | O_NONBLOCK);
```

## ioctl thường dùng
```c
ioctl(fd, FIONREAD, &n);     // bytes available to read
ioctl(fd, TIOCMGET, &bits);  // UART modem bits
ioctl(fd, TIOCMBIS, &bits);  // set modem bits
```

## References
- [bai_23_file_io_nang_cao.md](bai_23_file_io_nang_cao.md)
