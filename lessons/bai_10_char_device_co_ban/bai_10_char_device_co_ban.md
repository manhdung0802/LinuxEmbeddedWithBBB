# Bài 10 - Character Device Driver co b?n

## 1. M?c tiêu bài h?c
- Hi?u character device là gì trong Linux
- Phân bi?t major/minor number
- Dùng `alloc_chrdev_region()`, `cdev_add()` d? dang ký char device
- Implement `file_operations`: open, release, read, write
- T?o `/dev` node t? d?ng v?i `class_create()` / `device_create()`

---

## 2. Character Device là gì?

Trong Linux, m?i th? d?u là file. Device (thi?t b?) cung du?c bi?u di?n du?i d?ng file trong `/dev/`.

```
/dev/ttyS0    ? Serial port (character device)
/dev/sda      ? Hard disk (block device)
/dev/video0   ? Camera (character device)
/dev/myled    ? Driver b?n vi?t (character device)
```

**Character device**: Ð?c/ghi **tu?n t?**, t?ng byte ho?c t?ng block nh?.
**Block device**: Ð?c/ghi theo **block c? d?nh** (512B, 4KB), có buffer cache.

H?u h?t driver ph?n c?ng embedded dùng **character device**.

### 2.1 Userspace giao ti?p v?i driver:

```c
/* Userspace code */
int fd = open("/dev/myled", O_RDWR);    /* ? driver's .open */
write(fd, "1", 1);                       /* ? driver's .write */
read(fd, buf, sizeof(buf));              /* ? driver's .read */
close(fd);                               /* ? driver's .release */
```

```
Userspace              Kernel
---------              ------
open("/dev/myled") --? my_open()
write(fd, buf, n)  --? my_write()
read(fd, buf, n)   --? my_read()
ioctl(fd, cmd, arg)--? my_ioctl()
close(fd)          --? my_release()
```

---

## 3. Major / Minor Number

M?i device file có 2 s?:
- **Major number**: Xác d?nh **driver** nào x? lý
- **Minor number**: Xác d?nh **thi?t b? c? th?** trong driver dó

```bash
ls -l /dev/ttyS*
# crw-rw---- 1 root dialout 4, 64 ... /dev/ttyS0
# crw-rw---- 1 root dialout 4, 65 ... /dev/ttyS1
#                            ^  ^^
#                         major  minor
```

### 3.1 C?p phát dynamic:

```c
#include <linux/fs.h>

dev_t dev_num;
int major;

/* Kernel t? ch?n major number r?nh */
alloc_chrdev_region(&dev_num, 0, 1, "mydevice");
/*                   ?       ?  ?    ?
                   output  first count name  */

major = MAJOR(dev_num);
pr_info("Major = %d\n", major);

/* Gi?i phóng khi exit */
unregister_chrdev_region(dev_num, 1);
```

### 3.2 Macro ti?n ích:

```c
MAJOR(dev_num)       /* L?y major t? dev_t */
MINOR(dev_num)       /* L?y minor t? dev_t */
MKDEV(major, minor)  /* T?o dev_t t? major + minor */
```

---

## 4. `struct cdev` và `file_operations`

### 4.1 cdev - C?u trúc character device:

```c
#include <linux/cdev.h>

struct cdev my_cdev;

/* Kh?i t?o cdev và gán file_operations */
cdev_init(&my_cdev, &my_fops);
my_cdev.owner = THIS_MODULE;

/* Ðang ký cdev vào kernel */
cdev_add(&my_cdev, dev_num, 1);
/*                  ?        ?
                  dev_t    count  */

/* G? khi exit */
cdev_del(&my_cdev);
```

### 4.2 file_operations - B?ng hàm x? lý:

```c
#include <linux/fs.h>

static int my_open(struct inode *inode, struct file *file);
static int my_release(struct inode *inode, struct file *file);
static ssize_t my_read(struct file *file, char __user *buf,
                        size_t len, loff_t *offset);
static ssize_t my_write(struct file *file, const char __user *buf,
                         size_t len, loff_t *offset);

static const struct file_operations my_fops = {
    .owner   = THIS_MODULE,
    .open    = my_open,
    .release = my_release,
    .read    = my_read,
    .write   = my_write,
};
```

---

## 5. Implement file_operations

### 5.1 open / release:

```c
static int my_open(struct inode *inode, struct file *file)
{
    pr_info("Device opened\n");
    return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
    pr_info("Device closed\n");
    return 0;
}
```

