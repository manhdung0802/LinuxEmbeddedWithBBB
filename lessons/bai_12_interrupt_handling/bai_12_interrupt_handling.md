# Bài 12 - Interrupt Handling

## 1. Mục tiêu bài học
- Hiểu interrupt trong Linux kernel: hardware IRQ, IRQ number, IRQ domain
- Đăng ký interrupt handler với `request_irq()` / `devm_request_irq()`
- Viết top-half handler và bottom-half (threaded IRQ, tasklet, workqueue)
- Xử lý interrupt từ GPIO trên AM335x
- Nắm nguyên tắc: handler phải nhanh, không sleep

---

## 2. Interrupt trong Linux Kernel

### 2.1 Flow interrupt trên AM335x:

```
GPIO pin thay đổi
       │
       ▼
GPIO module phát IRQ → INTC (AM335x Interrupt Controller)
                               │
                               ▼
                        CPU nhận interrupt
                               │
                               ▼
                    Linux kernel dispatch
                               │
                               ▼
                    Driver's IRQ handler
```

### 2.2 IRQ Number:

AM335x có INTC (Interrupt Controller) quản lý ~128 interrupt lines.

| Peripheral | IRQ Number (INTC) |
|-----------|-------------------|
| GPIO0 | 96, 97 |
| GPIO1 | 98, 99 |
| GPIO2 | 32, 33 |
| GPIO3 | 62, 63 |
| UART0 | 72 |
| I2C0 | 70 |
| SPI0 | 65 |

Nguồn: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 6 - Interrupt Controller

**Lưu ý**: Linux có thể remap IRQ number qua IRQ domain. Dùng `platform_get_irq()` thay vì hardcode.

---

## 3. Đăng ký Interrupt Handler

### 3.1 `request_irq()`:

```c
#include <linux/interrupt.h>

int request_irq(unsigned int irq,
                irq_handler_t handler,
                unsigned long flags,
                const char *name,
                void *dev_id);
```

- **irq**: IRQ number (từ `platform_get_irq()`)
- **handler**: Hàm xử lý interrupt
- **flags**: `IRQF_SHARED`, `IRQF_TRIGGER_RISING`, ...
- **name**: Tên hiển thị trong `/proc/interrupts`
- **dev_id**: Pointer truyền cho handler (thường là device struct)

### 3.2 IRQ Flags:

```c
/* Trigger */
IRQF_TRIGGER_RISING     /* Cạnh lên */
IRQF_TRIGGER_FALLING    /* Cạnh xuống */
IRQF_TRIGGER_HIGH       /* Mức cao */
IRQF_TRIGGER_LOW        /* Mức thấp */

/* Options */
IRQF_SHARED             /* Nhiều driver share 1 IRQ line */
IRQF_ONESHOT            /* Disable IRQ cho đến khi threaded handler xong */
```

### 3.3 Handler function:

```c
static irqreturn_t my_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    /* Kiểm tra xem interrupt có phải của mình không */
    u32 status = readl(dev->base + IRQ_STATUS_REG);
    if (!(status & MY_IRQ_BIT))
        return IRQ_NONE;    /* Không phải của mình */

    /* Xử lý interrupt */
    /* ... */

    /* Acknowledge interrupt (xóa pending bit) */
    writel(MY_IRQ_BIT, dev->base + IRQ_STATUS_REG);

    return IRQ_HANDLED;     /* Đã xử lý */
}
```

### 3.4 Return values:

| Value | Ý nghĩa |
|-------|---------|
| `IRQ_NONE` | Interrupt không phải của driver này |
| `IRQ_HANDLED` | Interrupt đã xử lý xong |
| `IRQ_WAKE_THREAD` | Cần chạy threaded handler (bottom half) |

---

## 4. Top-Half vs Bottom-Half

### 4.1 Vấn đề:

Interrupt handler chạy trong **interrupt context**:
- **Không được sleep** (không mutex, kmalloc với GFP_KERNEL, copy_to/from_user)
- **Phải nhanh** (interrupt bị disable trong lúc handler chạy)
- **Không thể gọi scheduler**

→ Chia thành 2 phần:

