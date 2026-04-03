# Bài 31 - Sysfs, Debugfs & Procfs Interface

> **Tham chiếu phần cứng BBB:** Xem [mapping/](../mapping/) để tra cứu thanh ghi và chân BBB.

## 1. Mục tiêu bài học
- Hiểu 3 virtual filesystem: sysfs, debugfs, procfs — khi nào dùng cái nào
- Tạo sysfs attributes cho device driver (read/write từ userspace)
- Tạo debugfs files để debug driver nội bộ
- Dùng `seq_file` API cho output dài
- Expose thông tin thanh ghi AM335x qua debugfs trên BBB

---

## 2. Tổng quan 3 Virtual Filesystem

```
┌─────────────────────────────────────────────────────┐
│                    Userspace                         │
│  cat /sys/...    cat /sys/kernel/debug/...   /proc  │
└────────┬──────────────────┬──────────────┬──────────┘
         │                  │              │
┌────────┴─────┐  ┌────────┴─────┐  ┌─────┴────────┐
│    sysfs     │  │   debugfs    │  │    procfs     │
│  /sys/       │  │  /sys/kernel │  │   /proc/      │
│              │  │  /debug/     │  │               │
│ Stable ABI   │  │ No ABI       │  │ Legacy,       │
│ 1 value/file │  │ Free format  │  │ process info  │
│ User-facing  │  │ Dev-only     │  │ + misc        │
└──────────────┘  └──────────────┘  └───────────────┘
```

### 2.1 Khi nào dùng gì?

| Tiêu chí | sysfs | debugfs | procfs |
|----------|-------|---------|--------|
| **Mục đích** | Configuration, status | Debug, register dump | Process info (legacy) |
| **ABI ổn định** | Có — không được thay đổi | Không — có thể thay đổi bất kỳ lúc nào | Có cho /proc/PID |
| **Quy tắc** | 1 giá trị / 1 file | Tự do | Tránh thêm mới |
| **Mount** | Auto (`/sys`) | `mount -t debugfs none /sys/kernel/debug` | Auto (`/proc`) |
| **Dùng cho driver** | ✅ Khuyến khích | ✅ Debug only | ❌ Tránh |

---

## 3. Sysfs — Device Attributes

### 3.1 Kiến trúc sysfs

```
/sys/
├── bus/
│   └── platform/
│       └── devices/
│           └── my_device/
│               ├── driver -> ../../../...
│               └── my_attr       ← bạn tạo
├── class/
│   └── gpio/
│       └── gpio53/
│           ├── value
│           ├── direction
│           └── edge
└── devices/
    └── platform/
        └── ...
```

### 3.2 Tạo device attribute

```c
#include <linux/device.h>
#include <linux/sysfs.h>

struct my_dev {
    struct platform_device *pdev;
    void __iomem *base;
    u32 cached_value;
};

/* show callback — đọc từ userspace: cat /sys/.../my_register */
static ssize_t my_register_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    struct my_dev *mydev = dev_get_drvdata(dev);
    u32 val = readl(mydev->base + 0x10);  /* đọc thanh ghi */
    return sysfs_emit(buf, "0x%08x\n", val);
}

/* store callback — ghi từ userspace: echo 0x1 > /sys/.../my_register */
static ssize_t my_register_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    struct my_dev *mydev = dev_get_drvdata(dev);
    u32 val;
    int ret;

    ret = kstrtou32(buf, 0, &val);
    if (ret)
        return ret;

    writel(val, mydev->base + 0x10);
    return count;
}

static DEVICE_ATTR_RW(my_register);

/* Attribute group */
static struct attribute *my_attrs[] = {
    &dev_attr_my_register.attr,
    NULL,
};
ATTRIBUTE_GROUPS(my);
```

### 3.3 Đăng ký trong probe

```c
static int my_probe(struct platform_device *pdev)
{
    struct my_dev *mydev;
    int ret;

    mydev = devm_kzalloc(&pdev->dev, sizeof(*mydev), GFP_KERNEL);
    if (!mydev)
        return -ENOMEM;

    platform_set_drvdata(pdev, mydev);

    /* Cách 1: tạo từng file */
    ret = device_create_file(&pdev->dev, &dev_attr_my_register);
    if (ret)
        return ret;

    /* Cách 2 (khuyến khích): dùng groups trong driver struct */
    /* .dev_groups = my_groups trong platform_driver */

    return 0;
}

static void my_remove(struct platform_device *pdev)
{
    device_remove_file(&pdev->dev, &dev_attr_my_register);
}

static struct platform_driver my_driver = {
    .driver = {
        .name = "my-device",
        .dev_groups = my_groups,  /* ← tự động tạo/xóa */
    },
    .probe = my_probe,
    .remove = my_remove,
};
```

