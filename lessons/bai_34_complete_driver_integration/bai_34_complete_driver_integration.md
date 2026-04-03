# Bài 34 - Complete Driver Integration

> **Tham chiếu phần cứng BBB:** Xem [mapping/](../mapping/) để tra cứu thanh ghi và chân BBB.

## 1. Mục tiêu bài học
- Tích hợp tất cả kiến thức từ bài 1-30 vào một driver hoàn chỉnh
- Kết hợp: platform driver + DT binding + MMIO + IRQ + DMA + PM + sysfs + debugfs
- Viết driver cho AM335x GPIO controller hoàn chỉnh trên BBB
- Hiểu flow từ Device Tree → probe → runtime → remove

---

## 2. Kiến trúc driver tích hợp

```
┌─────────────────────────────────────────────────────────────┐
│  Userspace                                                   │
│  /dev/bbb-gpio  /sys/.../led_pattern  /sys/kernel/debug/..  │
└──────┬──────────────────┬────────────────────┬──────────────┘
       │                  │                    │
┌──────┴──────┐  ┌───────┴────────┐  ┌────────┴──────────┐
│ Char Device │  │     Sysfs      │  │     Debugfs       │
│ read/write  │  │ dev_groups     │  │ register dump     │
│ ioctl/poll  │  │ led_pattern    │  │ irq_count         │
└──────┬──────┘  └───────┬────────┘  └────────┬──────────┘
       │                 │                     │
┌──────┴─────────────────┴─────────────────────┴──────────────┐
│                    Driver Core                               │
│  ┌──────────┐ ┌──────────┐ ┌─────────┐ ┌─────────────────┐ │
│  │ GPIO     │ │ IRQ      │ │ Timer   │ │ Power Mgmt      │ │
│  │ gpio_chip│ │ threaded │ │ hrtimer │ │ pm_runtime      │ │
│  │ gpio_desc│ │ handler  │ │ pattern │ │ clk_get/enable  │ │
│  └────┬─────┘ └────┬─────┘ └────┬────┘ └───────┬─────────┘ │
│       │             │            │               │           │
│  ┌────┴─────────────┴────────────┴───────────────┴────────┐ │
│  │              MMIO Layer (readl/writel)                  │ │
│  │              devm_ioremap_resource                      │ │
│  └────────────────────────┬───────────────────────────────┘ │
└───────────────────────────┴─────────────────────────────────┘
                            │
              ┌─────────────┴──────────────┐
              │  AM335x GPIO1 @ 0x4804C000 │
              │  USR LED0-3, Button S2     │
              │  BeagleBone Black           │
              └────────────────────────────┘
```

---

## 3. BBB Connection — Phần cứng sử dụng

```
BeagleBone Black — Hardware cho bài tích hợp
═══════════════════════════════════════════════

 Onboard (không cần nối thêm):
 ┌──────────────────────────┐
 │  USR LED0 ← GPIO1_21    │  Dùng cho: LED pattern (sysfs)
 │  USR LED1 ← GPIO1_22    │  Dùng cho: LED pattern (sysfs)
 │  USR LED2 ← GPIO1_23    │  Dùng cho: IRQ indicator
 │  USR LED3 ← GPIO1_24    │  Dùng cho: heartbeat (timer)
 │  Button S2 → GPIO0_27   │  Dùng cho: IRQ trigger (input)
 └──────────────────────────┘

 AM335x Resources:
 ┌────────────────────────────────────────┐
 │  GPIO0 base: 0x44E07000               │
 │  GPIO1 base: 0x4804C000               │
 │  GPIO1 IRQ:  98 (INTC)                │
 │  CM_PER_GPIO1_CLKCTRL: 0x44E000AC     │
 │                                        │
 │  Control Module (pinmux):              │
 │  conf_gpmc_a5  (GPIO1_21): 0x44E10854 │
 │  conf_gpmc_a6  (GPIO1_22): 0x44E10858 │
 │  conf_gpmc_a7  (GPIO1_23): 0x44E1085C │
 │  conf_gpmc_a8  (GPIO1_24): 0x44E10860 │
 └────────────────────────────────────────┘
```