```
Interrupt xảy ra
       │
       ▼
┌─────────────────┐
│   Top Half      │  ← Chạy trong IRQ context
│   (handler)     │  ← Nhanh: ack IRQ, đọc status, schedule bottom half
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   Bottom Half   │  ← Chạy trong process context (có thể sleep)
│   (threaded/wq) │  ← Chậm: xử lý data, wake userspace, I/O
└─────────────────┘
```

### 4.2 Threaded IRQ (khuyên dùng):

```c
static irqreturn_t my_hard_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    /* Top half: chỉ ack interrupt */
    writel(IRQ_BIT, dev->base + IRQ_STATUS);

    return IRQ_WAKE_THREAD;  /* Kích hoạt thread handler */
}

static irqreturn_t my_thread_handler(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    /* Bottom half: xử lý nặng, CÓ THỂ SLEEP */
    mutex_lock(&dev->lock);
    /* Đọc data từ hardware, xử lý, thông báo userspace */
    mutex_unlock(&dev->lock);

    return IRQ_HANDLED;
}

/* Đăng ký threaded IRQ */
devm_request_threaded_irq(&pdev->dev, irq,
                           my_hard_handler,    /* top half */
                           my_thread_handler,  /* bottom half */
                           IRQF_ONESHOT,
                           "my-device",
                           my_dev);
```

### 4.3 Tasklet (bottom half nhẹ):

```c
#include <linux/interrupt.h>

static void my_tasklet_fn(struct tasklet_struct *t)
{
    struct my_device *dev = from_tasklet(dev, t, tasklet);
    /* Xử lý trong softirq context (không sleep được) */
}

/* Khai báo */
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

### 4.4 Workqueue (bottom half nặng):

```c
#include <linux/workqueue.h>

static void my_work_fn(struct work_struct *work)
{
    struct my_device *dev = container_of(work, struct my_device, work);
    /* CÓ THỂ SLEEP ở đây */
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

## 5. GPIO Interrupt trên AM335x

### 5.1 GPIO interrupt registers:

```c
#define GPIO_IRQSTATUS_0       0x02C  /* IRQ status cho line 0 */
#define GPIO_IRQSTATUS_1       0x030  /* IRQ status cho line 1 */
#define GPIO_IRQSTATUS_SET_0   0x034  /* Enable IRQ */
#define GPIO_IRQSTATUS_CLR_0   0x03C  /* Disable IRQ */
#define GPIO_RISINGDETECT      0x148  /* Rising edge detect */
#define GPIO_FALLINGDETECT     0x14C  /* Falling edge detect */
```

Nguồn: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 25 - GPIO

### 5.2 Ví dụ: Button interrupt driver:

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

	/* Đọc interrupt status */
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

	/* Enable interrupt cho bit này */
	writel((1 << btn->gpio_bit), btn->base + GPIO_IRQSTATUS_SET_0);

	/* Đăng ký threaded IRQ */
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
# Trên BBB:
cat /proc/interrupts
#            CPU0
# 96:       123  INTC  96 Edge  44e07000.gpio
# 98:       456  INTC  98 Edge  4804c000.gpio
```

---

## 7. Kiến thức cốt lõi sau bài này

1. **request_irq** / **devm_request_irq**: đăng ký handler cho IRQ
2. **Top half**: nhanh, không sleep, ack IRQ → return IRQ_HANDLED hoặc IRQ_WAKE_THREAD
3. **Threaded IRQ**: bottom half chạy trong process context, có thể sleep
4. **IRQF_ONESHOT**: disable IRQ cho đến khi thread handler xong
5. GPIO interrupt cần: enable detect (rising/falling) + enable IRQ + ack khi xử lý xong

---

## 8. Câu hỏi kiểm tra

1. Tại sao top-half handler không được sleep?
2. Sự khác nhau giữa threaded IRQ, tasklet, và workqueue?
3. IRQF_ONESHOT dùng để làm gì?
4. Khi IRQ handler return `IRQ_NONE`, điều gì xảy ra?
5. Viết DT node cho device có 2 interrupt lines.

---

## 9. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 13 - GPIO Subsystem tổng quan**

Ta sẽ học gpiolib architecture trong kernel: gpio_chip, GPIO controller driver, cách kernel quản lý GPIO.
