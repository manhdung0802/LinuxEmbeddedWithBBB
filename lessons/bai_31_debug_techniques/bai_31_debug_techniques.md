# Bài 31 - Debug Techniques

## 1. Mục tiêu bài học
- Nắm vững các kỹ thuật debug kernel driver
- printk levels và dynamic debug
- ftrace cho function tracing
- debugfs cho runtime driver debug
- /proc và /sys interface
- Các tool hỗ trợ: devmem, i2ctools, gpiomon

---

## 2. printk

### 2.1 Log levels

```c
#include <linux/printk.h>

/* 8 levels: 0 = emergency, 7 = debug */
pr_emerg("Emergency: system unusable\n");    /* 0 */
pr_alert("Alert: immediate action\n");        /* 1 */
pr_crit("Critical: hardware error\n");        /* 2 */
pr_err("Error: something failed\n");          /* 3 */
pr_warn("Warning: might be problem\n");       /* 4 */
pr_notice("Notice: normal but notable\n");    /* 5 */
pr_info("Info: general information\n");       /* 6 */
pr_debug("Debug: verbose debug info\n");      /* 7 */

/* Device-specific (thêm device name prefix) */
dev_err(&pdev->dev, "Error: %d\n", ret);
dev_warn(&pdev->dev, "Warning\n");
dev_info(&pdev->dev, "Info\n");
dev_dbg(&pdev->dev, "Debug\n");
```

### 2.2 Console log level

```bash
# Xem current log level
cat /proc/sys/kernel/printk
# 4    4    1    7
# current  default  minimum  boot-default

# Set console level to 8 (show all messages)
echo 8 > /proc/sys/kernel/printk

# Xem kernel log
dmesg
dmesg | tail -20
dmesg -w          # Follow (like tail -f)
dmesg -l err      # Chỉ show errors
dmesg -T          # Human-readable timestamps
```

### 2.3 Rate-limited printk

```c
/* Tránh spam log */
pr_info_ratelimited("IRQ handled: %d\n", count);

/* Chỉ in lần đầu */
pr_info_once("Driver initialized\n");

/* Conditional debug */
pr_debug_ratelimited("Periodic update\n");
```

---

## 3. Dynamic Debug

### 3.1 Khái niệm

`pr_debug()` và `dev_dbg()` mặc định bị tắt. Dynamic debug cho phép bật/tắt tại runtime.

### 3.2 Sử dụng

```bash
# Mount debugfs (thường tự mount)
mount -t debugfs none /sys/kernel/debug

# Xem tất cả debug statements
cat /sys/kernel/debug/dynamic_debug/control

# Bật debug cho module cụ thể
echo 'module my_driver +p' > /sys/kernel/debug/dynamic_debug/control

# Bật debug cho file cụ thể
echo 'file my_driver.c +p' > /sys/kernel/debug/dynamic_debug/control

# Bật debug cho function
echo 'func my_probe +p' > /sys/kernel/debug/dynamic_debug/control

# Bật debug + function name + line number
echo 'module my_driver +pfl' > /sys/kernel/debug/dynamic_debug/control

# Tắt
echo 'module my_driver -p' > /sys/kernel/debug/dynamic_debug/control
```

### 3.3 Flags

| Flag | Mô tả |
|------|-------|
| p | Enable print |
| f | Include function name |
| l | Include line number |
| m | Include module name |
| t | Include thread ID |

---

## 4. ftrace

### 4.1 Function tracing

```bash
# Xem available tracers
cat /sys/kernel/debug/tracing/available_tracers
# function function_graph nop

# Set function tracer
echo function > /sys/kernel/debug/tracing/current_tracer

# Filter: chỉ trace module
echo ':mod:my_driver' > /sys/kernel/debug/tracing/set_ftrace_filter

# Hoặc trace function cụ thể
echo 'my_probe' > /sys/kernel/debug/tracing/set_ftrace_filter

# Bật tracing
echo 1 > /sys/kernel/debug/tracing/tracing_on

# Đọc trace
cat /sys/kernel/debug/tracing/trace

# Tắt
echo 0 > /sys/kernel/debug/tracing/tracing_on
echo nop > /sys/kernel/debug/tracing/current_tracer
```

### 4.2 Function graph tracer

```bash
echo function_graph > /sys/kernel/debug/tracing/current_tracer
echo ':mod:my_driver' > /sys/kernel/debug/tracing/set_graph_function
echo 1 > /sys/kernel/debug/tracing/tracing_on

# Output shows call graph:
#  0)               |  my_probe() {
#  0)   1.234 us    |    platform_get_resource();
#  0)   0.567 us    |    devm_ioremap_resource();
#  0) + 15.678 us   |  }
```

### 4.3 Event tracing

```bash
# Xem available events
cat /sys/kernel/debug/tracing/available_events | grep gpio

# Enable GPIO events
echo 1 > /sys/kernel/debug/tracing/events/gpio/enable

# Enable IRQ events
echo 1 > /sys/kernel/debug/tracing/events/irq/enable

cat /sys/kernel/debug/tracing/trace
```

---

## 5. debugfs

### 5.1 Tạo debugfs entries trong driver

