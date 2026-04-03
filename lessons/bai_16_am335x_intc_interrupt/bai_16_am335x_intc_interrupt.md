# B�i 16 - AM335x INTC & Interrupt Handling

> **Tham chi?u ph?n c?ng BBB:** Xem [intc.md](../mapping/intc.md)

## 0. BBB Connection � Interrupt Sources tr�n BBB

```
AM335x INTC @ 0x48200000
------------------------
 128 interrupt lines ? Cortex-A8

  Peripheral          IRQ#   Trigger source tr�n BBB
  -----------------------------------------------------
  GPIO0               96     Button S2 (GPIO0_27)
  GPIO1               98     USR LEDs (GPIO1_21-24)
  GPIO2               32     P8 header pins
  GPIO3               62     P9 header pins
  DMTIMER2            68     Kernel timer
  I2C2                30     P9.19/P9.20 sensor
  McSPI0              65     P9.17-P9.22
  UART1               73     P9.24/P9.26
  TSCADC              16     P9.33-P9.39 analog
  EDMA CC0            12     DMA completion
  WDT1                91     Watchdog timeout
```

### Test nhanh interrupt tr�n BBB
```bash
# Xem interrupt counters
cat /proc/interrupts | grep -E 'gpio|timer|i2c'

# GPIO interrupt cho button S2 (GPIO0_27)
# D�ng trong driver: devm_request_irq(dev, gpio_to_irq(27), ...)
```

---

## 1. M?c ti�u b�i h?c
- Hi?u interrupt trong Linux kernel: hardware IRQ, IRQ number, IRQ domain
- �ang k� interrupt handler v?i `request_irq()` / `devm_request_irq()`
- Vi?t top-half handler v� bottom-half (threaded IRQ, tasklet, workqueue)
- X? l� interrupt t? GPIO tr�n AM335x
- N?m nguy�n t?c: handler ph?i nhanh, kh�ng sleep

---

## 2. Interrupt trong Linux Kernel

### 2.1 Flow interrupt tr�n AM335x:

```
GPIO pin thay d?i
       �
       ?
GPIO module ph�t IRQ ? INTC (AM335x Interrupt Controller)
                               �
                               ?
                        CPU nh?n interrupt
                               �
                               ?
                    Linux kernel dispatch
                               �
                               ?
                    Driver's IRQ handler
```

### 2.2 IRQ Number:

AM335x c� INTC (Interrupt Controller) qu?n l� ~128 interrupt lines.

| Peripheral | IRQ Number (INTC) |
|-----------|-------------------|
| GPIO0 | 96, 97 |
| GPIO1 | 98, 99 |
| GPIO2 | 32, 33 |
| GPIO3 | 62, 63 |
| UART0 | 72 |
| I2C0 | 70 |
| SPI0 | 65 |

Ngu?n: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 6 - Interrupt Controller

**Luu �**: Linux c� th? remap IRQ number qua IRQ domain. D�ng `platform_get_irq()` thay v� hardcode.

---

## 3. �ang k� Interrupt Handler

### 3.1 `request_irq()`:

```c
#include <linux/interrupt.h>

int request_irq(unsigned int irq,
                irq_handler_t handler,
                unsigned long flags,
                const char *name,
                void *dev_id);
```

- **irq**: IRQ number (t? `platform_get_irq()`)
- **handler**: H�m x? l� interrupt
- **flags**: `IRQF_SHARED`, `IRQF_TRIGGER_RISING`, ...
- **name**: T�n hi?n th? trong `/proc/interrupts`
- **dev_id**: Pointer truy?n cho handler (thu?ng l� device struct)

### 3.2 IRQ Flags:

```c
/* Trigger */
IRQF_TRIGGER_RISING     /* C?nh l�n */
IRQF_TRIGGER_FALLING    /* C?nh xu?ng */
IRQF_TRIGGER_HIGH       /* M?c cao */
IRQF_TRIGGER_LOW        /* M?c th?p */

/* Options */
IRQF_SHARED             /* Nhi?u driver share 1 IRQ line */
IRQF_ONESHOT            /* Disable IRQ cho d?n khi threaded handler xong */
```

### 3.3 Handler function:

