# Bài 11 - Character Device Driver nâng cao

## 1. M?c tiêu bài h?c
- Implement `ioctl()` d? g?i command t? userspace
- Dùng `private_data` d? luu per-device state
- Qu?n lý buffer read/write v?i circular buffer
- H? tr? multiple device instances
- Vi?t userspace test program cho driver

---

## 2. IOCTL - I/O Control

### 2.1 T?i sao c?n ioctl?

`read()`/`write()` ch? truy?n data. Khi c?n **g?i l?nh di?u khi?n** (set baud rate, get status, reset device...), dùng `ioctl()`.

```
Userspace                          Kernel Driver
---------                          ------------
ioctl(fd, SET_LED, 1)         --?  my_ioctl(file, SET_LED, 1)
ioctl(fd, GET_STATUS, &val)   --?  my_ioctl(file, GET_STATUS, arg)
```

### 2.2 Ð?nh nghia ioctl command number:

```c
/* Trong header file dùng chung gi?a kernel và userspace */
#include <linux/ioctl.h>

#define MY_IOC_MAGIC  'k'     /* Magic number, duy nh?t */

/* _IO: không có data, _IOW: write, _IOR: read, _IOWR: c? hai */
#define MY_SET_LED     _IOW(MY_IOC_MAGIC, 1, int)
#define MY_GET_STATUS  _IOR(MY_IOC_MAGIC, 2, int)
#define MY_RESET       _IO(MY_IOC_MAGIC, 3)
#define MY_CONFIG      _IOWR(MY_IOC_MAGIC, 4, struct my_config)
```

**Macro phân tích**:
- `_IOW('k', 1, int)` ? command write data type `int`, magic 'k', sequence 1
- M?i driver ch?n magic number riêng (xem `Documentation/userspace-api/ioctl/ioctl-number.rst`)

### 2.3 Implement ioctl trong driver:

```c
static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct my_device *dev = file->private_data;
    int val;

    switch (cmd) {
    case MY_SET_LED:
        if (copy_from_user(&val, (int __user *)arg, sizeof(val)))
            return -EFAULT;
        /* Ði?u khi?n LED */
        dev->led_state = val;
        writel(val ? (1 << dev->gpio_bit) : 0,
               dev->base + GPIO_SETDATAOUT);
        break;

    case MY_GET_STATUS:
        val = dev->led_state;
        if (copy_to_user((int __user *)arg, &val, sizeof(val)))
            return -EFAULT;
        break;

    case MY_RESET:
        dev->led_state = 0;
        writel((1 << dev->gpio_bit), dev->base + GPIO_CLEARDATAOUT);
        break;

    default:
        return -ENOTTY;   /* Unknown command */
    }

    return 0;
}

static const struct file_operations my_fops = {
    .owner          = THIS_MODULE,
    .open           = my_open,
    .release        = my_release,
    .read           = my_read,
    .write          = my_write,
    .unlocked_ioctl = my_ioctl,    /* Dùng unlocked_ioctl, không ph?i ioctl */
};
```

### 2.4 Userspace g?i ioctl:

```c
#include <sys/ioctl.h>
#include <fcntl.h>
#include "my_ioctl.h"   /* Header ch?a MY_SET_LED, ... */

int main(void)
{
    int fd = open("/dev/myled", O_RDWR);
    int val = 1;

    /* B?t LED */
    ioctl(fd, MY_SET_LED, &val);

    /* Ð?c status */
    ioctl(fd, MY_GET_STATUS, &val);
    printf("LED state: %d\n", val);

    /* Reset */
    ioctl(fd, MY_RESET);

    close(fd);
    return 0;
}
```

---

## 3. Private Data - Per-device State

### 3.1 V?n d?:

Khi driver qu?n lý **nhi?u device** (ví d? 4 LED), m?i device c?n state riêng. Dùng `private_data` trong `struct file`.

### 3.2 Ð?nh nghia struct device:

```c
struct my_led_device {
    struct cdev cdev;
    void __iomem *base;
    int gpio_bit;
    int led_state;
    char name[32];
    dev_t devno;
};
```

### 3.3 Gán private_data trong open:

```c
static int my_open(struct inode *inode, struct file *file)
{
    struct my_led_device *dev;

    /* container_of: t? cdev trong inode, tìm ra struct ch?a nó */
    dev = container_of(inode->i_cdev, struct my_led_device, cdev);

    /* Gán vào private_data d? các hàm khác dùng */
    file->private_data = dev;

    pr_info("Opened %s\n", dev->name);
    return 0;
}
```

### 3.4 Dùng private_data trong read/write/ioctl:

```c
static ssize_t my_read(struct file *file, char __user *buf,
                        size_t count, loff_t *offset)
{
    /* L?y device struct t? private_data */
    struct my_led_device *dev = file->private_data;

    /* Dùng dev->base, dev->gpio_bit, ... */
    char status = dev->led_state ? '1' : '0';

    if (*offset > 0)
        return 0;

    if (copy_to_user(buf, &status, 1))
        return -EFAULT;

    *offset += 1;
    return 1;
}
```

### 3.5 `container_of` macro:

```c
/* Cho bi?t struct cha ch?a member */
container_of(ptr, type, member)

/* Ví d?: */
struct my_led_device *dev;
dev = container_of(inode->i_cdev, struct my_led_device, cdev);
/*                  ?              ?                      ?
             ptr t?i cdev    type struct cha     tên member cdev trong struct */
```

Ðây là pattern c?c k? ph? bi?n trong Linux kernel.

---

## 4. Multiple Device Instances

### 4.1 T?o nhi?u device t? 1 driver:

```c
#define NUM_DEVICES 4

static struct my_led_device devices[NUM_DEVICES];
static dev_t base_dev;
static struct class *led_class;

static int __init multi_led_init(void)
{
    int i, ret;

    /* C?p phát NUM_DEVICES minor numbers */
    ret = alloc_chrdev_region(&base_dev, 0, NUM_DEVICES, "multi_led");
    if (ret)
        return ret;

    led_class = class_create("multi_led");
    if (IS_ERR(led_class)) {
        unregister_chrdev_region(base_dev, NUM_DEVICES);
        return PTR_ERR(led_class);
    }

    for (i = 0; i < NUM_DEVICES; i++) {
        devices[i].devno = MKDEV(MAJOR(base_dev), i);
        devices[i].gpio_bit = 21 + i;  /* GPIO1_21..GPIO1_24 */
        snprintf(devices[i].name, sizeof(devices[i].name),
                 "led%d", i);

        cdev_init(&devices[i].cdev, &my_fops);
        devices[i].cdev.owner = THIS_MODULE;

        ret = cdev_add(&devices[i].cdev, devices[i].devno, 1);
        if (ret)
            goto err_cleanup;

        device_create(led_class, NULL, devices[i].devno,
                      NULL, "led%d", i);
    }
    /* K?t qu?: /dev/led0, /dev/led1, /dev/led2, /dev/led3 */

    return 0;

err_cleanup:
    while (--i >= 0) {
        device_destroy(led_class, devices[i].devno);
        cdev_del(&devices[i].cdev);
    }
    class_destroy(led_class);
    unregister_chrdev_region(base_dev, NUM_DEVICES);
    return ret;
}
```

---

## 5. Read/Write Buffer nâng cao

### 5.1 Circular buffer (ring buffer):

```c
#define CBUF_SIZE 1024

struct circ_buf_data {
    char buf[CBUF_SIZE];
    int head;   /* V? trí ghi */
    int tail;   /* V? trí d?c */
};

static inline int circ_count(struct circ_buf_data *cb)
{
    return (cb->head - cb->tail) & (CBUF_SIZE - 1);
}

static inline int circ_space(struct circ_buf_data *cb)
{
    return (cb->tail - cb->head - 1) & (CBUF_SIZE - 1);
}
```

