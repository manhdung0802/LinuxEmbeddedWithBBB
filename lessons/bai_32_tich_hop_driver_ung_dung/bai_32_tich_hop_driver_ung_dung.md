# Bài 32 - Tích Hợp Driver & Ứng Dụng vào Yocto Image

## 1. Mục tiêu bài học

- Hiểu quy trình hoàn chỉnh từ code → Yocto image
- Đưa kernel driver (module .ko) tự viết vào Yocto
- Đưa ứng dụng userspace C vào image cùng driver
- Viết systemd service tự load module và chạy ứng dụng
- Tạo device node tự động qua udev rule
- Toàn bộ integration test trên BBB thực tế

---

## 2. Tổng Quan Luồng Tích Hợp

```
Bạn có:
  drivers/my_gpio_drv/
    my_gpio_drv.c        ← Kernel module (char device)
  apps/gpio_control/
    gpio_control.c       ← Userspace app dùng driver
    gpio_control.service ← Systemd service

Mục tiêu:
  Sau bitbake my-bbb-image:
    /lib/modules/<kernel_ver>/extra/my_gpio_drv.ko  ← auto-load
    /usr/bin/gpio_control                            ← running
    /dev/my_gpio0                                    ← device node
    systemd: gpio-control.service = active (running)
```

---

## 3. Ví Dụ: Driver GPIO Character Device

### 3.1 Kernel module: my_gpio_drv.c

```c
/* my_gpio_drv.c
 * GPIO character device driver cho BBB
 * Cho phép userspace đọc/ghi GPIO qua /dev/my_gpio0
 *
 * User interface:
 *   write(fd, "1", 1)  → set GPIO high
 *   write(fd, "0", 1)  → set GPIO low
 *   read(fd, buf, 1)   → đọc state hiện tại
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

#define DRIVER_NAME  "my_gpio_drv"
#define DEVICE_NAME  "my_gpio"
#define CLASS_NAME   "bbb_gpio"

/* GPIO60 = P9.12 trên BBB (GPIO1_28) */
#define GPIO_PIN     60

static int    major_num;
static struct class  *gpio_class;
static struct cdev    gpio_cdev;
static dev_t          gpio_dev;

static int gpio_drv_open(struct inode *inode, struct file *file)
{
    pr_info("%s: opened\n", DRIVER_NAME);
    return 0;
}

static int gpio_drv_release(struct inode *inode, struct file *file)
{
    pr_info("%s: closed\n", DRIVER_NAME);
    return 0;
}

static ssize_t gpio_drv_read(struct file *file,
                              char __user *buf, size_t len, loff_t *off)
{
    char state;
    int val = gpio_get_value(GPIO_PIN);

    state = val ? '1' : '0';

    if (*off >= 1)
        return 0;   /* EOF */

    if (copy_to_user(buf, &state, 1))
        return -EFAULT;

    *off += 1;
    return 1;
}

static ssize_t gpio_drv_write(struct file *file,
                               const char __user *buf, size_t len, loff_t *off)
{
    char kbuf[2] = {0};

    if (len > 1)
        len = 1;

    if (copy_from_user(kbuf, buf, len))
        return -EFAULT;

    if (kbuf[0] == '1') {
        gpio_set_value(GPIO_PIN, 1);
        pr_info("%s: GPIO%d → HIGH\n", DRIVER_NAME, GPIO_PIN);
    } else if (kbuf[0] == '0') {
        gpio_set_value(GPIO_PIN, 0);
        pr_info("%s: GPIO%d → LOW\n", DRIVER_NAME, GPIO_PIN);
    } else {
        return -EINVAL;
    }

    return len;
}

static const struct file_operations gpio_fops = {
    .owner   = THIS_MODULE,
    .open    = gpio_drv_open,
    .release = gpio_drv_release,
    .read    = gpio_drv_read,
    .write   = gpio_drv_write,
};

static int __init gpio_drv_init(void)
{
    int ret;

    pr_info("%s: initializing\n", DRIVER_NAME);

    /* Yêu cầu GPIO pin */
    ret = gpio_request(GPIO_PIN, "my_gpio_drv");
    if (ret) {
        pr_err("%s: gpio_request(%d) failed: %d\n",
               DRIVER_NAME, GPIO_PIN, ret);
        return ret;
    }
    gpio_direction_output(GPIO_PIN, 0);

    /* Cấp phát major/minor numbers */
    ret = alloc_chrdev_region(&gpio_dev, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("%s: alloc_chrdev_region failed: %d\n", DRIVER_NAME, ret);
        goto err_gpio;
    }
    major_num = MAJOR(gpio_dev);

    /* Tạo class (xuất hiện trong /sys/class/) */
    gpio_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(gpio_class)) {
        ret = PTR_ERR(gpio_class);
        goto err_chrdev;
    }

    /* Tạo device → udev tự tạo /dev/my_gpio0 */
    if (IS_ERR(device_create(gpio_class, NULL, gpio_dev, NULL, DEVICE_NAME "0"))) {
        ret = -ENOMEM;
        goto err_class;
    }

    /* Khởi tạo và đăng ký cdev */
    cdev_init(&gpio_cdev, &gpio_fops);
    gpio_cdev.owner = THIS_MODULE;
    ret = cdev_add(&gpio_cdev, gpio_dev, 1);
    if (ret < 0) {
        pr_err("%s: cdev_add failed: %d\n", DRIVER_NAME, ret);
        goto err_device;
    }

    pr_info("%s: loaded, /dev/%s0 (major=%d)\n",
            DRIVER_NAME, DEVICE_NAME, major_num);
    return 0;

err_device:
    device_destroy(gpio_class, gpio_dev);
err_class:
    class_destroy(gpio_class);
err_chrdev:
    unregister_chrdev_region(gpio_dev, 1);
err_gpio:
    gpio_free(GPIO_PIN);
    return ret;
}

static void __exit gpio_drv_exit(void)
{
    cdev_del(&gpio_cdev);
    device_destroy(gpio_class, gpio_dev);
    class_destroy(gpio_class);
    unregister_chrdev_region(gpio_dev, 1);
    gpio_free(GPIO_PIN);
    pr_info("%s: unloaded\n", DRIVER_NAME);
}

module_init(gpio_drv_init);
module_exit(gpio_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BBB Student");
MODULE_DESCRIPTION("GPIO character device driver for BeagleBone Black");
MODULE_VERSION("1.0");
```