---

## 4. Driver hoàn chỉnh — bbb_integrated_driver.c

### 4.1 Header & Data structures

```c
/* bbb_integrated_driver.c — Complete integrated driver for BBB */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/gpio/driver.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/hrtimer.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/poll.h>

#define DRIVER_NAME     "bbb-integrated"
#define NUM_LEDS        4

/* AM335x GPIO Register Offsets */
#define GPIO_REVISION       0x000
#define GPIO_SYSCONFIG      0x010
#define GPIO_IRQSTATUS_0    0x02C
#define GPIO_IRQSTATUS_1    0x030
#define GPIO_IRQENABLE_0    0x034
#define GPIO_IRQENABLE_1    0x038
#define GPIO_OE             0x134
#define GPIO_DATAIN         0x138
#define GPIO_DATAOUT        0x13C
#define GPIO_LEVELDETECT0   0x140
#define GPIO_LEVELDETECT1   0x144
#define GPIO_RISINGDETECT   0x148
#define GPIO_FALLINGDETECT  0x14C
#define GPIO_SETDATAOUT     0x194
#define GPIO_CLEARDATAOUT   0x190

/* BBB specifics */
#define USR_LED0_BIT  21  /* GPIO1_21 */
#define USR_LED3_BIT  24  /* GPIO1_24 */
#define LED_BITS_MASK ((0xF) << USR_LED0_BIT)

struct bbb_dev {
    struct platform_device *pdev;
    void __iomem *base;
    struct clk *fclk;
    int irq;

    /* Char device */
    struct cdev cdev;
    dev_t devno;
    struct class *cls;

    /* GPIO chip */
    struct gpio_chip gc;

    /* Timer for LED heartbeat */
    struct hrtimer heartbeat_timer;
    bool heartbeat_state;
    u64 heartbeat_period_ns;

    /* IRQ tracking */
    spinlock_t lock;
    u32 irq_count;
    bool button_pressed;
    wait_queue_head_t wq;

    /* Sysfs */
    u8 led_pattern;  /* bitmask for LED0-3 */

    /* Debugfs */
    struct dentry *debugfs_dir;

    /* Mutex for char dev */
    struct mutex fops_lock;
};
```

### 4.2 GPIO chip operations

```c
static int bbb_gpio_direction_input(struct gpio_chip *gc, unsigned offset)
{
    struct bbb_dev *bdev = gpiochip_get_data(gc);
    unsigned long flags;
    u32 val;

    spin_lock_irqsave(&bdev->lock, flags);
    val = readl(bdev->base + GPIO_OE);
    val |= BIT(offset);  /* 1 = input */
    writel(val, bdev->base + GPIO_OE);
    spin_unlock_irqrestore(&bdev->lock, flags);

    return 0;
}

static int bbb_gpio_direction_output(struct gpio_chip *gc, unsigned offset, int value)
{
    struct bbb_dev *bdev = gpiochip_get_data(gc);
    unsigned long flags;
    u32 val;

    spin_lock_irqsave(&bdev->lock, flags);
    /* Set direction */
    val = readl(bdev->base + GPIO_OE);
    val &= ~BIT(offset);  /* 0 = output */
    writel(val, bdev->base + GPIO_OE);

    /* Set value */
    if (value)
        writel(BIT(offset), bdev->base + GPIO_SETDATAOUT);
    else
        writel(BIT(offset), bdev->base + GPIO_CLEARDATAOUT);
    spin_unlock_irqrestore(&bdev->lock, flags);

    return 0;
}

static int bbb_gpio_get(struct gpio_chip *gc, unsigned offset)
{
    struct bbb_dev *bdev = gpiochip_get_data(gc);
    return !!(readl(bdev->base + GPIO_DATAIN) & BIT(offset));
}

static void bbb_gpio_set(struct gpio_chip *gc, unsigned offset, int value)
{
    struct bbb_dev *bdev = gpiochip_get_data(gc);
    if (value)
        writel(BIT(offset), bdev->base + GPIO_SETDATAOUT);
    else
        writel(BIT(offset), bdev->base + GPIO_CLEARDATAOUT);
}
```

