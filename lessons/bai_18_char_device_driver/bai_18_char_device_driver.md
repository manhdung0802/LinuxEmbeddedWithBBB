# Bài 18 - Character Device Driver: Giao Tiếp Userspace ↔ Kernel

## 1. Mục tiêu bài học

- Hiểu mô hình Character Device trong Linux
- Viết driver đầy đủ với `open`, `release`, `read`, `write`, `ioctl`
- Tạo device node `/dev/mydevice` tự động qua `class_create`
- An toàn truyền dữ liệu giữa userspace và kernel space
- Test driver từ userspace bằng C và shell

---

## 2. Character Device là gì?

Linux quản lý thiết bị qua **device file** trong `/dev/`. Có 3 loại:
- **Character device** (`c`): đọc/ghi tuần tự (serial, GPIO, sensor)
- **Block device** (`b`): đọc/ghi theo block (disk, SD card)
- **Network device**: không có file `/dev/`, dùng socket

```bash
ls -la /dev/ttyS0  /dev/sda
# crw-rw---- 1 root dialout 4, 64 ... /dev/ttyS0   ← c = character
# brw-rw---- 1 root disk    8,  0 ... /dev/sda      ← b = block
#                           ↑  ↑
#                     major num  minor num
```

**Major number**: xác định driver nào phục vụ
**Minor number**: xác định thiết bị cụ thể trong cùng driver

---

## 3. Kiến Trúc Character Device Driver

```
Userspace                   Kernel Space
─────────────────────────────────────────────────
open("/dev/mydev")    →  filp->f_op->open()
read(fd, buf, n)      →  filp->f_op->read()
write(fd, buf, n)     →  filp->f_op->write()
ioctl(fd, cmd, arg)   →  filp->f_op->unlocked_ioctl()
close(fd)             →  filp->f_op->release()
```

---

## 4. Viết Character Device Driver Đầy Đủ

### mychar_driver.c