### 5.2 read - G?i data t? kernel ? userspace:

```c
static char kernel_buf[256] = "Hello from kernel!\n";
static int buf_len = 19;

static ssize_t my_read(struct file *file, char __user *ubuf,
                        size_t count, loff_t *offset)
{
    int bytes_to_read;

    /* Ki?m tra dã d?c h?t chua */
    if (*offset >= buf_len)
        return 0;   /* EOF */

    bytes_to_read = min((int)count, buf_len - (int)*offset);

    /* Copy t? kernel space ? user space */
    if (copy_to_user(ubuf, kernel_buf + *offset, bytes_to_read))
        return -EFAULT;

    *offset += bytes_to_read;
    return bytes_to_read;
}
```

### 5.3 write - Nh?n data t? userspace ? kernel:

```c
static ssize_t my_write(struct file *file, const char __user *ubuf,
                         size_t count, loff_t *offset)
{
    int bytes_to_write;

    bytes_to_write = min((int)count, (int)sizeof(kernel_buf) - 1);

    /* Copy t? user space ? kernel space */
    if (copy_from_user(kernel_buf, ubuf, bytes_to_write))
        return -EFAULT;

    kernel_buf[bytes_to_write] = '\0';
    buf_len = bytes_to_write;
    *offset += bytes_to_write;

    pr_info("Received %d bytes: %s\n", bytes_to_write, kernel_buf);
    return bytes_to_write;
}
```

### 5.4 `copy_to_user` / `copy_from_user`:

```c
/* KHÔNG BAO GI? d?c/ghi tr?c ti?p user pointer trong kernel! */

/* SAI - kernel panic ho?c security hole */
memcpy(kernel_buf, user_buf, len);

/* ÐÚNG - ki?m tra address h?p l?, x? lý page fault */
if (copy_from_user(kernel_buf, user_buf, len))
    return -EFAULT;   /* Address không h?p l? */
```

---

## 6. T?o `/dev` node t? d?ng

### 6.1 Cách th? công (không khuyên):
```bash
sudo mknod /dev/mydevice c 240 0
```

### 6.2 Cách t? d?ng v?i `class_create` + `device_create`:

```c
#include <linux/device.h>

static struct class *my_class;
static struct device *my_device;

/* Trong init */
my_class = class_create("my_class");
if (IS_ERR(my_class)) {
    pr_err("class_create failed\n");
    /* cleanup... */
    return PTR_ERR(my_class);
}

my_device = device_create(my_class, NULL, dev_num, NULL, "mydevice");
if (IS_ERR(my_device)) {
    pr_err("device_create failed\n");
    /* cleanup... */
    return PTR_ERR(my_device);
}
/* Bây gi? /dev/mydevice t? d?ng xu?t hi?n */

/* Trong exit */
device_destroy(my_class, dev_num);
class_destroy(my_class);
```

---

## 7. Ví d? hoàn ch?nh: Simple Char Device

