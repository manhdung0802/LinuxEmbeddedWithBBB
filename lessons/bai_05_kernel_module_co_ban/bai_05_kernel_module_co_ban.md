# Bài 5 - Linux Kernel Module cơ bản

## 1. Mục tiêu bài học
- Hiểu kernel module là gì và vai trò trong Linux kernel
- Viết kernel module đầu tiên với `module_init` / `module_exit`
- Sử dụng `printk` để debug
- Truyền tham số cho module
- Tạo sysfs entry đơn giản
- Hiểu vòng đời module: insmod → hoạt động → rmmod

---

## 2. Kernel Module là gì?

Kernel module là đoạn code có thể **load/unload vào kernel khi đang chạy**, không cần reboot.

```
┌────────────────────────────────────┐
│         Linux Kernel               │
│                                    │
│  ┌──────┐  ┌──────┐  ┌──────┐    │
│  │GPIO  │  │I2C   │  │ SPI  │    │  ← In-tree modules (built-in)
│  │driver│  │driver│  │driver│    │
│  └──────┘  └──────┘  └──────┘    │
│                                    │
│  ┌──────────────────┐             │
│  │ my_driver.ko     │  ← insmod  │  ← Out-of-tree module (loadable)
│  │ (your module)    │  ← rmmod   │
│  └──────────────────┘             │
└────────────────────────────────────┘
```

**Ưu điểm**:
- Không cần rebuild toàn bộ kernel khi thêm driver
- Load/unload runtime → tiết kiệm bộ nhớ
- Phát triển nhanh: sửa code → build → insmod → test

---

## 3. Module đầu tiên: Hello Kernel

### 3.1 Source code: `hello.c`

```c
#include <linux/module.h>      /* Mọi module đều cần */
#include <linux/kernel.h>      /* printk(), KERN_INFO */
#include <linux/init.h>        /* __init, __exit macros */

/* Hàm được gọi khi insmod */
static int __init hello_init(void)
{
	pr_info("Hello, Kernel! Module loaded.\n");
	return 0;   /* 0 = thành công, < 0 = lỗi */
}

/* Hàm được gọi khi rmmod */
static void __exit hello_exit(void)
{
	pr_info("Goodbye, Kernel! Module unloaded.\n");
}

/* Đăng ký hàm init và exit */
module_init(hello_init);
module_exit(hello_exit);

/* Metadata */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("BBB Student");
MODULE_DESCRIPTION("Hello Kernel Module");
MODULE_VERSION("1.0");
```

### 3.2 Giải thích từng phần:

**Headers**:
- `linux/module.h`: Macro `module_init`, `module_exit`, `MODULE_LICENSE`
- `linux/kernel.h`: `printk`, `pr_info`, `pr_err`
- `linux/init.h`: `__init`, `__exit` — đánh dấu hàm chỉ dùng lúc init/exit

**`__init` macro**:
```c
static int __init hello_init(void)
```
- `__init` báo kernel: hàm này chỉ dùng 1 lần lúc load
- Kernel sẽ free bộ nhớ sau khi init xong → tiết kiệm RAM

**`__exit` macro**:
- Tương tự, chỉ dùng khi unload module
- Nếu module built-in (không loadable), hàm exit bị bỏ qua

**Return value của init**:
- `return 0`: Module load thành công
- `return -ENOMEM`, `-ENODEV`, ...: Load thất bại, kernel tự gỡ module

**`MODULE_LICENSE("GPL")`**:
- **Bắt buộc** — nếu thiếu, kernel sẽ cảnh báo "tainted"
- GPL cho phép dùng tất cả kernel API
- Các license khác bị hạn chế một số symbol

### 3.3 Makefile:

```makefile
obj-m += hello.o

KERNEL_DIR ?= ~/bbb-kernel
CROSS_COMPILE ?= arm-linux-gnueabihf-
ARCH ?= arm

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
	     -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
	     -C $(KERNEL_DIR) M=$(PWD) clean
```

### 3.4 Build và test:

