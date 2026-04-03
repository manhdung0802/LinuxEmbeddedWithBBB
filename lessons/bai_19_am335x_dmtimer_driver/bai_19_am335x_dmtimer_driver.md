# B�i 19 - AM335x DMTIMER Driver

> **Tham chiếu phần cứng BBB:** Xem [timer.md](../mapping/timer.md)

## 0. BBB Connection � DMTIMER tr�n BBB

```
AM335x DMTIMERs tr�n BeagleBone Black
---------------------------------------
 Timer   Base Address   IRQ   Clock Source     Ghi ch�
 ---------------------------------------------------------
 TIMER0  0x44E05000     66    CLK_M_OSC (24MHz) Secure, kh�ng d�ng
 TIMER1  0x44E31000     67    CLK_32KHZ         1ms tick (kernel)
 TIMER2  0x48040000     68    CLK_M_OSC         ? D�ng cho b�i h?c
 TIMER3  0x48042000     69    CLK_M_OSC         C� th? d�ng
 TIMER4  0x48044000     92    CLK_M_OSC         C� th? d�ng
 TIMER5  0x48046000     93    CLK_M_OSC         C� th? d�ng
 TIMER6  0x48048000     94    CLK_M_OSC         C� th? d�ng
 TIMER7  0x4804A000     95    CLK_M_OSC         C� th? d�ng

 TIMER2 clock enable: CM_PER_TIMER2_CLKCTRL @ 0x44E00080

 ?ng d?ng tr�n BBB:
 - T?o periodic interrupt ? toggle USR LED3 (GPIO1_24)
 - Measure button press duration (capture mode)
 - Generate precise timing for PWM software
```

---

## 1. M?c ti�u b�i h?c
- Hi?u kernel timer APIs: timer_list, hrtimer
- clocksource v� clockevent framework
- AM335x DMTIMER (Dual-Mode Timer)
- Vi?t driver s? d?ng kernel timer

---

## 2. AM335x DMTIMER

### 2.1 Timer modules

AM335x c� 8 DMTIMER (Timer0-7):

| Timer | Base Address | Clock Domain | Ngu?n: spruh73q.pdf |
|-------|-------------|--------------|---------------------|
| Timer0 | 0x44E05000 | WKUP | Chapter 20 |
| Timer1 | 0x44E31000 | WKUP (1ms) | Chapter 20 |
| Timer2 | 0x48040000 | PER | Chapter 20 |
| Timer3 | 0x48042000 | PER | Chapter 20 |
| Timer4 | 0x48044000 | PER | Chapter 20 |
| Timer5 | 0x48046000 | PER | Chapter 20 |
| Timer6 | 0x48048000 | PER | Chapter 20 |
| Timer7 | 0x4804A000 | PER | Chapter 20 |

### 2.2 DMTIMER Registers

| Register | Offset | M� t? |
|----------|--------|-------|
| TIDR | 0x00 | Timer ID |
| TIOCP_CFG | 0x10 | OCP Configuration |
| IRQSTATUS_RAW | 0x24 | Raw interrupt status |
| IRQSTATUS | 0x28 | Interrupt status (write 1 to clear) |
| IRQENABLE_SET | 0x2C | Interrupt enable |
| IRQENABLE_CLR | 0x30 | Interrupt disable |
| TCLR | 0x38 | Timer control (start/stop, mode) |
| TCRR | 0x3C | Timer counter register |
| TLDR | 0x40 | Timer load register (auto-reload value) |
| TTGR | 0x44 | Timer trigger register |

### 2.3 TCLR bits

| Bit | T�n | M� t? |
|-----|-----|-------|
| 0 | ST | Start/Stop timer (1=start) |
| 1 | AR | Auto-Reload (1=auto-reload from TLDR) |

### 2.4 IRQ bits

| Bit | T�n | M� t? |
|-----|-----|-------|
| 0 | MAT_IT | Match interrupt |
| 1 | OVF_IT | Overflow interrupt |
| 2 | TCAR_IT | Capture interrupt |