### Giải thích chi tiết từng dòng code (my_gpio_drv.c)

a) **`#include <linux/gpio.h>`**:
- Legacy GPIO API: `gpio_request()`, `gpio_direction_output()`, `gpio_get_value()`, `gpio_set_value()`.
- **Lưu ý**: API này đã deprecated, kernel mới dùng `gpiod_*` (GPIO descriptor API). Tuy nhiên, `gpio_*` vẫn hoạt động và dễ học hơn.

b) **`#define GPIO_PIN 60`**:
- GPIO60 = GPIO1_28 (bank 1 × 32 + 28 = 60) = pin P9.12 trên BBB.
- Dùng số **Linux GPIO** (không phải số pin vật lý). Tính: `bank * 32 + pin_within_bank`.

c) **`gpio_request(GPIO_PIN, "my_gpio_drv")`**:
- Đăng ký quyền sử dụng GPIO pin với kernel. Nếu pin đang bị driver khác dùng → trả về lỗi.
- Tham số 2 là label (hiện trong `/sys/kernel/debug/gpio`).

d) **Trình tự init: GPIO → chrdev → class → device → cdev**:
- Tương tự bài 18 (mychar_driver.c), nhưng thêm bước `gpio_request` ở đầu.
- Error handling dùng `goto` với **reverse cleanup** (giống bài 18).

e) **`gpio_drv_read()` và `gpio_drv_write()`**:
- `gpio_get_value(GPIO_PIN)` — đọc trạng thái pin (0 hoặc 1). Trả về character `'0'` hoặc `'1'` cho userspace.
- `gpio_set_value(GPIO_PIN, 1)` — set pin HIGH. Chỉ chấp nhận `'0'` hoặc `'1'`, giá trị khác trả `-EINVAL`.
- Vẫn dùng `copy_to_user()` / `copy_from_user()` như bài 18.

