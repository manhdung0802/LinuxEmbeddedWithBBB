# Bài Tập - Bài 18: Character Device Driver

## Bài tập 1: Driver với Multiple Minor Numbers

Sửa `mychar_driver.c` để hỗ trợ **2 device** (`/dev/mychar0` và `/dev/mychar1`), mỗi device có buffer riêng.

**Yêu cầu**:
- `alloc_chrdev_region(&dev_num, 0, 2, "mychar")` — cấp 2 minor numbers
- Dùng mảng `kernel_buf[2][256]` và `buf_len[2]`
- Trong `open()`, xác định minor number bằng `iminor(inode)` để biết đang mở device nào
- Test: `echo "Dev0" > /dev/mychar0 ; echo "Dev1" > /dev/mychar1`

---

## Bài tập 2: GPIO Driver qua ioctl

Viết character device driver `/dev/gpio_ctrl` điều khiển LED USR0 (GPIO1_21):

```c
/* ioctl commands */
#define GPIO_IOC_MAGIC    'G'
#define GPIO_LED_ON       _IO(GPIO_IOC_MAGIC, 0)    // bật LED
#define GPIO_LED_OFF      _IO(GPIO_IOC_MAGIC, 1)    // tắt LED
#define GPIO_LED_GET      _IOR(GPIO_IOC_MAGIC, 2, int) // đọc trạng thái
```

**Yêu cầu**:
- `ioremap(0x4804C000, 0x1000)` trong `probe`/`init`
- `GPIO_OE` (offset 0x134): set GPIO1_21 làm output
- `GPIO_SETDATAOUT` (0x194), `GPIO_CLEARDATAOUT` (0x190) để bật/tắt
- `GPIO_DATAIN` (0x138) để đọc trạng thái

**Test userspace**:
```c
int fd = open("/dev/gpio_ctrl", O_RDWR);
ioctl(fd, GPIO_LED_ON);
sleep(1);
ioctl(fd, GPIO_LED_OFF);
int state;
ioctl(fd, GPIO_LED_GET, &state);
printf("LED state: %d\n", state);
```

---

## Bài tập 3: Phân tích Race Condition

Đoạn code sau có race condition khi 2 process cùng gọi `write()`:

```c
static ssize_t mychar_write(struct file *filp, const char __user *ubuf,
                             size_t count, loff_t *ppos)
{
    /* RACE CONDITION ở đây - tại sao? */
    buf_len = 0;
    if (copy_from_user(kernel_buf, ubuf, count))
        return -EFAULT;
    buf_len = count;
    return count;
}
```

**Yêu cầu**:
1. Giải thích tại sao đây là race condition
2. Sửa bằng `mutex`:
   ```c
   static DEFINE_MUTEX(mychar_mutex);
   // Trong write():
   mutex_lock(&mychar_mutex);
   // ... critical section ...
   mutex_unlock(&mychar_mutex);
   ```
3. Tại sao KHÔNG được dùng mutex trong interrupt context?
