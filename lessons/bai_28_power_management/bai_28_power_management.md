# Bài 28 - Power Management

## 1. Mục tiêu bài học
- Hiểu Linux kernel Power Management framework
- Runtime PM: `pm_runtime_get/put`
- System suspend/resume: `dev_pm_ops`
- Clock gating trên AM335x
- Viết driver hỗ trợ PM đúng cách

---

## 2. Tổng quan Power Management

### 2.1 Các cấp PM trong Linux

```
┌─────────────────────────────────────────────┐
│ System PM (Suspend/Hibernate)               │
│  /sys/power/state → mem, freeze             │
│  pm_ops: suspend(), resume()                │
├─────────────────────────────────────────────┤
│ Runtime PM (Per-device PM)                  │
│  pm_runtime_get_sync() / pm_runtime_put()   │
│  Tự động bật/tắt peripheral khi cần        │
├─────────────────────────────────────────────┤
│ Clock Framework                             │
│  clk_enable() / clk_disable()              │
│  Gate clock cho peripheral không dùng       │
├─────────────────────────────────────────────┤
│ Hardware (AM335x Power Domains)             │
│  PRCM: Power, Reset, Clock Manager         │
└─────────────────────────────────────────────┘
```

### 2.2 AM335x Power Domains

| Power Domain | Mô tả | Nguồn: spruh73q.pdf |
|-------------|-------|---------------------|
| PER | GPIO, I2C, SPI, UART, Timer, EHRPWM | Chapter 8 |
| WKUP | UART0, Timer0/1, GPIO0, CM_WKUP | Chapter 8 |
| MPU | Cortex-A8, L1/L2 cache | Chapter 8 |
| GFX | SGX (3D graphics) | Chapter 8 |

---

## 3. Runtime PM

### 3.1 Khái niệm

Runtime PM tự động bật/tắt device khi có/không có ai dùng:

```
Driver mở device      Driver đóng device
        │                      │
        ▼                      ▼
pm_runtime_get_sync()   pm_runtime_put()
        │                      │
        ▼                      ▼
  runtime_resume()      runtime_suspend()
  (bật clock, power)    (tắt clock, power)
```

### 3.2 API

```c
#include <linux/pm_runtime.h>

/* Trong probe: enable runtime PM */
pm_runtime_enable(&pdev->dev);

/* Khi cần dùng device: bật power + clock */
pm_runtime_get_sync(&pdev->dev);

/* Xong việc: cho phép tắt */
pm_runtime_put(&pdev->dev);          /* Non-blocking, autosuspend later */
pm_runtime_put_sync(&pdev->dev);     /* Blocking, suspend ngay */

/* Autosuspend: suspend sau N ms idle */
pm_runtime_set_autosuspend_delay(&pdev->dev, 2000);  /* 2 giây */
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

    /* Tắt clock */
    clk_disable_unprepare(data->clk);

    dev_dbg(dev, "runtime suspended\n");
    return 0;
}

static int my_runtime_resume(struct device *dev)
{
    struct my_data *data = dev_get_drvdata(dev);
    int ret;

    /* Bật clock */
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

    /* Lưu trạng thái hardware */
    data->saved_reg = readl(data->base + MY_REG);

    /* Disable interrupt */
    disable_irq(data->irq);

    /* Tắt clock (nếu chưa runtime suspend) */
    if (!pm_runtime_suspended(dev))
        clk_disable_unprepare(data->clk);

    return 0;
}

static int my_resume(struct device *dev)
{
    struct my_data *data = dev_get_drvdata(dev);

    /* Bật clock */
    if (!pm_runtime_suspended(dev))
        clk_prepare_enable(data->clk);

    /* Khôi phục trạng thái */
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

## 5. Driver hoàn chỉnh với PM

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

	/* Lưu register state */
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

	/* Autosuspend sau 2 giây idle */
	pm_runtime_set_autosuspend_delay(&pdev->dev, 2000);
	pm_runtime_use_autosuspend(&pdev->dev);

	/* Bật device ngay */
	pm_runtime_get_sync(&pdev->dev);

	dev_info(&pdev->dev, "Probe done, Runtime PM enabled\n");

	/* Cho phép auto-suspend */
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
# 0 = có thể suspend, >0 = đang dùng

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

## 7. Kiến thức cốt lõi

1. **Runtime PM**: per-device, tự động bật/tắt → tiết kiệm điện khi idle
2. **`pm_runtime_get_sync()`** bật device, **`pm_runtime_put()`** cho phép tắt
3. **Autosuspend**: suspend sau N ms idle, tránh bật/tắt liên tục
4. **System suspend**: lưu register state → tắt clock → resume khôi phục
5. **`pm_runtime_suspended()`**: kiểm tra trước khi access registers
6. **`dev_pm_ops`**: kết hợp runtime PM và system sleep callbacks
7. **AM335x PRCM**: quản lý clock gate cho từng module

---

## 8. Câu hỏi kiểm tra

1. Phân biệt Runtime PM và System Suspend.
2. Khi nào `runtime_suspend()` callback được gọi?
3. `pm_runtime_get_sync()` trả về gì? Xử lý lỗi thế nào?
4. Tại sao cần `pm_runtime_suspended()` check trong `suspend()` callback?
5. Autosuspend có lợi gì so với put_sync() ngay lập tức?

---

## 9. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 29 - Watchdog Driver**

Ta sẽ học watchdog framework: `watchdog_device`, `watchdog_ops`, và AM335x WDT.