```c
static irqreturn_t my_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    /* Ki?m tra xem interrupt c� ph?i c?a m�nh kh�ng */
    u32 status = readl(dev->base + IRQ_STATUS_REG);
    if (!(status & MY_IRQ_BIT))
        return IRQ_NONE;    /* Kh�ng ph?i c?a m�nh */

    /* X? l� interrupt */
    /* ... */

    /* Acknowledge interrupt (x�a pending bit) */
    writel(MY_IRQ_BIT, dev->base + IRQ_STATUS_REG);

    return IRQ_HANDLED;     /* �� x? l� */
}
```

### 3.4 Return values:

| Value | � nghia |
|-------|---------|
| `IRQ_NONE` | Interrupt kh�ng ph?i c?a driver n�y |
| `IRQ_HANDLED` | Interrupt d� x? l� xong |
| `IRQ_WAKE_THREAD` | C?n ch?y threaded handler (bottom half) |

---

## 4. Top-Half vs Bottom-Half

### 4.1 V?n d?:

Interrupt handler ch?y trong **interrupt context**:
- **Kh�ng du?c sleep** (kh�ng mutex, kmalloc v?i GFP_KERNEL, copy_to/from_user)
- **Ph?i nhanh** (interrupt b? disable trong l�c handler ch?y)
- **Kh�ng th? g?i scheduler**

? Chia th�nh 2 ph?n:

```
Interrupt x?y ra
       �
       ?
+-----------------+
�   Top Half      �  ? Ch?y trong IRQ context
�   (handler)     �  ? Nhanh: ack IRQ, d?c status, schedule bottom half
+-----------------+
         �
         ?
+-----------------+
�   Bottom Half   �  ? Ch?y trong process context (c� th? sleep)
�   (threaded/wq) �  ? Ch?m: x? l� data, wake userspace, I/O
+-----------------+
```

### 4.2 Threaded IRQ (khuy�n d�ng):

```c
static irqreturn_t my_hard_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    /* Top half: ch? ack interrupt */
    writel(IRQ_BIT, dev->base + IRQ_STATUS);

    return IRQ_WAKE_THREAD;  /* K�ch ho?t thread handler */
}

static irqreturn_t my_thread_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    /* Bottom half: x? l� n?ng, C� TH? SLEEP */
    mutex_lock(&dev->lock);
    /* �?c data t? hardware, x? l�, th�ng b�o userspace */
    mutex_unlock(&dev->lock);

    return IRQ_HANDLED;
}

/* �ang k� threaded IRQ */
devm_request_threaded_irq(&pdev->dev, irq,
                           my_hard_handler,    /* top half */
                           my_thread_handler,  /* bottom half */
                           IRQF_ONESHOT,
                           "my-device",
                           my_dev);
```

### 4.3 Tasklet (bottom half nh?):

```c
#include <linux/interrupt.h>

static void my_tasklet_fn(struct tasklet_struct *t)
{
    struct my_device *dev = from_tasklet(dev, t, tasklet);
    /* X? l� trong softirq context (kh�ng sleep du?c) */
}

/* Khai b�o */
struct my_device {
    struct tasklet_struct tasklet;
};

/* Init */
tasklet_init(&dev->tasklet, my_tasklet_fn, 0);

/* Trong top half: */
tasklet_schedule(&dev->tasklet);

/* Cleanup: */
tasklet_kill(&dev->tasklet);
```

### 4.4 Workqueue (bottom half n?ng):

```c
#include <linux/workqueue.h>

static void my_work_fn(struct work_struct *work)
{
    struct my_device *dev = container_of(work, struct my_device, work);
    /* C� TH? SLEEP ? d�y */
}

struct my_device {
    struct work_struct work;
};

/* Init */
INIT_WORK(&dev->work, my_work_fn);

/* Trong top half: */
schedule_work(&dev->work);
```

---

## 5. GPIO Interrupt tr�n AM335x

### 5.1 GPIO interrupt registers:

```c
#define GPIO_IRQSTATUS_0       0x02C  /* IRQ status cho line 0 */
#define GPIO_IRQSTATUS_1       0x030  /* IRQ status cho line 1 */
#define GPIO_IRQSTATUS_SET_0   0x034  /* Enable IRQ */
#define GPIO_IRQSTATUS_CLR_0   0x03C  /* Disable IRQ */
#define GPIO_RISINGDETECT      0x148  /* Rising edge detect */
#define GPIO_FALLINGDETECT     0x14C  /* Falling edge detect */
```

Ngu?n: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 25 - GPIO

### 5.2 V� d?: Button interrupt driver:

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>

