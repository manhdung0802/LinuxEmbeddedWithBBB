# B�i 29 - Clock & Power Management

> **Tham chi?u ph?n c?ng BBB:** Xem [clock.md](../mapping/clock.md)

## 1. M?c ti�u b�i h?c
- Hi?u Linux kernel Power Management framework
- Runtime PM: `pm_runtime_get/put`
- System suspend/resume: `dev_pm_ops`
- Clock gating tr�n AM335x
- Vi?t driver h? tr? PM d�ng c�ch

---

## 2. T?ng quan Power Management

### 2.1 C�c c?p PM trong Linux

```
+---------------------------------------------+
� System PM (Suspend/Hibernate)               �
�  /sys/power/state ? mem, freeze             �
�  pm_ops: suspend(), resume()                �
+---------------------------------------------�
� Runtime PM (Per-device PM)                  �
�  pm_runtime_get_sync() / pm_runtime_put()   �
�  T? d?ng b?t/t?t peripheral khi c?n        �
+---------------------------------------------�
� Clock Framework                             �
�  clk_enable() / clk_disable()              �
�  Gate clock cho peripheral kh�ng d�ng       �
+---------------------------------------------�
� Hardware (AM335x Power Domains)             �
�  PRCM: Power, Reset, Clock Manager         �
+---------------------------------------------+
```

### 2.2 AM335x Power Domains

| Power Domain | M� t? | Ngu?n: spruh73q.pdf |
|-------------|-------|---------------------|
| PER | GPIO, I2C, SPI, UART, Timer, EHRPWM | Chapter 8 |
| WKUP | UART0, Timer0/1, GPIO0, CM_WKUP | Chapter 8 |
| MPU | Cortex-A8, L1/L2 cache | Chapter 8 |
| GFX | SGX (3D graphics) | Chapter 8 |

---

## 3. Runtime PM

### 3.1 Kh�i ni?m

Runtime PM t? d?ng b?t/t?t device khi c�/kh�ng c� ai d�ng:

```
Driver m? device      Driver d�ng device
        �                      �
        ?                      ?
pm_runtime_get_sync()   pm_runtime_put()
        �                      �
        ?                      ?
  runtime_resume()      runtime_suspend()
  (b?t clock, power)    (t?t clock, power)
```

### 3.2 API

```c
#include <linux/pm_runtime.h>

/* Trong probe: enable runtime PM */
pm_runtime_enable(&pdev->dev);

/* Khi c?n d�ng device: b?t power + clock */
pm_runtime_get_sync(&pdev->dev);

/* Xong vi?c: cho ph�p t?t */
pm_runtime_put(&pdev->dev);          /* Non-blocking, autosuspend later */
pm_runtime_put_sync(&pdev->dev);     /* Blocking, suspend ngay */

/* Autosuspend: suspend sau N ms idle */
pm_runtime_set_autosuspend_delay(&pdev->dev, 2000);  /* 2 gi�y */
pm_runtime_use_autosuspend(&pdev->dev);
pm_runtime_put_autosuspend(&pdev->dev);

/* Trong remove: */
pm_runtime_disable(&pdev->dev);
```

### 3.3 Runtime PM callbacks

```c
static int my_runtime_suspend(struct device *dev)
{
    struct my_data *data = dev_get_drvdata(dev);

    /* T?t clock */
    clk_disable_unprepare(data->clk);

    dev_dbg(dev, "runtime suspended\n");
    return 0;
}

static int my_runtime_resume(struct device *dev)
{
    struct my_data *data = dev_get_drvdata(dev);
    int ret;

    /* B?t clock */
    ret = clk_prepare_enable(data->clk);
    if (ret)
        return ret;

    dev_dbg(dev, "runtime resumed\n");
    return 0;
}
```

---

## 4. System Suspend/Resume

### 4.1 dev_pm_ops

```c
static int my_suspend(struct device *dev)
{
    struct my_data *data = dev_get_drvdata(dev);

    /* Luu tr?ng th�i hardware */
    data->saved_reg = readl(data->base + MY_REG);

    /* Disable interrupt */
    disable_irq(data->irq);

    /* T?t clock (n?u chua runtime suspend) */
    if (!pm_runtime_suspended(dev))
        clk_disable_unprepare(data->clk);

    return 0;
}

static int my_resume(struct device *dev)
{
    struct my_data *data = dev_get_drvdata(dev);

    /* B?t clock */
    if (!pm_runtime_suspended(dev))
        clk_prepare_enable(data->clk);

    /* Kh�i ph?c tr?ng th�i */
    writel(data->saved_reg, data->base + MY_REG);

    enable_irq(data->irq);

    return 0;
}
```

### 4.2 SET_SYSTEM_SLEEP_PM_OPS

```c
static const struct dev_pm_ops my_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(my_suspend, my_resume)
    SET_RUNTIME_PM_OPS(my_runtime_suspend, my_runtime_resume, NULL)
};

static struct platform_driver my_driver = {
    .driver = {
        .name = "my-device",
        .pm = &my_pm_ops,
        .of_match_table = my_of_match,
    },
    .probe = my_probe,
    .remove = my_remove,
};
```

---

