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

### Giải thích chi tiết từng dòng code

a) **Header includes**:
- `#include <linux/module.h>` — cung cấp macro `module_init()`, `module_exit()`, `MODULE_LICENSE()`, `MODULE_AUTHOR()`... Đây là header **bắt buộc** cho mọi kernel module.
- `#include <linux/kernel.h>` — cung cấp `printk()` và các macro log level (`KERN_INFO`, `KERN_ERR`...).
- `#include <linux/init.h>` — cung cấp `__init` và `__exit` macro.

b) **MODULE_LICENSE("GPL")**:
- Khai báo module tuân thủ license GPL. **Bắt buộc** — nếu thiếu, kernel sẽ in warning "module license taints kernel" và **một số API sẽ bị khóa** (các hàm export chỉ dành cho GPL module).
- Các giá trị hợp lệ: `"GPL"`, `"GPL v2"`, `"Dual BSD/GPL"`.

c) **`static int __init hello_init(void)`**:
- `static` — hàm chỉ visible trong file này (không export ra ngoài module).
- `__init` — macro đánh dấu vùng nhớ của hàm này thuộc section `.init.text`. Sau khi init xong, kernel **giải phóng** vùng nhớ này để tiết kiệm RAM.
- Trả về `int`: `0` = thành công, giá trị âm = error code (ví dụ `-ENOMEM`, `-EINVAL`). Nếu trả về lỗi, module **không được load**.

d) **`printk(KERN_INFO "hello_module: ...\n")`**:
- `printk` là `printf` của kernel — ghi vào **kernel ring buffer**, đọc bằng `dmesg`.
- `KERN_INFO` = log level 6 (informational). Không phải tham số — nó là **string literal** nối trực tiếp: `KERN_INFO` = `"\0016"`, kết quả là `"\0016hello_module: ...\n"`.

e) **`static void __exit hello_exit(void)`**:
- `__exit` — hàm chỉ dùng khi unload module. Nếu module được **built-in** vào kernel (không phải `.ko`), compiler loại bỏ hàm này hoàn toàn.
- Trả về `void` — hàm exit **không thể fail** (phải luôn cleanup thành công).

f) **`module_init(hello_init)` / `module_exit(hello_exit)`**:
- Macro đăng ký hàm nào sẽ chạy khi `insmod` (init) và `rmmod` (exit).
- Bên trong, chúng tạo function pointer mà kernel dùng để gọi đúng hàm.

> **Bài học**: Mọi kernel module **tối thiểu** cần: 1 hàm init, 1 hàm exit, `MODULE_LICENSE`, và 2 macro `module_init`/`module_exit`.

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

### Giải thích chi tiết từng dòng code

a) **`#include <linux/io.h>`**:
- Cung cấp hàm `ioremap()`, `iounmap()`, `ioread32()`, `iowrite32()` — các hàm truy cập I/O register chuẩn của kernel.
- Khác với userspace dùng `mmap(/dev/mem)`, kernel dùng `ioremap()` để map physical address vào **kernel virtual address space**.

b) **`#define GPIO1_BASE 0x4804C000` và `#define GPIO1_SIZE 0x1000`**:
- Địa chỉ vật lý GPIO1 từ TRM (spruh73q.pdf, Table 2-3). Size 4KB = 0x1000 là kích thước vùng register của một GPIO module.
- `#define LED_PIN (1 << 21)` — tạo bitmask `0x00200000`, bit 21 = 1 (GPIO1_21 = LED USR0).

c) **`static void __iomem *gpio1_base`**:
- `void __iomem *` — kiểu con trỏ đặc biệt cho **I/O mapped memory**.
- `__iomem` là annotation cho sparse checker (tool kiểm tra code kernel). Nếu bạn dùng `*(gpio1_base + offset)` trực tiếp, sparse sẽ warning — bắt buộc phải dùng `ioread32()`/`iowrite32()`.

d) **`gpio1_base = ioremap(GPIO1_BASE, GPIO1_SIZE)`**:
- `ioremap()` map vùng physical `0x4804C000..0x4804CFFF` vào virtual address mà kernel có thể truy cập.
- Trả về `NULL` nếu thất bại → kiểm tra lỗi bắt buộc.
- Khác `mmap()` userspace: `ioremap()` chạy trong kernel context, không cần `open(/dev/mem)`.

e) **`ioread32()` / `iowrite32()`**:
- `u32 oe = ioread32(gpio1_base + GPIO_OE)` — đọc 32-bit register tại offset `0x134`.
- `iowrite32(oe, gpio1_base + GPIO_OE)` — ghi giá trị 32-bit vào register.
- Các hàm này đảm bảo **memory barrier** và **byte ordering** đúng trên mọi kiến trúc ARM.
- Pointer arithmetic: `gpio1_base + GPIO_OE` = `gpio1_base + 0x134`. Vì `void __iomem *`, phép cộng tính theo **byte** (khác `volatile uint32_t *` tính theo 4-byte).

f) **Bit manipulation cho OE register**:
```c
oe &= ~LED_PIN;   /* Clear bit 21 → output mode */
```
- `~LED_PIN` = `~0x00200000` = `0xFFDFFFFF` — tất cả bit 1 trừ bit 21.
- `oe &= ~LED_PIN` — giữ nguyên các bit khác, chỉ clear bit 21.

g) **`iowrite32(LED_PIN, gpio1_base + GPIO_SETDATAOUT)`**:
- Ghi `0x00200000` vào register SETDATAOUT (offset `0x194`).
- Register SETDATAOUT chỉ **set** các bit có giá trị 1, **không ảnh hưởng** bit 0 → an toàn cho multi-pin.

h) **Cleanup trong `gpio_exit()`**:
- `iowrite32(LED_PIN, gpio1_base + GPIO_CLEARDATAOUT)` — tắt LED trước.
- `iounmap(gpio1_base)` — **bắt buộc** giải phóng mapping. Quên `iounmap()` = **memory leak** trong kernel (khác userspace, kernel không tự cleanup khi module unload).

> **Bài học**: So sánh userspace vs kernel GPIO access:
> | | Userspace (bài 4) | Kernel module (bài 17) |
> |---|---|---|
> | Map | `mmap(/dev/mem)` | `ioremap()` |
> | Read | `*(volatile uint32_t*)` | `ioread32()` |
> | Write | gán trực tiếp | `iowrite32()` |
> | Unmap | `munmap()` | `iounmap()` |
> | Quyền | Cần root + `/dev/mem` | Tự động (kernel space) |

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