```c
/*
 * mychar_driver.c - Character device driver cơ bản cho BBB
 * Tạo /dev/mychar, cho phép đọc/ghi string
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>           /* cdev_init, cdev_add */
#include <linux/fs.h>             /* file_operations, alloc_chrdev_region */
#include <linux/device.h>         /* class_create, device_create */
#include <linux/uaccess.h>        /* copy_to_user, copy_from_user */
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BBB Student");
MODULE_DESCRIPTION("Basic Character Device Driver");

#define DEVICE_NAME "mychar"      /* tên device file: /dev/mychar */
#define CLASS_NAME  "myclass"     /* tên class trong /sys/class/ */
#define BUF_SIZE    256

/* Global variables */
static dev_t dev_num;             /* major + minor number */
static struct cdev my_cdev;       /* cdev structure */
static struct class *my_class;    /* device class */
static struct device *my_device;  /* device in /sys/devices */

/* Internal buffer lưu dữ liệu */
static char kernel_buf[BUF_SIZE];
static int buf_len = 0;

/* ──────────────────────────────────────────
 * File Operations
 * ────────────────────────────────────────── */

static int mychar_open(struct inode *inode, struct file *filp)
{
    pr_info("mychar: opened (major=%d, minor=%d)\n",
            imajor(inode), iminor(inode));
    return 0;
}

static int mychar_release(struct inode *inode, struct file *filp)
{
    pr_info("mychar: closed\n");
    return 0;
}

/*
 * read: copy dữ liệu từ kernel_buf → userspace
 * filp: file pointer
 * ubuf: user buffer (địa chỉ userspace)
 * count: số byte muốn đọc
 * ppos: file position (offset)
 */
static ssize_t mychar_read(struct file *filp, char __user *ubuf,
                           size_t count, loff_t *ppos)
{
    int bytes_to_copy;

    if (*ppos >= buf_len)
        return 0;   /* EOF */

    bytes_to_copy = min((int)(buf_len - *ppos), (int)count);

    /* QUAN TRỌNG: không được dùng memcpy trực tiếp với userspace pointer!
     * copy_to_user() validate địa chỉ và handle page fault an toàn */
    if (copy_to_user(ubuf, kernel_buf + *ppos, bytes_to_copy)) {
        pr_err("mychar: copy_to_user failed\n");
        return -EFAULT;
    }

    *ppos += bytes_to_copy;
    pr_info("mychar: read %d bytes\n", bytes_to_copy);
    return bytes_to_copy;
}

/*
 * write: copy dữ liệu từ userspace → kernel_buf
 */
static ssize_t mychar_write(struct file *filp, const char __user *ubuf,
                            size_t count, loff_t *ppos)
{
    int bytes_to_copy = min((int)count, BUF_SIZE - 1);

    /* copy_from_user() validate địa chỉ userspace, trả về số byte CHƯA copy */
    if (copy_from_user(kernel_buf, ubuf, bytes_to_copy)) {
        pr_err("mychar: copy_from_user failed\n");
        return -EFAULT;
    }

    kernel_buf[bytes_to_copy] = '\0';   /* null-terminate */
    buf_len = bytes_to_copy;
    pr_info("mychar: wrote %d bytes: %s\n", bytes_to_copy, kernel_buf);
    return bytes_to_copy;
}

/* ioctl commands */
#define MYCHAR_IOC_MAGIC   'M'
#define MYCHAR_IOC_RESET   _IO(MYCHAR_IOC_MAGIC,  0)     /* no arg */
#define MYCHAR_IOC_GET_LEN _IOR(MYCHAR_IOC_MAGIC, 1, int) /* read int */

static long mychar_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int len;
    switch (cmd) {
    case MYCHAR_IOC_RESET:
        memset(kernel_buf, 0, BUF_SIZE);
        buf_len = 0;
        pr_info("mychar: buffer reset\n");
        break;

    case MYCHAR_IOC_GET_LEN:
        len = buf_len;
        if (copy_to_user((int __user *)arg, &len, sizeof(len)))
            return -EFAULT;
        break;

    default:
        return -ENOTTY;   /* Inappropriate ioctl */
    }
    return 0;
}

/* Đăng ký các hàm xử lý vào struct file_operations */
static const struct file_operations mychar_fops = {
    .owner          = THIS_MODULE,
    .open           = mychar_open,
    .release        = mychar_release,
    .read           = mychar_read,
    .write          = mychar_write,
    .unlocked_ioctl = mychar_ioctl,
};

/* ──────────────────────────────────────────
 * Module Init / Exit
 * ────────────────────────────────────────── */

static int __init mychar_init(void)
{
    int ret;

    /* 1. Cấp phát major/minor number tự động */
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("mychar: alloc_chrdev_region failed: %d\n", ret);
        return ret;
    }
    pr_info("mychar: major=%d, minor=%d\n", MAJOR(dev_num), MINOR(dev_num));

    /* 2. Init cdev và liên kết với file_operations */
    cdev_init(&my_cdev, &mychar_fops);
    my_cdev.owner = THIS_MODULE;

    /* 3. Đăng ký cdev với kernel */
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("mychar: cdev_add failed: %d\n", ret);
        goto err_cdev;
    }

    /* 4. Tạo class (hiện trong /sys/class/myclass/) */
    my_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(my_class)) {
        pr_err("mychar: class_create failed\n");
        ret = PTR_ERR(my_class);
        goto err_class;
    }

    /* 5. Tạo device node /dev/mychar (udev tự động tạo file) */
    my_device = device_create(my_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(my_device)) {
        pr_err("mychar: device_create failed\n");
        ret = PTR_ERR(my_device);
        goto err_device;
    }

    pr_info("mychar: driver loaded, /dev/%s created\n", DEVICE_NAME);
    return 0;

err_device:
    class_destroy(my_class);
err_class:
    cdev_del(&my_cdev);
err_cdev:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void __exit mychar_exit(void)
{
    device_destroy(my_class, dev_num);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("mychar: driver unloaded\n");
}

module_init(mychar_init);
module_exit(mychar_exit);
```