```c
#include <linux/debugfs.h>

struct my_driver_data {
    struct dentry *debugfs_dir;
    u32 debug_reg_addr;
    u32 irq_count;
};

static int irq_count_show(struct seq_file *s, void *data)
{
    struct my_driver_data *drv = s->private;
    seq_printf(s, "%u\n", drv->irq_count);
    return 0;
}
DEFINE_SHOW_ATTRIBUTE(irq_count);

static ssize_t reg_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos)
{
    struct my_driver_data *drv = file->private_data;
    char tmp[32];
    u32 val;
    int len;

    val = readl(drv->base + drv->debug_reg_addr);
    len = snprintf(tmp, sizeof(tmp), "0x%08x\n", val);
    return simple_read_from_buffer(buf, count, ppos, tmp, len);
}

static const struct file_operations reg_fops = {
    .owner = THIS_MODULE,
    .open = simple_open,
    .read = reg_read,
};

/* Trong probe: */
static void my_debugfs_init(struct my_driver_data *drv)
{
    drv->debugfs_dir = debugfs_create_dir("my_driver", NULL);

    /* File readonly: irq count */
    debugfs_create_file("irq_count", 0444, drv->debugfs_dir,
                         drv, &irq_count_fops);

    /* File cho register address */
    debugfs_create_x32("reg_addr", 0644, drv->debugfs_dir,
                        &drv->debug_reg_addr);

    /* File cho register read */
    debugfs_create_file("reg_read", 0444, drv->debugfs_dir,
                         drv, &reg_fops);

    /* Boolean */
    debugfs_create_bool("debug_enabled", 0644, drv->debugfs_dir,
                         &drv->debug_enabled);
}

/* Trong remove: */
debugfs_remove_recursive(drv->debugfs_dir);
```

### 5.2 Sử dụng

```bash
ls /sys/kernel/debug/my_driver/

# Đọc IRQ count
cat /sys/kernel/debug/my_driver/irq_count

# Set register address
echo 0x40 > /sys/kernel/debug/my_driver/reg_addr

# Read register
cat /sys/kernel/debug/my_driver/reg_read
```

---

## 6. Helper Tools

### 6.1 devmem2

```bash
# Đọc register trực tiếp
devmem2 0x44E07000 w    # GPIO1 base, output data

# Ghi register
devmem2 0x44E07194 w 0x10000000  # Set GPIO1_28
```

### 6.2 i2c-tools

```bash
# Scan I2C bus
i2cdetect -y 2

# Read register
i2cget -y 2 0x48 0x00 w

# Write register
i2cset -y 2 0x48 0x01 0x60 b

# Dump all registers
i2cdump -y 2 0x48
```

### 6.3 GPIO tools

```bash
# gpioinfo (libgpiod)
gpioinfo gpiochip0

# Monitor GPIO events
gpiomon gpiochip1 28

# Set GPIO output
gpioset gpiochip1 28=1
```

### 6.4 /proc filesystem

```bash
# Interrupt statistics
cat /proc/interrupts

# Memory map
cat /proc/iomem

# Loaded modules
cat /proc/modules

# Device tree (decompiled)
dtc -I fs /sys/firmware/devicetree/base
```

---

## 7. OOPS và Panic analysis

### 7.1 Đọc kernel OOPS

```
[  123.456] Unable to handle kernel NULL pointer dereference at virtual address 00000004
[  123.456] pgd = db7f4000
[  123.456] [00000004] *pgd=9b7e6831, *pte=00000000, *ppte=00000000
[  123.456] Internal error: Oops: 17 [#1] SMP ARM
[  123.456] Modules linked in: my_driver(O)
[  123.456] CPU: 0 PID: 1234 Comm: insmod Tainted: G           O
[  123.456] PC is at my_probe+0x3c/0x120 [my_driver]
[  123.456] LR is at my_probe+0x28/0x120 [my_driver]
```

### 7.2 Decode stack trace

```bash
# Với addr2line
arm-linux-gnueabihf-addr2line -e my_driver.ko -f 0x3c

# Với objdump
arm-linux-gnueabihf-objdump -d -S my_driver.ko | less
# Tìm offset 0x3c trong function my_probe
```

---

## 8. Kiến thức cốt lõi

1. **printk levels**: `dev_err/warn/info/dbg`, rate-limited variants
2. **Dynamic debug**: bật/tắt `pr_debug()` tại runtime qua debugfs
3. **ftrace**: function tracer, function_graph, event tracing
4. **debugfs**: tạo file debug trong `/sys/kernel/debug/`
5. **i2c-tools / devmem2**: debug hardware register trực tiếp
6. **OOPS decode**: `addr2line` + `objdump` xác định dòng code gây crash

---

## 9. Câu hỏi kiểm tra

1. `dev_dbg()` khác `dev_info()` thế nào? Khi nào `dev_dbg()` xuất hiện trong dmesg?
2. Cách bật dynamic debug cho tất cả hàm trong module "my_driver"?
3. ftrace function_graph tracer hiển thị gì?
4. `debugfs_create_x32()` tạo file debugfs dạng gì?
5. Đọc kernel OOPS: "PC is at my_probe+0x3c" nghĩa gì?

---

## 10. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 32 - Capstone Project**

Ta sẽ tích hợp tất cả kiến thức: GPIO + I2C + interrupt + Device Tree + sysfs + debugfs.