```bash
# Build
make

# Deploy lên BBB
scp hello.ko debian@192.168.7.2:/home/debian/

# Trên BBB:
sudo insmod hello.ko
dmesg | tail -3
# [xxx] Hello, Kernel! Module loaded.

sudo rmmod hello
dmesg | tail -3
# [xxx] Goodbye, Kernel! Module unloaded.
```

---

## 4. Printk - Hệ thống log của kernel

### 4.1 Log levels:

```c
pr_emerg("Emergency!\n");     /* KERN_EMERG   = 0 */
pr_alert("Alert!\n");         /* KERN_ALERT   = 1 */
pr_crit("Critical!\n");       /* KERN_CRIT    = 2 */
pr_err("Error!\n");           /* KERN_ERR     = 3 */
pr_warn("Warning!\n");        /* KERN_WARNING = 4 */
pr_notice("Notice!\n");       /* KERN_NOTICE  = 5 */
pr_info("Info!\n");           /* KERN_INFO    = 6 */
pr_debug("Debug!\n");         /* KERN_DEBUG   = 7 */
```

### 4.2 Xem log:

```bash
# Xem tất cả kernel log
dmesg

# Xem log mới nhất
dmesg | tail -20

# Theo dõi log realtime
dmesg -w

# Xem level hiện tại
cat /proc/sys/kernel/printk
# 7  4  1  7
# ↑ console_loglevel: chỉ hiển thị level < 7
```

### 4.3 `pr_info` vs `printk`:

```c
/* Hai dòng này tương đương: */
printk(KERN_INFO "Hello\n");
pr_info("Hello\n");              /* Viết gọn hơn, khuyên dùng */

/* Dùng pr_fmt để thêm prefix tự động */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
/* Khi dùng pr_info("test") → output: "hello: test" */
```

---

## 5. Module Parameters

Cho phép truyền tham số khi load module:

### 5.1 Khai báo parameter:

```c
#include <linux/moduleparam.h>

/* Khai báo biến tham số */
static int gpio_num = 21;           /* Giá trị mặc định */
static char *name = "my_led";
static bool active = true;

/* Đăng ký tham số */
module_param(gpio_num, int, 0644);
MODULE_PARM_DESC(gpio_num, "GPIO number to use (default: 21)");

module_param(name, charp, 0444);
MODULE_PARM_DESC(name, "Device name");

module_param(active, bool, 0644);
MODULE_PARM_DESC(active, "Active state (default: true)");
```

### 5.2 Giải thích `module_param(name, type, perm)`:

- **name**: Tên biến C = tên parameter khi insmod
- **type**: `int`, `uint`, `long`, `charp` (char*), `bool`, `short`
- **perm**: Permission trong `/sys/module/<name>/parameters/`
  - `0644`: Owner read/write, group/other read
  - `0444`: Read-only
  - `0`: Không xuất ra sysfs

### 5.3 Sử dụng:

```bash
# Load với tham số
sudo insmod hello.ko gpio_num=24 name="test_led" active=false

# Xem tham số đang dùng
cat /sys/module/hello/parameters/gpio_num
# 24

# Thay đổi tham số runtime (nếu perm cho phép write)
echo 30 > /sys/module/hello/parameters/gpio_num
```

---

## 6. Module với nhiều file source

Khi driver phức tạp, tách thành nhiều file:

### 6.1 Cấu trúc:

```
my_driver/
├── main.c
├── hw_ops.c
├── hw_ops.h
└── Makefile
```

### 6.2 Makefile cho multi-file module:

```makefile
# Tên module cuối cùng: my_driver.ko
obj-m += my_driver.o

# my_driver.ko được build từ 2 file .o
my_driver-objs := main.o hw_ops.o

KERNEL_DIR ?= ~/bbb-kernel
CROSS_COMPILE ?= arm-linux-gnueabihf-
ARCH ?= arm

all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
	     -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
	     -C $(KERNEL_DIR) M=$(PWD) clean
```

---

## 7. Sysfs entry cơ bản

Module có thể tạo file trong `/sys/` để giao tiếp với userspace:

```c
#include <linux/kobject.h>
#include <linux/sysfs.h>

static int my_value = 0;
static struct kobject *my_kobj;

/* Hàm đọc (cat /sys/kernel/my_module/my_value) */
static ssize_t my_value_show(struct kobject *kobj,
                              struct kobj_attribute *attr,
                              char *buf)
{
	return sprintf(buf, "%d\n", my_value);
}

/* Hàm ghi (echo 123 > /sys/kernel/my_module/my_value) */
static ssize_t my_value_store(struct kobject *kobj,
                               struct kobj_attribute *attr,
                               const char *buf, size_t count)
{
	int ret;

	ret = kstrtoint(buf, 10, &my_value);
	if (ret < 0)
		return ret;

	return count;
}

static struct kobj_attribute my_attr =
	__ATTR(my_value, 0664, my_value_show, my_value_store);

static int __init my_init(void)
{
	my_kobj = kobject_create_and_add("my_module", kernel_kobj);
	if (!my_kobj)
		return -ENOMEM;

	if (sysfs_create_file(my_kobj, &my_attr.attr)) {
		kobject_put(my_kobj);
		return -ENOMEM;
	}

	return 0;
}

static void __exit my_exit(void)
{
	kobject_put(my_kobj);
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
```

---

## 8. Vòng đời của Module

```
insmod hello.ko
    │
    ▼
module_init(hello_init)  ← Kernel gọi hàm init
    │
    ├── Thành công (return 0) → Module hoạt động
    │       │
    │       ├── /sys/module/hello/ được tạo
    │       ├── Module xuất hiện trong lsmod
    │       └── Sẵn sàng phục vụ (file_ops, interrupt, ...)
    │
    └── Thất bại (return -EXXX) → Kernel gỡ module tự động
            └── Cleanup code trong init phải tự dọn đã alloc

rmmod hello
    │
    ▼
module_exit(hello_exit)  ← Kernel gọi hàm exit
    │
    └── Giải phóng mọi resource đã xin
        (unregister, free, iounmap, ...)
```

---

## 9. Các lỗi thường gặp

### 9.1 Thiếu `MODULE_LICENSE`:
```
module: loading module taints kernel
```
→ Thêm `MODULE_LICENSE("GPL")`.

### 9.2 Lỗi "Unknown symbol":
```
insmod: ERROR: could not insert module: Unknown symbol in module
```
→ Module phụ thuộc symbol chưa được export. Kiểm tra `modprobe` hoặc load dependency trước.

### 9.3 Init return lỗi nhưng không cleanup:
```c
/* SAI - nếu step2 fail, step1 chưa được cleanup */
static int __init bad_init(void)
{
    step1_alloc();
    ret = step2_alloc();
    if (ret)
        return ret;  /* step1 bị leak! */
}

/* ĐÚNG - cleanup ngược lại */
static int __init good_init(void)
{
    step1_alloc();
    ret = step2_alloc();
    if (ret) {
        step1_free();  /* Dọn step1 */
        return ret;
    }
    return 0;
}
```

---

## 10. Kiến thức cốt lõi sau bài này

1. **Kernel module** = code loadable vào kernel runtime
2. **`module_init/exit`** = entry/exit point
3. **`pr_info`** = cách log trong kernel, xem bằng `dmesg`
4. **Module parameters** = truyền giá trị lúc insmod, thay đổi qua sysfs
5. **`MODULE_LICENSE("GPL")`** bắt buộc
6. **Cleanup ngược**: nếu init fail giữa chừng, phải dọn những gì đã alloc

---

## 11. Câu hỏi kiểm tra

1. `__init` và `__exit` macro có tác dụng gì?
2. Sự khác nhau giữa `printk(KERN_INFO ...)` và `pr_info(...)` là gì?
3. Tại sao `MODULE_LICENSE("GPL")` lại bắt buộc?
4. Nếu `module_init` return `-ENOMEM`, điều gì xảy ra?
5. Viết Makefile cho module gồm 3 file: `main.c`, `gpio_ops.c`, `spi_ops.c`.

---

## 12. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 6 - Memory-Mapped I/O trong kernel**

Ta sẽ học cách driver truy cập thanh ghi phần cứng trong kernel space: `ioremap`, `readl`/`writel`, `platform_get_resource`.