f) **`gpio_free(GPIO_PIN)` trong exit**:
- **Bắt buộc** giải phóng GPIO khi module unload. Nếu không, pin bị "khóa" — không driver nào khác dùng được cho đến khi reboot.
- Cleanup ngược thứ tự: `cdev_del → device_destroy → class_destroy → unregister_chrdev → gpio_free`.

> **Bài học**: Driver này kết hợp kiến thức từ nhiều bài: char device (bài 18), GPIO registers (bài 5/17), error handling với goto (bài 18), và kernel module API (bài 17). Khi tích hợp vào Yocto, chỉ cần `inherit module` và `KERNEL_MODULE_AUTOLOAD` để kernel tự load.

### 3.2 Makefile cho module

```makefile
# Makefile cho kernel module
# Sử dụng khi build ngoài Yocto (trên board trực tiếp)
KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build
PWD  := $(shell pwd)

obj-m := my_gpio_drv.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a
```

---

## 4. Ứng Dụng Userspace: gpio_control.c

```c
/* gpio_control.c
 * Điều khiển GPIO qua /dev/my_gpio0
 * Sử dụng: gpio_control <on|off|read>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define DEV_PATH  "/dev/my_gpio0"

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <on|off|read>\n", prog);
    fprintf(stderr, "  on   - Set GPIO high\n");
    fprintf(stderr, "  off  - Set GPIO low\n");
    fprintf(stderr, "  read - Read current state\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    int fd;
    char buf[4];

    if (argc != 2)
        usage(argv[0]);

    fd = open(DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open " DEV_PATH);
        fprintf(stderr, "Lỗi: Driver my_gpio_drv chưa được load?\n");
        fprintf(stderr, "Thử: modprobe my_gpio_drv\n");
        return 1;
    }

    if (strcmp(argv[1], "on") == 0) {
        write(fd, "1", 1);
        printf("GPIO → HIGH\n");
    } else if (strcmp(argv[1], "off") == 0) {
        write(fd, "0", 1);
        printf("GPIO → LOW\n");
    } else if (strcmp(argv[1], "read") == 0) {
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("GPIO state: %s\n", buf[0] == '1' ? "HIGH" : "LOW");
        }
    } else {
        usage(argv[0]);
    }

    close(fd);
    return 0;
}
```

### Giải thích chi tiết từng dòng code (gpio_control.c)

a) **`open(DEV_PATH, O_RDWR)`**:
- Mở `/dev/my_gpio0` — device node do driver tạo (qua `device_create` trong `my_gpio_drv.c`).
- Nếu driver chưa load, `open()` trả `-1` với `errno = ENOENT` (No such file or directory).
- khi `open()` thành công, kernel gọi `gpio_drv_open()` trong driver.

b) **`write(fd, "1", 1)` và `read(fd, buf, ...)`**:
- `write` → kernel gọi `gpio_drv_write()` → `gpio_set_value()`.
- `read` → kernel gọi `gpio_drv_read()` → `gpio_get_value()`.
- Đây là **luồng hoàn chỉnh**: userspace `write()` → VFS → `file_operations.write` → driver code → GPIO hardware.

c) **Error message hướng dẫn**:
```c
fprintf(stderr, "Lỗi: Driver my_gpio_drv chưa được load?\n");
fprintf(stderr, "Thử: modprobe my_gpio_drv\n");
```
- User-friendly error khi driver chưa load — quan trọng trong embedded vì debug khó hơn.

> **Bài học**: Đây là **full-stack embedded**: DTS (bài 16/25) → kernel driver (bài 17/18) → userspace app (bài 23) → Yocto recipe (bài 29) → production image (bài 30). Tất cả kết nối lại thành workflow hoàn chỉnh.

---

## 5. Recipe cho Kernel Module

```
meta-bbb-custom/
└── recipes-kernel/
    └── my-gpio-driver/
        ├── my-gpio-driver_1.0.bb
        └── files/
            ├── my_gpio_drv.c
            └── Makefile
```