---

## 5. Userspace Test Program

```c
/*
 * test_mychar.c - Test character device từ userspace
 */
#include <stdio.h>
#include <fcntl.h>       /* open */
#include <unistd.h>      /* read, write, close */
#include <sys/ioctl.h>
#include <string.h>

#define DEVICE "/dev/mychar"
#define MYCHAR_IOC_MAGIC   'M'
#define MYCHAR_IOC_RESET   _IO(MYCHAR_IOC_MAGIC,  0)
#define MYCHAR_IOC_GET_LEN _IOR(MYCHAR_IOC_MAGIC, 1, int)

int main(void)
{
    int fd, len;
    char write_buf[] = "Hello from BBB userspace!";
    char read_buf[256] = {0};

    /* Mở device */
    fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    /* Ghi vào device */
    write(fd, write_buf, strlen(write_buf));
    printf("Wrote: %s\n", write_buf);

    /* Lấy độ dài buffer qua ioctl */
    ioctl(fd, MYCHAR_IOC_GET_LEN, &len);
    printf("Buffer length (ioctl): %d\n", len);

    /* Reset file position về đầu */
    lseek(fd, 0, SEEK_SET);

    /* Đọc lại từ device */
    read(fd, read_buf, sizeof(read_buf));
    printf("Read back: %s\n", read_buf);

    /* Reset buffer qua ioctl */
    ioctl(fd, MYCHAR_IOC_RESET);
    printf("Buffer reset via ioctl\n");

    close(fd);
    return 0;
}
```

### Test từ shell

```bash
# Load driver
sudo insmod mychar_driver.ko

# Kiểm tra device đã tạo
ls -la /dev/mychar
# crw------- 1 root root 240, 0 ... /dev/mychar

# Cho phép user truy cập (hoặc chạy sudo)
sudo chmod 666 /dev/mychar

# Test bằng shell
echo "Hello BBB" > /dev/mychar
cat /dev/mychar
# Hello BBB

# Build và chạy test program
gcc test_mychar.c -o test_mychar
./test_mychar
```

---

## 6. copy_to_user vs copy_from_user

Đây là điểm **cực kỳ quan trọng** về security:

```
Userspace pointer        Kernel pointer
─────────────────────────────────────────
Có thể NULL
Có thể trỏ đến kernel  ← NGUY HIỂM!
Có thể không mapped
```

`copy_to_user()` và `copy_from_user()`:
1. **Validate** địa chỉ userspace (không nằm trong kernel space)
2. **Handle page fault** (page có thể chưa mapped)
3. Trả về số byte **chưa** copy (0 = thành công toàn bộ)

**KHÔNG BAO GIỜ** làm:
```c
*(int *)arg = value;           /* SRTICT KERNEL BUG - no validation */
memcpy(user_ptr, kernel_ptr, n); /* SECURITY HOLE - bypass check */
```

---

## 7. Câu hỏi kiểm tra

1. Tại sao cần `copy_to_user()`/`copy_from_user()` thay vì `memcpy()`?
2. `alloc_chrdev_region()` và `register_chrdev()` khác nhau thế nào?
3. Khi `open()` được gọi 2 lần lên cùng một device file, `mychar_open()` được gọi mấy lần?
4. `_IO`, `_IOR`, `_IOW`, `_IOWR` macro encode thông tin gì?
5. Nếu `module_exit` không gọi `device_destroy()` và `class_destroy()`, hậu quả là gì?

---

## 8. Tài liệu tham khảo

| Nội dung | Header | Ghi chú |
|----------|--------|---------|
| cdev API | linux/cdev.h | cdev_init, cdev_add, cdev_del |
| file_operations | linux/fs.h | open, read, write, ioctl |
| user/kernel copy | linux/uaccess.h | copy_to/from_user |
| device class | linux/device.h | class_create, device_create |
| ioctl macros | linux/ioctl.h | _IO, _IOR, _IOW, _IOWR |