### 4.3 IRQ handler (threaded)

```c
static irqreturn_t bbb_gpio_irq_handler(int irq, void *dev_id)
{
    struct bbb_dev *bdev = dev_id;
    u32 status;

    status = readl(bdev->base + GPIO_IRQSTATUS_0);
    if (!status)
        return IRQ_NONE;

    /* Clear IRQ */
    writel(status, bdev->base + GPIO_IRQSTATUS_0);

    return IRQ_WAKE_THREAD;
}

static irqreturn_t bbb_gpio_irq_thread(int irq, void *dev_id)
{
    struct bbb_dev *bdev = dev_id;
    unsigned long flags;

    spin_lock_irqsave(&bdev->lock, flags);
    bdev->irq_count++;
    bdev->button_pressed = true;
    spin_unlock_irqrestore(&bdev->lock, flags);

    /* Toggle LED2 as IRQ indicator */
    if (readl(bdev->base + GPIO_DATAOUT) & BIT(USR_LED0_BIT + 2))
        writel(BIT(USR_LED0_BIT + 2), bdev->base + GPIO_CLEARDATAOUT);
    else
        writel(BIT(USR_LED0_BIT + 2), bdev->base + GPIO_SETDATAOUT);

    /* Wake up poll() waiters */
    wake_up_interruptible(&bdev->wq);

    dev_dbg(&bdev->pdev->dev, "IRQ #%u, button event\n", bdev->irq_count);
    return IRQ_HANDLED;
}
```

### 4.4 Heartbeat timer (hrtimer)

```c
static enum hrtimer_restart bbb_heartbeat_callback(struct hrtimer *timer)
{
    struct bbb_dev *bdev = container_of(timer, struct bbb_dev, heartbeat_timer);

    bdev->heartbeat_state = !bdev->heartbeat_state;

    /* Toggle LED3 */
    if (bdev->heartbeat_state)
        writel(BIT(USR_LED3_BIT), bdev->base + GPIO_SETDATAOUT);
    else
        writel(BIT(USR_LED3_BIT), bdev->base + GPIO_CLEARDATAOUT);

    hrtimer_forward_now(timer, ns_to_ktime(bdev->heartbeat_period_ns));
    return HRTIMER_RESTART;
}

static void bbb_heartbeat_start(struct bbb_dev *bdev)
{
    bdev->heartbeat_period_ns = 500000000ULL;  /* 500ms */
    hrtimer_init(&bdev->heartbeat_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    bdev->heartbeat_timer.function = bbb_heartbeat_callback;
    hrtimer_start(&bdev->heartbeat_timer,
                  ns_to_ktime(bdev->heartbeat_period_ns),
                  HRTIMER_MODE_REL);
}
```

### 4.5 Char device operations