```bitbake
# my-gpio-driver_1.0.bb — Kernel module recipe
SUMMARY = "GPIO character device driver for BeagleBone Black"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

inherit module    # ← class đặc biệt cho kernel modules

SRC_URI = "file://my_gpio_drv.c \
           file://Makefile"
S = "${WORKDIR}"

# Module sẽ được cài vào:
# ${D}/lib/modules/${KERNEL_VERSION}/extra/my_gpio_drv.ko

# Kernel version phải khớp với kernel trong image
KERNEL_VERSION = "${@d.getVar('KERNEL_VERSION') or d.getVar('PKGV')}"

# Tự động load module khi boot
# Tạo file /etc/modules-load.d/my_gpio_drv.conf
KERNEL_MODULE_AUTOLOAD += "my_gpio_drv"
# Hoặc tạo thủ công:
# do_install:append() {
#     install -d ${D}${sysconfdir}/modules-load.d
#     echo "my_gpio_drv" > ${D}${sysconfdir}/modules-load.d/my_gpio_drv.conf
# }
```

---

## 6. Recipe cho Userspace Application

```
meta-bbb-custom/
└── recipes-bbb/
    └── gpio-control/
        ├── gpio-control_1.0.bb
        └── files/
            ├── gpio_control.c
            └── gpio-control.service
```

```bitbake
# gpio-control_1.0.bb
SUMMARY = "GPIO control application using my_gpio_drv"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://gpio_control.c \
           file://gpio-control.service"
S = "${WORKDIR}"

# Phụ thuộc vào kernel module (phải có trên board khi chạy)
RDEPENDS:${PN} = "kernel-module-my-gpio-drv"
# Tên package của kernel module: kernel-module-<tên với - thay _>

inherit systemd
SYSTEMD_SERVICE:${PN} = "gpio-control.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} gpio_control.c -o gpio_control
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 gpio_control ${D}${bindir}/gpio_control

    install -d ${D}${systemd_unitdir}/system
    install -m 0644 ${WORKDIR}/gpio-control.service \
        ${D}${systemd_unitdir}/system/gpio-control.service
}
```

### 6.1 gpio-control.service

```ini
[Unit]
Description=GPIO Control Service for BBB
# Đợi kernel module được load
After=systemd-modules-load.service
Requires=systemd-modules-load.service

[Service]
Type=oneshot
RemainAfterExit=yes
# Bật GPIO khi boot (ví dụ: status LED)
ExecStart=/usr/bin/gpio_control on
ExecStop=/usr/bin/gpio_control off

[Install]
WantedBy=multi-user.target
```

---

## 7. udev Rule — Tự Động Tạo Device Node

Thay vì cứng số `my_gpio0`, dùng udev rule để đặt tên chuẩn:

```
meta-bbb-custom/
└── recipes-bbb/
    └── gpio-control/
        └── files/
            └── 99-my-gpio.rules     ← udev rule
```

```
# 99-my-gpio.rules
# Tự động đặt permissions khi /dev/my_gpio0 xuất hiện
KERNEL=="my_gpio0", MODE="0666", GROUP="gpio"
# MODE="0666" → mọi user đọc/ghi được (không cần sudo)
# GROUP="gpio" → cần tạo group gpio trên rootfs
```

```bitbake
# Trong gpio-control_1.0.bb — thêm udev rule
SRC_URI:append = " file://99-my-gpio.rules"

do_install:append() {
    install -d ${D}${sysconfdir}/udev/rules.d
    install -m 0644 ${WORKDIR}/99-my-gpio.rules \
        ${D}${sysconfdir}/udev/rules.d/99-my-gpio.rules
}
```

---

## 8. Tổng Hợp: Thêm vào Image Recipe

