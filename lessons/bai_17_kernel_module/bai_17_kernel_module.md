# Bài 17 - Linux Kernel Module: Viết Driver Cơ Bản

## 1. Mục tiêu bài học

- Hiểu kiến trúc kernel module (LKM - Loadable Kernel Module)
- Viết module đầu tiên: hello world trong kernel space
- Nắm cơ chế `module_init` / `module_exit`
- Sử dụng `printk` và đọc log bằng `dmesg`
- Build module out-of-tree với Makefile chuẩn
- Truyền tham số vào module khi load

---

## 2. Kernel Module là gì?

**Kernel Module (LKM)** là một đoạn code có thể **load vào** và **unload khỏi** kernel đang chạy — không cần reboot, không cần compile lại toàn bộ kernel.

```
Userspace  │  Kernel Space
───────────┼─────────────────────────────
app        │  kernel core
           │      ↕ module interface
           │  [module.ko] ← động, load/unload bất kỳ lúc
```

### So sánh kernel module vs userspace

| | Kernel Module | Userspace App |
|---|---|---|
| Chạy ở đâu | Ring 0 (privileged) | Ring 3 (user) |
| Truy cập HW | Trực tiếp | Qua syscall / /dev |
| Lỗi ảnh hưởng | Toàn hệ thống (kernel panic) | Chỉ process đó |
| Debug | Khó hơn, dùng printk | Dùng gdb, strace |
| Standard lib | Không có libc | Có toàn bộ libc |

---

## 3. Cấu trúc Module Cơ Bản

### hello_module.c

```c
/*
 * hello_module.c - Module đầu tiên cho BBB
 */
#include <linux/module.h>    /* module_init, module_exit, MODULE_* */
#include <linux/kernel.h>    /* printk, KERN_INFO */
#include <linux/init.h>      /* __init, __exit */

/* Metadata của module */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("BBB Student");
MODULE_DESCRIPTION("Hello World Kernel Module");
MODULE_VERSION("1.0");

/*
 * Hàm init - gọi khi insmod
 * __init: kernel có thể giải phóng vùng nhớ này sau khi init xong
 */
static int __init hello_init(void)
{
    printk(KERN_INFO "hello_module: Loaded! Running on BBB AM335x\n");
    return 0;  /* 0 = success, < 0 = error code */
}

/*
 * Hàm exit - gọi khi rmmod
 * __exit: chỉ dùng cho cleanup, kernel biết không cần built-in
 */
static void __exit hello_exit(void)
{
    printk(KERN_INFO "hello_module: Unloaded!\n");
}

/* Đăng ký hàm init và exit với kernel */
module_init(hello_init);
module_exit(hello_exit);
```

---

## 4. Makefile Build Out-of-Tree

### Makefile

```makefile
# Tên object file (tên module sẽ là hello_module.ko)
obj-m += hello_module.o

# Đường dẫn đến kernel headers (trên BBB)
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
```

### Build và chạy trên BBB

```bash
# Build module
make

# Kết quả: hello_module.ko được tạo
ls -la *.ko

# Load module
sudo insmod hello_module.ko

# Xem log
dmesg | tail -5
# [12345.678] hello_module: Loaded! Running on BBB AM335x

# Xem danh sách module đang chạy
lsmod | grep hello
# hello_module    16384  0

# Xem thông tin module
modinfo hello_module.ko
# filename: /path/to/hello_module.ko
# description: Hello World Kernel Module
# author: BBB Student
# license: GPL
# version: 1.0

# Unload module
sudo rmmod hello_module

# Kiểm tra log unload
dmesg | tail -3
# [12350.123] hello_module: Unloaded!
```

---

## 5. printk Log Levels

`printk` khác với `printf` — nó ghi vào kernel ring buffer, đọc qua `dmesg`.

```c
printk(KERN_EMERG   "Emergency: system unusable\n");   /* 0 */
printk(KERN_ALERT   "Alert: action must be taken\n");  /* 1 */
printk(KERN_CRIT    "Critical condition\n");           /* 2 */
printk(KERN_ERR     "Error condition\n");              /* 3 */
printk(KERN_WARNING "Warning condition\n");            /* 4 */
printk(KERN_NOTICE  "Normal but significant\n");       /* 5 */
printk(KERN_INFO    "Informational\n");                /* 6 */
printk(KERN_DEBUG   "Debug-level message\n");          /* 7 */

/* Shorthand macros (thường dùng hơn) */
pr_err("Something went wrong: %d\n", ret);
pr_info("Module loaded, version %s\n", version);
pr_warn("This is deprecated\n");
pr_debug("Debug value: %x\n", val);   /* chỉ in nếu DEBUG defined */
```

### Xem và lọc dmesg

```bash
dmesg                        # toàn bộ kernel log
dmesg | tail -20             # 20 dòng cuối
dmesg -w                     # watch live (như tail -f)
dmesg -l err,warn            # chỉ lỗi và warning
dmesg -c                     # đọc rồi xóa buffer
dmesg --level=debug          # include debug messages

# Xem console log level hiện tại
cat /proc/sys/kernel/printk
# 7  4  1  7
# ↑  ↑  ↑  ↑
# current  default  min  boot-time default

# Tạm thời hiện TẤT CẢ message (kể cả debug)
echo 8 > /proc/sys/kernel/printk
```