```c
static int bbb_cdev_open(struct inode *inode, struct file *filp)
{
    struct bbb_dev *bdev = container_of(inode->i_cdev, struct bbb_dev, cdev);
    filp->private_data = bdev;
    return 0;
}

static ssize_t bbb_cdev_read(struct file *filp, char __user *buf,
                              size_t count, loff_t *ppos)
{
    struct bbb_dev *bdev = filp->private_data;
    char kbuf[64];
    int len;
    u32 datain;

    mutex_lock(&bdev->fops_lock);
    datain = readl(bdev->base + GPIO_DATAIN);
    len = snprintf(kbuf, sizeof(kbuf), "DATAIN=0x%08x IRQ_COUNT=%u\n",
                   datain, bdev->irq_count);
    mutex_unlock(&bdev->fops_lock);

    return simple_read_from_buffer(buf, count, ppos, kbuf, len);
}

static ssize_t bbb_cdev_write(struct file *filp, const char __user *buf,
                               size_t count, loff_t *ppos)
{
    struct bbb_dev *bdev = filp->private_data;
    char kbuf[16];
    u32 val;
    int ret;

    if (count > sizeof(kbuf) - 1)
        return -EINVAL;

    if (copy_from_user(kbuf, buf, count))
        return -EFAULT;
    kbuf[count] = '\0';

    ret = kstrtou32(kbuf, 0, &val);
    if (ret)
        return ret;

    mutex_lock(&bdev->fops_lock);
    /* Write to LED0-1 (bits 21-22) */
    val &= 0x03;
    writel((val << USR_LED0_BIT), bdev->base + GPIO_SETDATAOUT);
    writel(((~val & 0x03) << USR_LED0_BIT), bdev->base + GPIO_CLEARDATAOUT);
    mutex_unlock(&bdev->fops_lock);

    return count;
}

static __poll_t bbb_cdev_poll(struct file *filp, struct poll_table_struct *wait)
{
    struct bbb_dev *bdev = filp->private_data;
    __poll_t mask = 0;

    poll_wait(filp, &bdev->wq, wait);

    if (bdev->button_pressed) {
        mask |= EPOLLIN | EPOLLRDNORM;
        bdev->button_pressed = false;
    }

    return mask;
}

static const struct file_operations bbb_cdev_fops = {
    .owner   = THIS_MODULE,
    .open    = bbb_cdev_open,
    .read    = bbb_cdev_read,
    .write   = bbb_cdev_write,
    .poll    = bbb_cdev_poll,
};
```

### 4.6 Sysfs — LED pattern control

```c
static ssize_t led_pattern_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    struct bbb_dev *bdev = dev_get_drvdata(dev);
    return sysfs_emit(buf, "0x%02x\n", bdev->led_pattern);
}

static ssize_t led_pattern_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    struct bbb_dev *bdev = dev_get_drvdata(dev);
    u8 val;
    int ret;

    ret = kstrtou8(buf, 0, &val);
    if (ret)
        return ret;

    val &= 0x03;  /* LED0-1 only (LED2=IRQ, LED3=heartbeat) */
    bdev->led_pattern = val;

    writel((val << USR_LED0_BIT), bdev->base + GPIO_SETDATAOUT);
    writel(((~val & 0x03) << USR_LED0_BIT), bdev->base + GPIO_CLEARDATAOUT);

    return count;
}
static DEVICE_ATTR_RW(led_pattern);

static ssize_t heartbeat_period_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    struct bbb_dev *bdev = dev_get_drvdata(dev);
    return sysfs_emit(buf, "%llu\n", bdev->heartbeat_period_ns / 1000000);
}

static ssize_t heartbeat_period_store(struct device *dev,
                                      struct device_attribute *attr,
                                      const char *buf, size_t count)
{
    struct bbb_dev *bdev = dev_get_drvdata(dev);
    u64 ms;
    int ret;

    ret = kstrtou64(buf, 0, &ms);
    if (ret)
        return ret;

    if (ms < 50 || ms > 5000)
        return -EINVAL;

    bdev->heartbeat_period_ns = ms * 1000000ULL;
    return count;
}
static DEVICE_ATTR_RW(heartbeat_period);

static struct attribute *bbb_dev_attrs[] = {
    &dev_attr_led_pattern.attr,
    &dev_attr_heartbeat_period.attr,
    NULL,
};
ATTRIBUTE_GROUPS(bbb_dev);
```

### 4.7 Debugfs — Register dump