---

## 3. Kernel Timer APIs

### 3.1 Low-resolution timer (timer_list)

```c
#include <linux/timer.h>

struct timer_list my_timer;

/* Callback function */
void my_timer_callback(struct timer_list *t)
{
    /* Ch?y trong softirq context - KH�NG du?c sleep */
    pr_info("Timer fired!\n");

    /* L?p l?i sau 1 gi�y */
    mod_timer(&my_timer, jiffies + HZ);
}

/* Init */
timer_setup(&my_timer, my_timer_callback, 0);
mod_timer(&my_timer, jiffies + HZ);  /* Fire sau 1s */

/* H?y */
del_timer_sync(&my_timer);
```

### 3.2 High-resolution timer (hrtimer)

```c
#include <linux/hrtimer.h>
#include <linux/ktime.h>

struct hrtimer my_hrtimer;

enum hrtimer_restart my_hrtimer_callback(struct hrtimer *timer)
{
    pr_info("hrtimer fired!\n");

    /* L?p l?i sau 500ms */
    hrtimer_forward_now(timer, ms_to_ktime(500));
    return HRTIMER_RESTART;     /* Ho?c HRTIMER_NORESTART */
}

/* Init */
hrtimer_init(&my_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
my_hrtimer.function = my_hrtimer_callback;
hrtimer_start(&my_hrtimer, ms_to_ktime(500), HRTIMER_MODE_REL);

/* H?y */
hrtimer_cancel(&my_hrtimer);
```

### 3.3 So s�nh

| T�nh nang | timer_list | hrtimer |
|-----------|-----------|---------|
| �? ph�n gi?i | Jiffies (1-10ms) | Nanosecond |
| Context | Softirq | Softirq ho?c hardirq |
| D�ng cho | Timeout, periodic task | Precise timing |
| API | mod_timer() | hrtimer_start() |

---

## 4. DMTIMER Platform Driver

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/clk.h>

#define TIOCP_CFG    0x10
#define IRQSTATUS    0x28
#define IRQENABLE_SET 0x2C
#define TCLR         0x38
#define TCRR         0x3C
#define TLDR         0x40
#define TTGR         0x44

#define TCLR_ST      BIT(0)
#define TCLR_AR      BIT(1)
#define IRQ_OVF      BIT(1)

struct learn_timer {
	void __iomem *base;
	struct clk *fclk;
	int irq;
	unsigned long clk_rate;
	unsigned long overflow_count;
};

static irqreturn_t learn_timer_irq(int irq, void *dev_id)
{
	struct learn_timer *timer = dev_id;

	/* Clear overflow interrupt */
	writel(IRQ_OVF, timer->base + IRQSTATUS);

	timer->overflow_count++;

	if ((timer->overflow_count % 10) == 0)
		pr_info("Timer overflow #%lu\n", timer->overflow_count);

	return IRQ_HANDLED;
}

/* C?u h�nh timer v?i period_ms */
static void learn_timer_start(struct learn_timer *timer, unsigned int period_ms)
{
	u32 load_val;
	u32 ticks;

	/* Stop timer */
	writel(0, timer->base + TCLR);

	/* T�nh load value: overflow sau period_ms
	 * ticks = clk_rate * period_ms / 1000
	 * load_val = 0xFFFFFFFF - ticks + 1 */
	ticks = timer->clk_rate / 1000 * period_ms;
	load_val = 0xFFFFFFFF - ticks + 1;

	writel(load_val, timer->base + TLDR);  /* Auto-reload value */
	writel(load_val, timer->base + TCRR);  /* Current counter */
	writel(load_val, timer->base + TTGR);  /* Trigger reload */

	/* Enable overflow interrupt */
	writel(IRQ_OVF, timer->base + IRQENABLE_SET);

	/* Start timer: auto-reload + start */
	writel(TCLR_AR | TCLR_ST, timer->base + TCLR);

	pr_info("Timer started: period=%dms, load=0x%08x, clk=%lu\n",
	        period_ms, load_val, timer->clk_rate);
}