### 3.4 Quy tắc quan trọng

1. **Một giá trị duy nhất mỗi file** — không dump nhiều thanh ghi vào 1 file
2. **Dùng `sysfs_emit()`** thay `sprintf()` (an toàn hơn, kernel 5.10+)
3. **Validate input** với `kstrtou32()`, `kstrtoint()` — không dùng `sscanf`
4. **`DEVICE_ATTR_RO`** cho read-only, **`DEVICE_ATTR_WO`** cho write-only
5. **Dùng `.dev_groups`** trong `platform_driver` để kernel tự quản lifecycle

---

## 4. Debugfs — Debug Interface

### 4.1 Tạo debugfs directory và files

```c
#include <linux/debugfs.h>

struct my_dev {
    void __iomem *base;
    struct dentry *debugfs_dir;
};

/* Đọc tất cả thanh ghi GPIO — ví dụ cho GPIO1 trên BBB */
static int regs_show(struct seq_file *s, void *data)
{
    struct my_dev *mydev = s->private;

    seq_printf(s, "GPIO_REVISION    : 0x%08x\n", readl(mydev->base + 0x00));
    seq_printf(s, "GPIO_SYSCONFIG   : 0x%08x\n", readl(mydev->base + 0x10));
    seq_printf(s, "GPIO_EOI         : 0x%08x\n", readl(mydev->base + 0x20));
    seq_printf(s, "GPIO_IRQSTATUS_0 : 0x%08x\n", readl(mydev->base + 0x2C));
    seq_printf(s, "GPIO_IRQSTATUS_1 : 0x%08x\n", readl(mydev->base + 0x30));
    seq_printf(s, "GPIO_IRQENABLE_0 : 0x%08x\n", readl(mydev->base + 0x34));
    seq_printf(s, "GPIO_IRQENABLE_1 : 0x%08x\n", readl(mydev->base + 0x38));
    seq_printf(s, "GPIO_OE          : 0x%08x\n", readl(mydev->base + 0x134));
    seq_printf(s, "GPIO_DATAIN      : 0x%08x\n", readl(mydev->base + 0x138));
    seq_printf(s, "GPIO_DATAOUT     : 0x%08x\n", readl(mydev->base + 0x13C));
    seq_printf(s, "GPIO_SETDATAOUT  : 0x%08x\n", readl(mydev->base + 0x194));
    seq_printf(s, "GPIO_CLEARDATAOUT: 0x%08x\n", readl(mydev->base + 0x190));

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(regs);

/* File đọc/ghi 1 thanh ghi cụ thể */
static int reg_offset_get(void *data, u64 *val)
{
    struct my_dev *mydev = data;
    *val = readl(mydev->base + 0x134);  /* GPIO_OE */
    return 0;
}

static int reg_offset_set(void *data, u64 val)
{
    struct my_dev *mydev = data;
    writel((u32)val, mydev->base + 0x134);
    return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(fops_gpio_oe, reg_offset_get, reg_offset_set, "0x%08llx\n");

static void my_debugfs_init(struct my_dev *mydev, const char *name)
{
    mydev->debugfs_dir = debugfs_create_dir(name, NULL);

    /* File hiển thị tất cả registers */
    debugfs_create_file("registers", 0444, mydev->debugfs_dir, mydev, &regs_fops);

    /* File đọc/ghi GPIO_OE trực tiếp */
    debugfs_create_file("gpio_oe", 0644, mydev->debugfs_dir, mydev, &fops_gpio_oe);

    /* File đơn giản cho u32 */
    debugfs_create_u32("cached_value", 0644, mydev->debugfs_dir, &mydev->cached_value);
}

static void my_debugfs_remove(struct my_dev *mydev)
{
    debugfs_remove_recursive(mydev->debugfs_dir);
}
```

### 4.2 Sử dụng trên BBB

```bash
# Mount debugfs (nếu chưa mount)
mount -t debugfs none /sys/kernel/debug

# Đọc tất cả registers
cat /sys/kernel/debug/my-gpio/registers
# GPIO_REVISION    : 0x50600801
# GPIO_OE          : 0xff1fffff
# GPIO_DATAIN      : 0x04060000
# ...

# Đọc/ghi GPIO_OE trực tiếp
cat /sys/kernel/debug/my-gpio/gpio_oe
# 0xff1fffff
echo 0xff0fffff > /sys/kernel/debug/my-gpio/gpio_oe
```

### 4.3 `seq_file` API cho output dài