```c
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "simplechar"
#define BUF_SIZE    256

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *my_class;
static struct device *my_device;

static char device_buf[BUF_SIZE];
static int data_len;

static int sc_open(struct inode *inode, struct file *file)
{
	pr_info("simplechar: opened\n");
	return 0;
}

static int sc_release(struct inode *inode, struct file *file)
{
	pr_info("simplechar: closed\n");
	return 0;
}

static ssize_t sc_read(struct file *file, char __user *buf,
                        size_t count, loff_t *offset)
{
	int to_read;

	if (*offset >= data_len)
		return 0;

	to_read = min((int)count, data_len - (int)*offset);
	if (copy_to_user(buf, device_buf + *offset, to_read))
		return -EFAULT;

	*offset += to_read;
	return to_read;
}

static ssize_t sc_write(struct file *file, const char __user *buf,
                         size_t count, loff_t *offset)
{
	int to_write;

	to_write = min((int)count, BUF_SIZE - 1);
	if (copy_from_user(device_buf, buf, to_write))
		return -EFAULT;

	device_buf[to_write] = '\0';
	data_len = to_write;
	pr_info("simplechar: wrote %d bytes\n", to_write);
	return to_write;
}

static const struct file_operations sc_fops = {
	.owner   = THIS_MODULE,
	.open    = sc_open,
	.release = sc_release,
	.read    = sc_read,
	.write   = sc_write,
};

static int __init sc_init(void)
{
	int ret;

	/* 1. C?p phát major/minor */
	ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	if (ret < 0) {
		pr_err("alloc_chrdev_region failed\n");
		return ret;
	}

	/* 2. Kh?i t?o và dang ký cdev */
	cdev_init(&my_cdev, &sc_fops);
	my_cdev.owner = THIS_MODULE;
	ret = cdev_add(&my_cdev, dev_num, 1);
	if (ret < 0)
		goto err_cdev;

	/* 3. T?o class và device ? /dev/simplechar t? d?ng */
	my_class = class_create(DEVICE_NAME);
	if (IS_ERR(my_class)) {
		ret = PTR_ERR(my_class);
		goto err_class;
	}

	my_device = device_create(my_class, NULL, dev_num, NULL, DEVICE_NAME);
	if (IS_ERR(my_device)) {
		ret = PTR_ERR(my_device);
		goto err_device;
	}

	pr_info("simplechar: registered major=%d minor=%d\n",
	        MAJOR(dev_num), MINOR(dev_num));
	return 0;

err_device:
	class_destroy(my_class);
err_class:
	cdev_del(&my_cdev);
err_cdev:
	unregister_chrdev_region(dev_num, 1);
	return ret;
}

static void __exit sc_exit(void)
{
	device_destroy(my_class, dev_num);
	class_destroy(my_class);
	cdev_del(&my_cdev);
	unregister_chrdev_region(dev_num, 1);
	pr_info("simplechar: unregistered\n");
}

module_init(sc_init);
module_exit(sc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple character device driver");
```

### 7.1 Test trên BBB:

```bash
sudo insmod simplechar.ko
# [xxx] simplechar: registered major=240 minor=0

ls -l /dev/simplechar
# crw------- 1 root root 240, 0 ... /dev/simplechar

# Ghi data
echo "Hello BBB" > /dev/simplechar

# Ð?c data
cat /dev/simplechar
# Hello BBB

sudo rmmod simplechar
```

---

## 8. Error Handling Pattern (goto cleanup)

```c
static int __init my_init(void)
{
    /* Bu?c 1 */
    ret = alloc_chrdev_region(...);
    if (ret) return ret;

    /* Bu?c 2 */
    ret = cdev_add(...);
    if (ret) goto err_step1;

    /* Bu?c 3 */
    my_class = class_create(...);
    if (IS_ERR(my_class)) {
        ret = PTR_ERR(my_class);
        goto err_step2;
    }

    return 0;

err_step2:
    cdev_del(&my_cdev);
err_step1:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

/* Exit: cleanup ngu?c th? t? init */
static void __exit my_exit(void)
{
    /* Bu?c 3 ngu?c */
    class_destroy(my_class);
    /* Bu?c 2 ngu?c */
    cdev_del(&my_cdev);
    /* Bu?c 1 ngu?c */
    unregister_chrdev_region(dev_num, 1);
}
```

**Quy t?c**: Cleanup **ngu?c th? t?** init. Dùng `goto` label, **không** l?ng if.

---

## 9. Ki?n th?c c?t lõi sau bài này

1. **Character device** = file `/dev/xxx` + `file_operations` trong driver
2. **Major/minor** = kernel dùng d? dispatch syscall t?i dúng driver
3. **`alloc_chrdev_region`** ? **`cdev_init`** ? **`cdev_add`** = dang ký driver
4. **`copy_to_user`/`copy_from_user`** = transfer data an toàn kernel?userspace
5. **`class_create`** + **`device_create`** = t?o `/dev` node t? d?ng
6. **Goto cleanup** pattern = cách chu?n x? lý l?i trong kernel

---

## 10. Câu h?i ki?m tra

1. Major number và minor number khác nhau ? di?m nào?
2. T?i sao ph?i dùng `copy_to_user()` thay vì `memcpy()` khi g?i data cho userspace?
3. `file_operations` struct ch?a gì? T?i sao c?n `.owner = THIS_MODULE`?
4. V? flow: userspace g?i `write(fd, "on", 2)` ? kernel x? lý ? driver nh?n du?c gì?
5. Khi `cdev_add()` fail, nh?ng resource nào c?n cleanup?

---

## 11. Chu?n b? cho bài sau

Bài ti?p theo: **Bài 8 - Character Device Driver nâng cao**

Ta s? h?c `ioctl`, `private_data`, `read/write` buffer nâng cao, và cách driver qu?n lý per-device state.