#define GPIO_IRQSTATUS_0     0x02C
#define GPIO_IRQSTATUS_SET_0 0x034
#define GPIO_RISINGDETECT    0x148
#define GPIO_FALLINGDETECT   0x14C

struct button_dev {
	void __iomem *base;
	int irq;
	int gpio_bit;
	int press_count;
};

static irqreturn_t button_handler(int irq, void *dev_id)
{
	struct button_dev *btn = dev_id;
	u32 status;

	/* �?c interrupt status */
	status = readl(btn->base + GPIO_IRQSTATUS_0);
	if (!(status & (1 << btn->gpio_bit)))
		return IRQ_NONE;

	/* Acknowledge interrupt */
	writel((1 << btn->gpio_bit), btn->base + GPIO_IRQSTATUS_0);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t button_thread(int irq, void *dev_id)
{
	struct button_dev *btn = dev_id;

	btn->press_count++;
	pr_info("Button pressed! Count: %d\n", btn->press_count);

	return IRQ_HANDLED;
}

static int button_probe(struct platform_device *pdev)
{
	struct button_dev *btn;
	struct resource *res;
	u32 val;

	btn = devm_kzalloc(&pdev->dev, sizeof(*btn), GFP_KERNEL);
	if (!btn)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	btn->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(btn->base))
		return PTR_ERR(btn->base);

	btn->irq = platform_get_irq(pdev, 0);
	if (btn->irq < 0)
		return btn->irq;

	of_property_read_u32(pdev->dev.of_node, "button-bit", &btn->gpio_bit);

	/* Enable falling edge detect */
	val = readl(btn->base + GPIO_FALLINGDETECT);
	val |= (1 << btn->gpio_bit);
	writel(val, btn->base + GPIO_FALLINGDETECT);

	/* Enable interrupt cho bit n�y */
	writel((1 << btn->gpio_bit), btn->base + GPIO_IRQSTATUS_SET_0);

	/* �ang k� threaded IRQ */
	if (devm_request_threaded_irq(&pdev->dev, btn->irq,
	                               button_handler,
	                               button_thread,
	                               IRQF_ONESHOT | IRQF_SHARED,
	                               "bbb-button", btn))
		return -ENODEV;

	platform_set_drvdata(pdev, btn);
	dev_info(&pdev->dev, "Button driver probed, GPIO bit %d\n",
	         btn->gpio_bit);
	return 0;
}

static const struct of_device_id button_of_match[] = {
	{ .compatible = "bbb,button" },
	{ },
};
MODULE_DEVICE_TABLE(of, button_of_match);

static struct platform_driver button_driver = {
	.probe = button_probe,
	.driver = {
		.name = "bbb-button",
		.of_match_table = button_of_match,
	},
};

module_platform_driver(button_driver);
MODULE_LICENSE("GPL");
```

---

## 6. Xem interrupt statistics:

```bash
# Tr�n BBB:
cat /proc/interrupts
#            CPU0
# 96:       123  INTC  96 Edge  44e07000.gpio
# 98:       456  INTC  98 Edge  4804c000.gpio
```

---

## 7. Ki?n th?c c?t l�i sau b�i n�y

1. **request_irq** / **devm_request_irq**: dang k� handler cho IRQ
2. **Top half**: nhanh, kh�ng sleep, ack IRQ ? return IRQ_HANDLED ho?c IRQ_WAKE_THREAD
3. **Threaded IRQ**: bottom half ch?y trong process context, c� th? sleep
4. **IRQF_ONESHOT**: disable IRQ cho d?n khi thread handler xong
5. GPIO interrupt c?n: enable detect (rising/falling) + enable IRQ + ack khi x? l� xong

---

## 8. C�u h?i ki?m tra

1. T?i sao top-half handler kh�ng du?c sleep?
2. S? kh�c nhau gi?a threaded IRQ, tasklet, v� workqueue?
3. IRQF_ONESHOT d�ng d? l�m g�?
4. Khi IRQ handler return `IRQ_NONE`, di?u g� x?y ra?
5. Vi?t DT node cho device c� 2 interrupt lines.

---

## 9. Chu?n b? cho b�i sau

B�i ti?p theo: **B�i 13 - GPIO Subsystem t?ng quan**

Ta s? h?c gpiolib architecture trong kernel: gpio_chip, GPIO controller driver, c�ch kernel qu?n l� GPIO.