```bitbake
# my-bbb-image.bb
require recipes-core/images/core-image-minimal.bb

SUMMARY = "BBB image with GPIO driver"
IMAGE_FEATURES += "ssh-server-openssh"

IMAGE_INSTALL:append = " \
    my-gpio-driver \
    gpio-control \
    kernel-modules \
    i2c-tools \
    "
# kernel-modules: cài TẤT CẢ kernel modules (an toàn nhưng lớn)
# Hoặc chỉ cài module cụ thể: kernel-module-my-gpio-drv
```

---

## 9. Kiểm Tra Trên Board

```bash
# Sau khi boot BBB với image mới

# 1. Kiểm tra module đã load chưa
lsmod | grep my_gpio
# my_gpio_drv  4096  0

dmesg | grep my_gpio_drv
# [  2.345] my_gpio_drv: initializing
# [  2.346] my_gpio_drv: loaded, /dev/my_gpio0 (major=247)

# 2. Kiểm tra device node
ls -la /dev/my_gpio0
# crw-rw-rw- 1 root gpio 247, 0 Jan  1 00:00 /dev/my_gpio0

# 3. Test ứng dụng
gpio_control on
# GPIO → HIGH

gpio_control read
# GPIO state: HIGH

gpio_control off
# GPIO → LOW

# 4. Kiểm tra systemd service
systemctl status gpio-control
# ● gpio-control.service - GPIO Control Service for BBB
#    Loaded: loaded (/lib/systemd/system/gpio-control.service; enabled)
#    Active: active (exited) since ...

# 5. Đọc trực tiếp qua /dev
echo "1" > /dev/my_gpio0    # Set HIGH
cat /dev/my_gpio0            # Đọc state: "1"
```

---

## 10. Debug Khi Module Không Load

```bash
# Kiểm tra module có trong image
find /lib/modules -name "my_gpio_drv.ko"
# /lib/modules/5.10.168-ti/extra/my_gpio_drv.ko

# Thử load thủ công
modprobe my_gpio_drv
# hoặc
insmod /lib/modules/$(uname -r)/extra/my_gpio_drv.ko

# Nếu lỗi: Module version mismatch
uname -r                    # Kernel version đang chạy
modinfo my_gpio_drv.ko      # Module được build cho kernel nào

# Rebuild module đúng kernel version
# → Trong Yocto: bitbake my-gpio-driver (phải đúng MACHINE và kernel)

# Kiểm tra depmod
depmod -a                   # Rebuild module dependency
modinfo my_gpio_drv         # Xem info module
```

---

## 11. Quy Trình Lặp Phát Triển (dev loop)

```bash
# ── Trên máy build ──────────────────────────────────────────────

# 1. Sửa code trong meta-bbb-custom/recipes-xxx/xxx/files/

# 2. Dùng devtool để iterate nhanh
devtool modify my-gpio-driver    # Checkout vào workspace
# Sửa file trong workspace/sources/my-gpio-driver/
devtool build my-gpio-driver
devtool deploy-target my-gpio-driver root@192.168.7.2

# 3. Trên board, test:
ssh root@192.168.7.2
rmmod my_gpio_drv               # Unload module cũ
insmod /lib/modules/.../extra/my_gpio_drv.ko  # Load mới
gpio_control on && gpio_control read

# 4. Nếu ổn, finalize:
devtool finish my-gpio-driver meta-bbb-custom

# 5. Build image đầy đủ và flash
bitbake my-bbb-image
sudo bmaptool copy *.wic /dev/sdX
```

---

## 12. Câu Hỏi Kiểm Tra

1. `inherit module` trong recipe kernel module làm gì? Tasks nào được tự thêm?
2. `RDEPENDS:${PN} = "kernel-module-my-gpio-drv"` — tên package kernel module được tạo theo quy tắc nào?
3. `KERNEL_MODULE_AUTOLOAD += "my_gpio_drv"` tạo file gì ở đâu trong rootfs?
4. Tại sao cần `After=systemd-modules-load.service` trong systemd unit của ứng dụng?
5. udev rule `MODE="0666"` có ý nghĩa gì? Khi nào nên dùng `MODE="0660"` + `GROUP="gpio"` thay vì?
6. Nếu module build thành công nhưng `modprobe` báo "version magic mismatch", nguyên nhân là gì?