---

## 6. Module Parameters

Truyền tham số vào module khi load:

```c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>

/* Khai báo biến toàn cục */
static int gpio_pin = 21;       /* giá trị mặc định */
static char *device_name = "bbb_led";

/* Đăng ký là module parameter */
module_param(gpio_pin, int, S_IRUGO);           /* S_IRUGO = đọc được từ /sys */
MODULE_PARM_DESC(gpio_pin, "GPIO pin number (default: 21)");

module_param(device_name, charp, S_IRUGO);
MODULE_PARM_DESC(device_name, "Device name (default: bbb_led)");

MODULE_LICENSE("GPL");

static int __init param_init(void)
{
    pr_info("param_module: gpio_pin=%d, device_name=%s\n",
            gpio_pin, device_name);
    return 0;
}

static void __exit param_exit(void) { }

module_init(param_init);
module_exit(param_exit);
```

### Sử dụng

```bash
# Load với tham số mặc định
sudo insmod param_module.ko

# Load với tham số tùy chỉnh
sudo insmod param_module.ko gpio_pin=53 device_name="my_led"

# Xem parameter hiện tại (sau khi load)
cat /sys/module/param_module/parameters/gpio_pin
cat /sys/module/param_module/parameters/device_name

# Dùng modprobe (load từ /lib/modules, tự tìm dependency)
sudo modprobe param_module gpio_pin=53
```

---

## 7. Ví dụ: Module Truy Cập GPIO Register

Module kernel truy cập GPIO1 để bật LED USR0 (GPIO1_21):

```c
/*
 * gpio_module.c - Truy cập GPIO từ kernel module
 * LED USR0 = GPIO1_21, base address = 0x4804C000
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>           /* ioremap, ioread32, iowrite32 */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BBB Student");
MODULE_DESCRIPTION("GPIO LED control from kernel module");

/* GPIO1 base address (spruh73q.pdf Table 2-3) */
#define GPIO1_BASE      0x4804C000
#define GPIO1_SIZE      0x1000

/* GPIO register offsets (spruh73q.pdf Section 25.4.1) */
#define GPIO_OE         0x134   /* Output Enable: 0=output, 1=input */
#define GPIO_SETDATAOUT 0x194   /* Set pin HIGH */
#define GPIO_CLEARDATAOUT 0x190 /* Set pin LOW */

#define LED_PIN         (1 << 21)   /* GPIO1_21 */

static void __iomem *gpio1_base;    /* virtual address sau ioremap */

static int __init gpio_init(void)
{
    /* Map physical address vào virtual address space của kernel */
    gpio1_base = ioremap(GPIO1_BASE, GPIO1_SIZE);
    if (!gpio1_base) {
        pr_err("gpio_module: ioremap failed!\n");
        return -ENOMEM;
    }

    /* Set GPIO1_21 làm output (clear bit 21 trong OE register) */
    u32 oe = ioread32(gpio1_base + GPIO_OE);
    oe &= ~LED_PIN;     /* 0 = output */
    iowrite32(oe, gpio1_base + GPIO_OE);

    /* Bật LED */
    iowrite32(LED_PIN, gpio1_base + GPIO_SETDATAOUT);

    pr_info("gpio_module: LED USR0 turned ON\n");
    return 0;
}

static void __exit gpio_exit(void)
{
    /* Tắt LED khi unload */
    iowrite32(LED_PIN, gpio1_base + GPIO_CLEARDATAOUT);

    /* Giải phóng mapping */
    iounmap(gpio1_base);
    pr_info("gpio_module: LED turned OFF, unloaded\n");
}

module_init(gpio_init);
module_exit(gpio_exit);
```

> **Lưu ý**: Trong kernel module, dùng `ioremap`/`ioread32`/`iowrite32` thay vì `mmap(/dev/mem)`. Đây là cách "đúng" của kernel.

---

## 8. Câu hỏi kiểm tra

1. Tại sao kernel module lại cần thiết thay vì viết thẳng vào userspace?
2. Sự khác biệt giữa `insmod` và `modprobe` là gì?
3. `ioremap()` làm gì? Tại sao không dùng trực tiếp địa chỉ vật lý trong kernel?
4. `__init` và `__exit` macro có ý nghĩa gì với bộ nhớ?
5. Nếu hàm `module_init` trả về `-ENOMEM`, điều gì xảy ra?

---

## 9. Tài liệu tham khảo

| Nội dung | Tài liệu | Ghi chú |
|----------|----------|---------|
| GPIO1 base address | spruh73q.pdf, Table 2-3 | 0x4804C000 |
| GPIO register offsets | spruh73q.pdf, Section 25.4.1 | OE, SETDATAOUT |
| Kernel API | linux/kernel.h, linux/module.h | In-tree headers |
| Module parameters | linux/moduleparam.h | module_param() macro |