```c
static int bbb_regs_show(struct seq_file *s, void *data)
{
    struct bbb_dev *bdev = s->private;

    seq_puts(s, "=== AM335x GPIO1 Complete Register Dump ===\n");
    seq_printf(s, "GPIO_REVISION     [0x000] = 0x%08x\n", readl(bdev->base + 0x000));
    seq_printf(s, "GPIO_SYSCONFIG    [0x010] = 0x%08x\n", readl(bdev->base + 0x010));
    seq_printf(s, "GPIO_IRQSTATUS_0  [0x02C] = 0x%08x\n", readl(bdev->base + 0x02C));
    seq_printf(s, "GPIO_IRQENABLE_0  [0x034] = 0x%08x\n", readl(bdev->base + 0x034));
    seq_printf(s, "GPIO_OE           [0x134] = 0x%08x\n", readl(bdev->base + 0x134));
    seq_printf(s, "GPIO_DATAIN       [0x138] = 0x%08x\n", readl(bdev->base + 0x138));
    seq_printf(s, "GPIO_DATAOUT      [0x13C] = 0x%08x\n", readl(bdev->base + 0x13C));
    seq_printf(s, "\n--- Driver State ---\n");
    seq_printf(s, "IRQ count    : %u\n", bdev->irq_count);
    seq_printf(s, "LED pattern  : 0x%02x\n", bdev->led_pattern);
    seq_printf(s, "Heartbeat ns : %llu\n", bdev->heartbeat_period_ns);

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(bbb_regs);
```

### 4.8 Probe & Remove — tích hợp tất cả

```c
static int bbb_integrated_probe(struct platform_device *pdev)
{
    struct bbb_dev *bdev;
    struct resource *res;
    int ret;

    bdev = devm_kzalloc(&pdev->dev, sizeof(*bdev), GFP_KERNEL);
    if (!bdev)
        return -ENOMEM;

    bdev->pdev = pdev;
    spin_lock_init(&bdev->lock);
    mutex_init(&bdev->fops_lock);
    init_waitqueue_head(&bdev->wq);
    platform_set_drvdata(pdev, bdev);

    /* 1. Clock enable */
    bdev->fclk = devm_clk_get(&pdev->dev, "fck");
    if (IS_ERR(bdev->fclk)) {
        dev_warn(&pdev->dev, "No fck clock, using pm_runtime\n");
        bdev->fclk = NULL;
    }

    pm_runtime_enable(&pdev->dev);
    ret = pm_runtime_resume_and_get(&pdev->dev);
    if (ret < 0)
        goto err_pm;

    /* 2. MMIO mapping */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    bdev->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(bdev->base)) {
        ret = PTR_ERR(bdev->base);
        goto err_pm;
    }

    dev_info(&pdev->dev, "GPIO revision: 0x%08x\n",
             readl(bdev->base + GPIO_REVISION));

    /* 3. IRQ */
    bdev->irq = platform_get_irq(pdev, 0);
    if (bdev->irq > 0) {
        ret = devm_request_threaded_irq(&pdev->dev, bdev->irq,
                                        bbb_gpio_irq_handler,
                                        bbb_gpio_irq_thread,
                                        IRQF_SHARED,
                                        DRIVER_NAME, bdev);
        if (ret) {
            dev_err(&pdev->dev, "Failed to request IRQ %d\n", bdev->irq);
            goto err_pm;
        }
    }

    /* 4. GPIO chip */
    bdev->gc.label = DRIVER_NAME;
    bdev->gc.parent = &pdev->dev;
    bdev->gc.owner = THIS_MODULE;
    bdev->gc.base = -1;
    bdev->gc.ngpio = 32;
    bdev->gc.direction_input = bbb_gpio_direction_input;
    bdev->gc.direction_output = bbb_gpio_direction_output;
    bdev->gc.get = bbb_gpio_get;
    bdev->gc.set = bbb_gpio_set;

    ret = devm_gpiochip_add_data(&pdev->dev, &bdev->gc, bdev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to add gpio_chip\n");
        goto err_pm;
    }

    /* 5. Char device */
    ret = alloc_chrdev_region(&bdev->devno, 0, 1, DRIVER_NAME);
    if (ret)
        goto err_pm;

    cdev_init(&bdev->cdev, &bbb_cdev_fops);
    ret = cdev_add(&bdev->cdev, bdev->devno, 1);
    if (ret)
        goto err_chrdev;

    bdev->cls = class_create(DRIVER_NAME);
    if (IS_ERR(bdev->cls)) {
        ret = PTR_ERR(bdev->cls);
        goto err_cdev;
    }
    device_create(bdev->cls, &pdev->dev, bdev->devno, NULL, "bbb-gpio");

    /* 6. Heartbeat timer on LED3 */
    bbb_heartbeat_start(bdev);

    /* 7. Debugfs */
    bdev->debugfs_dir = debugfs_create_dir(DRIVER_NAME, NULL);
    debugfs_create_file("registers", 0444, bdev->debugfs_dir, bdev, &bbb_regs_fops);

    dev_info(&pdev->dev, "BBB integrated driver loaded: MMIO + IRQ + GPIO + cdev + sysfs + debugfs + timer\n");
    return 0;

err_cdev:
    cdev_del(&bdev->cdev);
err_chrdev:
    unregister_chrdev_region(bdev->devno, 1);
err_pm:
    pm_runtime_put_sync(&pdev->dev);
    pm_runtime_disable(&pdev->dev);
    return ret;
}

static void bbb_integrated_remove(struct platform_device *pdev)
{
    struct bbb_dev *bdev = platform_get_drvdata(pdev);

    hrtimer_cancel(&bdev->heartbeat_timer);
    debugfs_remove_recursive(bdev->debugfs_dir);
    device_destroy(bdev->cls, bdev->devno);
    class_destroy(bdev->cls);
    cdev_del(&bdev->cdev);
    unregister_chrdev_region(bdev->devno, 1);

    /* Turn off all LEDs */
    writel(LED_BITS_MASK, bdev->base + GPIO_CLEARDATAOUT);

    pm_runtime_put_sync(&pdev->dev);
    pm_runtime_disable(&pdev->dev);

    dev_info(&pdev->dev, "BBB integrated driver removed\n");
}
```