static int learn_timer_probe(struct platform_device *pdev)
{
	struct learn_timer *timer;
	int ret;

	timer = devm_kzalloc(&pdev->dev, sizeof(*timer), GFP_KERNEL);
	if (!timer)
		return -ENOMEM;

	timer->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(timer->base))
		return PTR_ERR(timer->base);

	timer->irq = platform_get_irq(pdev, 0);
	if (timer->irq < 0)
		return timer->irq;

	timer->fclk = devm_clk_get_enabled(&pdev->dev, "fck");
	if (IS_ERR(timer->fclk))
		return PTR_ERR(timer->fclk);

	timer->clk_rate = clk_get_rate(timer->fclk);

	/* Software reset */
	writel(BIT(0), timer->base + TIOCP_CFG);

	ret = devm_request_irq(&pdev->dev, timer->irq, learn_timer_irq,
	                        0, "learn-timer", timer);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, timer);

	/* B?t d?u timer 100ms */
	learn_timer_start(timer, 100);

	dev_info(&pdev->dev, "DMTIMER initialized, IRQ=%d\n", timer->irq);
	return 0;
}

static int learn_timer_remove(struct platform_device *pdev)
{
	struct learn_timer *timer = platform_get_drvdata(pdev);

	/* Stop timer */
	writel(0, timer->base + TCLR);

	dev_info(&pdev->dev, "Timer stopped, overflows=%lu\n",
	         timer->overflow_count);
	return 0;
}

static const struct of_device_id learn_timer_of_match[] = {
	{ .compatible = "learn,dmtimer" },
	{ },
};
MODULE_DEVICE_TABLE(of, learn_timer_of_match);

static struct platform_driver learn_timer_driver = {
	.probe = learn_timer_probe,
	.remove = learn_timer_remove,
	.driver = {
		.name = "learn-dmtimer",
		.of_match_table = learn_timer_of_match,
	},
};

module_platform_driver(learn_timer_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AM335x DMTIMER learning driver");
```

---

## 5. delayed_work (thay th? timer cho process context)

```c
#include <linux/workqueue.h>

struct delayed_work my_work;

void my_work_func(struct work_struct *work)
{
    /* Ch?y trong process context - C� TH? sleep */
    pr_info("Work executed\n");

    /* L?p l?i sau 1 gi�y */
    schedule_delayed_work(&my_work, HZ);
}

/* Init */
INIT_DELAYED_WORK(&my_work, my_work_func);
schedule_delayed_work(&my_work, HZ);

/* H?y */
cancel_delayed_work_sync(&my_work);
```

---

## 6. Ki?n th?c c?t l�i

1. **`timer_list`**: jiffies-based, softirq context, d�ng cho timeout
2. **`hrtimer`**: nanosecond precision, d�ng cho precise timing
3. **`delayed_work`**: process context timer, c� th? sleep
4. **DMTIMER**: 32-bit up counter, auto-reload t? TLDR, overflow interrupt
5. **Load value**: `0xFFFFFFFF - ticks + 1` d? timer overflow sau N ticks
6. **AM335x**: 8 DMTIMER, clock m?c d?nh 24MHz ho?c 32KHz

---

## 7. C�u h?i ki?m tra

1. S? kh�c nhau gi?a `timer_list` v� `hrtimer`?
2. `mod_timer(timer, jiffies + 2*HZ)` s? fire sau bao l�u?
3. T?i sao callback c?a `timer_list` kh�ng du?c g?i `msleep()`?
4. DMTIMER load value t�nh nhu th? n�o cho period 100ms?
5. `delayed_work` ch?y trong context g�? T?i sao d� l� l?i th??

---

## 8. Chu?n b? cho b�i sau

B�i ti?p theo: **B�i 27 - DMA Engine Framework**

Ta s? h?c DMA engine API, `dma_slave_config`, v� EDMA controller tr�n AM335x.