## 5. Driver ho�n ch?nh v?i PM

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

struct learn_pm_dev {
	void __iomem *base;
	struct clk *fclk;
	int irq;
	u32 saved_ctrl;
};

static int learn_pm_runtime_suspend(struct device *dev)
{
	struct learn_pm_dev *pdev = dev_get_drvdata(dev);

	clk_disable_unprepare(pdev->fclk);
	dev_dbg(dev, "runtime suspend\n");
	return 0;
}

static int learn_pm_runtime_resume(struct device *dev)
{
	struct learn_pm_dev *pdev = dev_get_drvdata(dev);

	return clk_prepare_enable(pdev->fclk);
}

static int learn_pm_suspend(struct device *dev)
{
	struct learn_pm_dev *pdev = dev_get_drvdata(dev);

	/* Luu register state */
	if (!pm_runtime_suspended(dev)) {
		pdev->saved_ctrl = readl(pdev->base + 0x10);
		clk_disable_unprepare(pdev->fclk);
	}

	return 0;
}

static int learn_pm_resume(struct device *dev)
{
	struct learn_pm_dev *pdev = dev_get_drvdata(dev);

	if (!pm_runtime_suspended(dev)) {
		clk_prepare_enable(pdev->fclk);
		writel(pdev->saved_ctrl, pdev->base + 0x10);
	}

	return 0;
}

static const struct dev_pm_ops learn_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(learn_pm_suspend, learn_pm_resume)
	SET_RUNTIME_PM_OPS(learn_pm_runtime_suspend,
	                    learn_pm_runtime_resume, NULL)
};

static int learn_pm_probe(struct platform_device *pdev)
{
	struct learn_pm_dev *dev;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dev->base))
		return PTR_ERR(dev->base);

	dev->fclk = devm_clk_get(&pdev->dev, "fck");
	if (IS_ERR(dev->fclk))
		return PTR_ERR(dev->fclk);

	platform_set_drvdata(pdev, dev);

	/* Enable Runtime PM */
	pm_runtime_enable(&pdev->dev);

	/* Autosuspend sau 2 gi�y idle */
	pm_runtime_set_autosuspend_delay(&pdev->dev, 2000);
	pm_runtime_use_autosuspend(&pdev->dev);

	/* B?t device ngay */
	pm_runtime_get_sync(&pdev->dev);

	dev_info(&pdev->dev, "Probe done, Runtime PM enabled\n");

	/* Cho ph�p auto-suspend */
	pm_runtime_put_autosuspend(&pdev->dev);

	return 0;
}

static int learn_pm_remove(struct platform_device *pdev)
{
	pm_runtime_dont_use_autosuspend(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	return 0;
}

static const struct of_device_id learn_pm_of_match[] = {
	{ .compatible = "learn,pm-device" },
	{ },
};
MODULE_DEVICE_TABLE(of, learn_pm_of_match);

static struct platform_driver learn_pm_driver = {
	.probe = learn_pm_probe,
	.remove = learn_pm_remove,
	.driver = {
		.name = "learn-pm",
		.pm = &learn_pm_ops,
		.of_match_table = learn_pm_of_match,
	},
};

module_platform_driver(learn_pm_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Power Management learning driver");
```

---

## 6. Debug PM

```bash
# Runtime PM status
cat /sys/devices/platform/my-dev/power/runtime_status
# active / suspended / unsupported

# Runtime PM usage count
cat /sys/devices/platform/my-dev/power/runtime_usage
# 0 = c� th? suspend, >0 = dang d�ng

# Autosuspend delay
cat /sys/devices/platform/my-dev/power/autosuspend_delay_ms
# 2000

# System suspend
echo mem > /sys/power/state

# PM debug
echo 1 > /sys/power/pm_debug_messages
dmesg | grep "PM:"
```

---

## 7. Ki?n th?c c?t l�i

1. **Runtime PM**: per-device, t? d?ng b?t/t?t ? ti?t ki?m di?n khi idle
2. **`pm_runtime_get_sync()`** b?t device, **`pm_runtime_put()`** cho ph�p t?t
3. **Autosuspend**: suspend sau N ms idle, tr�nh b?t/t?t li�n t?c
4. **System suspend**: luu register state ? t?t clock ? resume kh�i ph?c
5. **`pm_runtime_suspended()`**: ki?m tra tru?c khi access registers
6. **`dev_pm_ops`**: k?t h?p runtime PM v� system sleep callbacks
7. **AM335x PRCM**: qu?n l� clock gate cho t?ng module

---

## 8. C�u h?i ki?m tra

1. Ph�n bi?t Runtime PM v� System Suspend.
2. Khi n�o `runtime_suspend()` callback du?c g?i?
3. `pm_runtime_get_sync()` tr? v? g�? X? l� l?i th? n�o?
4. T?i sao c?n `pm_runtime_suspended()` check trong `suspend()` callback?
5. Autosuspend c� l?i g� so v?i put_sync() ngay l?p t?c?

---

## 9. Chu?n b? cho b�i sau

B�i ti?p theo: **B�i 29 - Watchdog Driver**

Ta s? h?c watchdog framework: `watchdog_device`, `watchdog_ops`, v� AM335x WDT.