### 4.9 Module registration

```c
static const struct of_device_id bbb_integrated_of_match[] = {
    { .compatible = "ti,bbb-integrated-gpio" },
    { },
};
MODULE_DEVICE_TABLE(of, bbb_integrated_of_match);

static const struct dev_pm_ops bbb_pm_ops = {
    SET_RUNTIME_PM_OPS(
        /* suspend */ NULL,
        /* resume  */ NULL,
        NULL
    )
};

static struct platform_driver bbb_integrated_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = bbb_integrated_of_match,
        .pm = &bbb_pm_ops,
        .dev_groups = bbb_dev_groups,
    },
    .probe  = bbb_integrated_probe,
    .remove = bbb_integrated_remove,
};
module_platform_driver(bbb_integrated_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BBB Student");
MODULE_DESCRIPTION("Complete integrated driver for BeagleBone Black GPIO1");
```

---

## 5. Device Tree

```dts
/* DTS overlay cho BBB integrated driver */
/dts-v1/;
/plugin/;

&{/} {
    bbb_integrated: bbb-integrated-gpio@4804c000 {
        compatible = "ti,bbb-integrated-gpio";
        reg = <0x4804c000 0x1000>;
        interrupts = <98>;
        interrupt-parent = <&intc>;
        clocks = <&gpio1_clk>;
        clock-names = "fck";

        pinctrl-names = "default";
        pinctrl-0 = <&bbb_integrated_pins>;

        status = "okay";
    };
};

&am33xx_pinmux {
    bbb_integrated_pins: bbb-integrated-pins {
        pinctrl-single,pins = <
            0x054 0x07  /* P8.20 gpio1_21 MODE7 output (USR LED0) */
            0x058 0x07  /* P8.21 gpio1_22 MODE7 output (USR LED1) */
            0x05C 0x07  /* P8.22 gpio1_23 MODE7 output (USR LED2) */
            0x060 0x07  /* P8.23 gpio1_24 MODE7 output (USR LED3) */
        >;
    };
};
```