### 5.2 Write vào circular buffer:

```c
static ssize_t my_write(struct file *file, const char __user *ubuf,
                         size_t count, loff_t *offset)
{
    struct my_device *dev = file->private_data;
    int space, to_write, first_part;

    space = circ_space(&dev->cbuf);
    if (space == 0)
        return -EAGAIN;   /* Buffer d?y */

    to_write = min((int)count, space);

    /* Copy vào circular buffer, x? lý wrap-around */
    first_part = min(to_write, CBUF_SIZE - dev->cbuf.head);
    if (copy_from_user(dev->cbuf.buf + dev->cbuf.head, ubuf, first_part))
        return -EFAULT;

    if (to_write > first_part) {
        if (copy_from_user(dev->cbuf.buf, ubuf + first_part,
                           to_write - first_part))
            return -EFAULT;
    }

    dev->cbuf.head = (dev->cbuf.head + to_write) & (CBUF_SIZE - 1);
    return to_write;
}
```

---

## 6. Ví d? hoàn ch?nh: GPIO LED v?i ioctl

### 6.1 Header chung (`my_led_ioctl.h`):

```c
#ifndef _MY_LED_IOCTL_H
#define _MY_LED_IOCTL_H

#include <linux/ioctl.h>

#define LED_IOC_MAGIC  'L'

#define LED_SET_STATE   _IOW(LED_IOC_MAGIC, 1, int)
#define LED_GET_STATE   _IOR(LED_IOC_MAGIC, 2, int)
#define LED_TOGGLE      _IO(LED_IOC_MAGIC, 3)

struct led_config {
    int gpio_num;
    int blink_rate_ms;
};

#define LED_SET_CONFIG   _IOW(LED_IOC_MAGIC, 4, struct led_config)

#endif
```

### 6.2 Test program (`test_led.c`):

```c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "my_led_ioctl.h"

int main(int argc, char *argv[])
{
    int fd, state;

    fd = open("/dev/myled0", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    /* B?t LED */
    state = 1;
    if (ioctl(fd, LED_SET_STATE, &state) < 0)
        perror("ioctl SET");

    sleep(1);

    /* Ð?c tr?ng thái */
    if (ioctl(fd, LED_GET_STATE, &state) < 0)
        perror("ioctl GET");
    printf("LED state: %d\n", state);

    /* Toggle */
    if (ioctl(fd, LED_TOGGLE) < 0)
        perror("ioctl TOGGLE");

    close(fd);
    return 0;
}
```

---

## 7. Ki?n th?c c?t lõi sau bài này

1. **ioctl** = g?i command + data gi?a userspace?kernel, dùng `_IO/_IOW/_IOR/_IOWR`
2. **private_data** = luu per-device state, gán trong `open()`, dùng trong m?i fops
3. **`container_of`** = tìm parent struct t? member pointer
4. **Multi-device** = 1 driver qu?n lý nhi?u device b?ng nhi?u minor number
5. **Circular buffer** = cách qu?n lý read/write buffer hi?u qu?

---

## 8. Câu h?i ki?m tra

1. S? khác nhau gi?a `ioctl` và `write` khi giao ti?p v?i driver?
2. T?i sao ioctl command c?n magic number? N?u 2 driver trùng magic number thì sao?
3. `container_of(ptr, type, member)` ho?t d?ng nhu th? nào?
4. Khi nào nên dùng `private_data`?
5. Vi?t ioctl header cho driver có 3 command: SET_FREQ, GET_FREQ, ENABLE_OUTPUT.

---

## 9. Chu?n b? cho bài sau

Bài ti?p theo: **Bài 9 - Platform Driver & Device Tree Binding**

Ta s? h?c **platform_driver** — framework chính d? vi?t driver cho peripheral trên SoC, k?t h?p Device Tree d? t? d?ng probe driver.