```c
#include <linux/seq_file.h>

/* Khi output > PAGE_SIZE, dùng seq_file iterator */
static void *my_seq_start(struct seq_file *s, loff_t *pos)
{
    if (*pos >= NUM_REGS)
        return NULL;
    return pos;
}

static void *my_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    (*pos)++;
    if (*pos >= NUM_REGS)
        return NULL;
    return pos;
}

static void my_seq_stop(struct seq_file *s, void *v)
{
    /* cleanup nếu cần */
}

static int my_seq_show(struct seq_file *s, void *v)
{
    loff_t *pos = v;
    struct my_dev *mydev = s->private;
    u32 offset = (*pos) * 4;

    seq_printf(s, "[0x%03x] = 0x%08x\n", offset, readl(mydev->base + offset));
    return 0;
}

static const struct seq_operations my_seq_ops = {
    .start = my_seq_start,
    .next  = my_seq_next,
    .stop  = my_seq_stop,
    .show  = my_seq_show,
};
```

---

## 5. Procfs — Legacy Interface (tham khảo)

```c
#include <linux/proc_fs.h>

/* Chỉ dùng khi thực sự cần — prefer sysfs/debugfs */
static int my_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "driver version: 1.0\n");
    return 0;
}

static int my_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, my_proc_show, PDE_DATA(inode));
}

static const struct proc_ops my_proc_ops = {
    .proc_open    = my_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* Trong init */
proc_create_data("my_driver_info", 0444, NULL, &my_proc_ops, mydev);

/* Trong exit */
remove_proc_entry("my_driver_info", NULL);
```

---

## 6. Ví dụ thực hành trên BBB — GPIO Register Debugger

### 6.1 Driver hoàn chỉnh

```c
/* bbb_gpio_debug.c — debug GPIO1 registers trên BBB qua debugfs + sysfs */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#define GPIO1_BASE  0x4804C000
#define GPIO1_SIZE  0x1000

struct bbb_gpio_debug {
    void __iomem *base;
    struct dentry *debugfs_dir;
    u32 led_mask;  /* USR LED0-3: bits 21-24 */
};

/* --- Sysfs: led_mask (controllable from userspace) --- */
static ssize_t led_mask_show(struct device *dev,
                             struct device_attribute *attr, char *buf)
{
    struct bbb_gpio_debug *priv = dev_get_drvdata(dev);
    return sysfs_emit(buf, "0x%02x\n", priv->led_mask);
}

static ssize_t led_mask_store(struct device *dev,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
    struct bbb_gpio_debug *priv = dev_get_drvdata(dev);
    u32 val;
    int ret;

    ret = kstrtou32(buf, 0, &val);
    if (ret)
        return ret;

    val &= 0x0F;  /* chỉ 4 LED */
    priv->led_mask = val;

    /* Set USR LED0-3 (GPIO1 bits 21-24) */
    writel((val & 0x0F) << 21, priv->base + 0x194);       /* SETDATAOUT */
    writel((~val & 0x0F) << 21, priv->base + 0x190);      /* CLEARDATAOUT */

    return count;
}
static DEVICE_ATTR_RW(led_mask);

static struct attribute *bbb_gpio_attrs[] = {
    &dev_attr_led_mask.attr,
    NULL,
};
ATTRIBUTE_GROUPS(bbb_gpio);

/* --- Debugfs: register dump --- */
static const struct {
    const char *name;
    u32 offset;
} gpio_regs[] = {
    { "GPIO_REVISION",     0x000 },
    { "GPIO_SYSCONFIG",    0x010 },
    { "GPIO_IRQSTATUS_0",  0x02C },
    { "GPIO_IRQENABLE_0",  0x034 },
    { "GPIO_OE",           0x134 },
    { "GPIO_DATAIN",       0x138 },
    { "GPIO_DATAOUT",      0x13C },
    { "GPIO_SETDATAOUT",   0x194 },
    { "GPIO_CLEARDATAOUT", 0x190 },
};

static int regs_show(struct seq_file *s, void *data)
{
    struct bbb_gpio_debug *priv = s->private;
    int i;

    seq_puts(s, "=== GPIO1 Register Dump (BBB USR LEDs) ===\n");
    for (i = 0; i < ARRAY_SIZE(gpio_regs); i++) {
        seq_printf(s, "%-20s [0x%03x] = 0x%08x\n",
                   gpio_regs[i].name,
                   gpio_regs[i].offset,
                   readl(priv->base + gpio_regs[i].offset));
    }

    /* Show individual LED states */
    u32 dataout = readl(priv->base + 0x13C);
    seq_puts(s, "\n--- USR LED Status ---\n");
    seq_printf(s, "USR LED0 (GPIO1_21): %s\n", (dataout & BIT(21)) ? "ON" : "OFF");
    seq_printf(s, "USR LED1 (GPIO1_22): %s\n", (dataout & BIT(22)) ? "ON" : "OFF");
    seq_printf(s, "USR LED2 (GPIO1_23): %s\n", (dataout & BIT(23)) ? "ON" : "OFF");
    seq_printf(s, "USR LED3 (GPIO1_24): %s\n", (dataout & BIT(24)) ? "ON" : "OFF");

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(regs);

static int bbb_gpio_debug_probe(struct platform_device *pdev)
{
    struct bbb_gpio_debug *priv;
    struct resource *res;

    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    priv->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(priv->base))
        return PTR_ERR(priv->base);

    platform_set_drvdata(pdev, priv);

    /* Debugfs */
    priv->debugfs_dir = debugfs_create_dir("bbb-gpio1", NULL);
    debugfs_create_file("registers", 0444, priv->debugfs_dir, priv, &regs_fops);

    dev_info(&pdev->dev, "BBB GPIO debug driver loaded\n");
    return 0;
}

static void bbb_gpio_debug_remove(struct platform_device *pdev)
{
    struct bbb_gpio_debug *priv = platform_get_drvdata(pdev);
    debugfs_remove_recursive(priv->debugfs_dir);
}

static const struct of_device_id bbb_gpio_debug_of_match[] = {
    { .compatible = "ti,am335x-gpio-debug" },
    { },
};
MODULE_DEVICE_TABLE(of, bbb_gpio_debug_of_match);

static struct platform_driver bbb_gpio_debug_driver = {
    .driver = {
        .name = "bbb-gpio-debug",
        .of_match_table = bbb_gpio_debug_of_match,
        .dev_groups = bbb_gpio_groups,
    },
    .probe  = bbb_gpio_debug_probe,
    .remove = bbb_gpio_debug_remove,
};
module_platform_driver(bbb_gpio_debug_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BBB Student");
MODULE_DESCRIPTION("BBB GPIO1 debug driver with sysfs + debugfs");
```