---

## 6. Makefile

```makefile
obj-m := bbb_integrated_driver.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Cross-compile for BBB
cross:
	$(MAKE) -C $(KDIR) M=$(PWD) \
		ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules
```

---

## 7. Test trên BBB

```bash
# 1. Load module
insmod bbb_integrated_driver.ko

# 2. Verify dmesg
dmesg | tail
# [  123.456] bbb-integrated 4804c000.bbb-integrated-gpio: GPIO revision: 0x50600801
# [  123.457] bbb-integrated 4804c000.bbb-integrated-gpio: BBB integrated driver loaded

# 3. Sysfs — LED pattern
echo 0x03 > /sys/devices/platform/.../led_pattern    # LED0 + LED1 ON
cat /sys/devices/platform/.../led_pattern             # 0x03
echo 250 > /sys/devices/platform/.../heartbeat_period # 250ms heartbeat

# 4. Char device — read status
cat /dev/bbb-gpio
# DATAIN=0x04060000 IRQ_COUNT=0

# 5. Char device — write LEDs
echo 0x01 > /dev/bbb-gpio   # LED0 ON

# 6. Debugfs — register dump
cat /sys/kernel/debug/bbb-integrated/registers
# === AM335x GPIO1 Complete Register Dump ===
# GPIO_REVISION     [0x000] = 0x50600801
# GPIO_OE           [0x134] = 0xfe1fffff
# ...
# --- Driver State ---
# IRQ count    : 3
# LED pattern  : 0x01

# 7. Test button IRQ (press S2 button)
# Watch LED2 toggle per press
# dmesg | grep "button event"

# 8. Poll test (C userspace)
# fd = open("/dev/bbb-gpio", O_RDONLY);
# poll(&pfd, 1, 5000);  /* blocks until button press */

# 9. Unload
rmmod bbb_integrated_driver
```

---

## 8. Checklist kiến thức tích hợp

| Bài | Kiến thức | Dùng trong driver |
|-----|-----------|-------------------|
| 5 | Device Tree | DTS overlay, compatible, reg, interrupts |
| 6 | MMIO | `devm_ioremap_resource`, `readl/writel` |
| 7 | Platform driver | `platform_driver`, `probe/remove` |
| 8 | devm | `devm_kzalloc`, `devm_request_threaded_irq`, `devm_gpiochip_add_data` |
| 9 | GPIO hardware | GPIO_OE, SETDATAOUT, CLEARDATAOUT registers |
| 10 | gpio_chip | `gpio_chip`, `direction_input/output`, `get/set` |
| 12 | Interrupt | Threaded IRQ, IRQSTATUS/IRQENABLE |
| 13 | Concurrency | `spinlock`, `mutex`, `wait_queue` |
| 14-15 | Char device | `cdev`, `file_operations`, `poll` |
| 16 | Timer | `hrtimer` for LED heartbeat |
| 26 | PM | `pm_runtime_enable`, `clk_get` |
| 28 | Sysfs/Debugfs | `DEVICE_ATTR_RW`, `debugfs_create_file`, `seq_file` |

---

## 9. Câu hỏi kiểm tra

1. Tại sao dùng `spinlock_irqsave` trong GPIO ops nhưng `mutex` trong char fops?
2. Driver này thiếu gì so với upstream `omap-gpio` driver?
3. Nếu muốn thêm I2C sensor (bài 18-19), bạn sẽ tích hợp thế nào?
4. Tại sao dùng `devm_request_threaded_irq` thay vì `request_irq`?
5. Heartbeat timer dùng `hrtimer` — so sánh với `mod_timer` ở đây.

---

Bài tiếp theo: **Bài 32 - Capstone Project**

Tích hợp GPIO + I2C + PWM + ADC + DMA + PM + DT overlay vào sensor hub driver hoàn chỉnh.