### 6.2 Device Tree node

```dts
/* Thêm vào DTS overlay cho BBB */
gpio1_debug: gpio1-debug@4804c000 {
    compatible = "ti,am335x-gpio-debug";
    reg = <0x4804c000 0x1000>;
    status = "okay";
};
```

### 6.3 Test trên BBB

```bash
# Load module
insmod bbb_gpio_debug.ko

# Sysfs — điều khiển LEDs
echo 0x0F > /sys/devices/platform/gpio1-debug/led_mask   # tất cả ON
echo 0x05 > /sys/devices/platform/gpio1-debug/led_mask   # LED0 + LED2 ON

# Debugfs — dump registers
cat /sys/kernel/debug/bbb-gpio1/registers
# === GPIO1 Register Dump (BBB USR LEDs) ===
# GPIO_REVISION     [0x000] = 0x50600801
# GPIO_OE           [0x134] = 0xfe1fffff
# GPIO_DATAOUT      [0x13C] = 0x00a00000
# --- USR LED Status ---
# USR LED0 (GPIO1_21): ON
# USR LED1 (GPIO1_22): OFF
# ...
```

---

## 7. So sánh sysfs vs debugfs — Bảng quyết định

| Bạn muốn... | Dùng |
|---|---|
| Expose 1 setting cho user (ví dụ: brightness, enable) | **sysfs** `DEVICE_ATTR_RW` |
| Dump toàn bộ register map | **debugfs** `seq_file` |
| Read-only status (ví dụ: temperature, version) | **sysfs** `DEVICE_ATTR_RO` |
| Debug IRQ counters, internal state | **debugfs** |
| Cho userspace tool đọc/ghi | **sysfs** (stable ABI) |
| Testing trong quá trình phát triển | **debugfs** (no ABI guarantee) |

---

## 8. Câu hỏi kiểm tra

1. Tại sao sysfs yêu cầu "1 giá trị / 1 file"?
2. Sự khác biệt giữa `device_create_file()` và `.dev_groups`?
3. Khi nào dùng `seq_file` thay vì `sysfs_emit()`?
4. Tại sao không nên expose thanh ghi qua sysfs (dùng debugfs thay thế)?
5. Viết debugfs file để dump Control Module pad config registers trên BBB.

---

Bài tiếp theo: **Bài 29 - Debug Techniques**

Sử dụng các công cụ debug kernel: printk, dynamic_debug, ftrace, trace-cmd, kgdb.
