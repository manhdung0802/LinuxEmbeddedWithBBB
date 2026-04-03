# Bài 29 - Watchdog Driver

## 1. Mục tiêu bài học
- Hiểu Watchdog framework trong Linux kernel
- `watchdog_device`, `watchdog_ops`, `watchdog_info`
- AM335x WDT (Watchdog Timer) registers
- Viết watchdog driver hoàn chỉnh

---

## 2. AM335x Watchdog Timer

### 2.1 Thông số

| Thông số | Giá trị | Nguồn: spruh73q.pdf |
|----------|---------|---------------------|
| Base Address | 0x44E35000 | Chapter 20 |
| Clock | 32KHz (32768 Hz) | Chapter 20 |
| Counter | 32-bit up counter | Chapter 20 |
| Prescaler | Có hỗ trợ | Chapter 20 |

### 2.2 Thanh ghi WDT

| Register | Offset | Mô tả |
|----------|--------|-------|
| WIDR | 0x00 | Watchdog ID |
| WDSC | 0x10 | System config |
| WDST | 0x14 | System status |
| WISR | 0x18 | Interrupt status |
| WIER | 0x1C | Interrupt enable |
| WCLR | 0x24 | Control register (prescaler) |
| WCRR | 0x28 | Counter register |
| WLDR | 0x2C | Load register |
| WTGR | 0x30 | Trigger register |
| WWPS | 0x34 | Write-pending status |
| WSPR | 0x48 | Start/Stop register |

### 2.3 Start/Stop sequence

AM335x WDT dùng cặp giá trị magic để start/stop:

```
Start WDT:
  1. Write 0xBBBB to WSPR
  2. Wait for WWPS[4]=0 (write pending clear)
  3. Write 0x4444 to WSPR
  4. Wait for WWPS[4]=0

Stop WDT:
  1. Write 0xAAAA to WSPR
  2. Wait for WWPS[4]=0
  3. Write 0x5555 to WSPR
  4. Wait for WWPS[4]=0
```

### 2.4 Reload (Kick/Pet):

```
Write bất kỳ giá trị khác current value vào WTGR
→ Counter reload từ WLDR
```

---

## 3. Linux Watchdog Framework

### 3.1 Kiến trúc

```
┌─────────────────────────────────────────┐
│ Userspace: /dev/watchdog                │
│  open() → start watchdog                │
│  write() → pet/kick watchdog            │
│  ioctl() → set timeout, get info        │
│  close() → stop (nếu NOWAYOUT=0)        │
├─────────────────────────────────────────┤
│ Watchdog Core (drivers/watchdog/watchdog_core.c) │
│  watchdog_register_device()             │
├─────────────────────────────────────────┤
│ Watchdog Driver (watchdog_ops)          │
│  .start(), .stop(), .ping(), .set_timeout() │
├─────────────────────────────────────────┤
│ Hardware (AM335x WDT)                   │
└─────────────────────────────────────────┘
```

### 3.2 Cấu trúc chính

```c
struct watchdog_info {
    __u32 options;              /* Capabilities */
    __u32 firmware_version;
    __u8  identity[32];         /* Tên watchdog */
};

struct watchdog_device {
    int id;
    struct device *parent;
    const struct watchdog_info *info;
    const struct watchdog_ops *ops;
    unsigned int timeout;       /* Current timeout (seconds) */
    unsigned int min_timeout;
    unsigned int max_timeout;
    unsigned int min_hw_heartbeat_ms;
    unsigned long status;       /* WDOG_ACTIVE, WDOG_DEV_OPEN, etc. */
};

struct watchdog_ops {
    int (*start)(struct watchdog_device *);
    int (*stop)(struct watchdog_device *);
    int (*ping)(struct watchdog_device *);
    int (*set_timeout)(struct watchdog_device *, unsigned int);
    unsigned int (*get_timeleft)(struct watchdog_device *);
};
```

---

## 4. AM335x Watchdog Driver

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/watchdog.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/clk.h>

#define WDT_WCLR   0x24
#define WDT_WCRR   0x28
#define WDT_WLDR   0x2C
#define WDT_WTGR   0x30
#define WDT_WWPS   0x34
#define WDT_WSPR   0x48

#define WDT_CLK_HZ  32768
#define WDT_DEFAULT_TIMEOUT  60  /* seconds */

struct learn_wdt {
	void __iomem *base;
	struct watchdog_device wdd;
	u32 trigger_counter;
};

static void learn_wdt_wait_write(struct learn_wdt *wdt, u32 bit)
{
	int timeout = 10000;
	while ((readl(wdt->base + WDT_WWPS) & bit) && --timeout)
		cpu_relax();
}

static int learn_wdt_start(struct watchdog_device *wdd)
{
	struct learn_wdt *wdt = watchdog_get_drvdata(wdd);

	/* Start sequence: 0xBBBB then 0x4444 */
	writel(0xBBBB, wdt->base + WDT_WSPR);
	learn_wdt_wait_write(wdt, BIT(4));
	writel(0x4444, wdt->base + WDT_WSPR);
	learn_wdt_wait_write(wdt, BIT(4));

	return 0;
}

static int learn_wdt_stop(struct watchdog_device *wdd)
{
	struct learn_wdt *wdt = watchdog_get_drvdata(wdd);

	/* Stop sequence: 0xAAAA then 0x5555 */
	writel(0xAAAA, wdt->base + WDT_WSPR);
	learn_wdt_wait_write(wdt, BIT(4));
	writel(0x5555, wdt->base + WDT_WSPR);
	learn_wdt_wait_write(wdt, BIT(4));

	return 0;
}

static int learn_wdt_ping(struct watchdog_device *wdd)
{
	struct learn_wdt *wdt = watchdog_get_drvdata(wdd);

	/* Kick: ghi WTGR với giá trị khác current */
	wdt->trigger_counter = ~wdt->trigger_counter;
	writel(wdt->trigger_counter, wdt->base + WDT_WTGR);
	learn_wdt_wait_write(wdt, BIT(3));

	return 0;
}

static int learn_wdt_set_timeout(struct watchdog_device *wdd,
                                  unsigned int timeout)
{
	struct learn_wdt *wdt = watchdog_get_drvdata(wdd);
	u32 load_val;

	/* Tính load value: counter overflow sau timeout giây
	 * ticks = 32768 * timeout
	 * load = 0xFFFFFFFF - ticks + 1 */
	load_val = 0xFFFFFFFF - (WDT_CLK_HZ * timeout) + 1;

	/* Stop → set load → start */
	learn_wdt_stop(wdd);

	writel(load_val, wdt->base + WDT_WLDR);
	learn_wdt_wait_write(wdt, BIT(2));

	writel(load_val, wdt->base + WDT_WCRR);
	learn_wdt_wait_write(wdt, BIT(1));

	wdd->timeout = timeout;

	learn_wdt_start(wdd);

	return 0;
}

static unsigned int learn_wdt_get_timeleft(struct watchdog_device *wdd)
{
	struct learn_wdt *wdt = watchdog_get_drvdata(wdd);
	u32 counter, load;

	counter = readl(wdt->base + WDT_WCRR);
	load = readl(wdt->base + WDT_WLDR);

	return (0xFFFFFFFF - counter) / WDT_CLK_HZ;
}

static const struct watchdog_info learn_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
	.identity = "AM335x Learning WDT",
};

static const struct watchdog_ops learn_wdt_ops = {
	.owner = THIS_MODULE,
	.start = learn_wdt_start,
	.stop = learn_wdt_stop,
	.ping = learn_wdt_ping,
	.set_timeout = learn_wdt_set_timeout,
	.get_timeleft = learn_wdt_get_timeleft,
};

static int learn_wdt_probe(struct platform_device *pdev)
{
	struct learn_wdt *wdt;
	int ret;

	wdt = devm_kzalloc(&pdev->dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	wdt->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(wdt->base))
		return PTR_ERR(wdt->base);

	wdt->wdd.info = &learn_wdt_info;
	wdt->wdd.ops = &learn_wdt_ops;
	wdt->wdd.min_timeout = 1;
	wdt->wdd.max_timeout = 131070;  /* 0xFFFFFFFF / 32768 */
	wdt->wdd.timeout = WDT_DEFAULT_TIMEOUT;
	wdt->wdd.parent = &pdev->dev;

	watchdog_set_drvdata(&wdt->wdd, wdt);
	watchdog_set_nowayout(&wdt->wdd, false);

	/* Đảm bảo WDT stopped khi probe */
	learn_wdt_stop(&wdt->wdd);

	ret = devm_watchdog_register_device(&pdev->dev, &wdt->wdd);
	if (ret)
		return ret;

	dev_info(&pdev->dev, "Watchdog registered, timeout=%ds\n",
	         wdt->wdd.timeout);
	return 0;
}

static const struct of_device_id learn_wdt_of_match[] = {
	{ .compatible = "learn,am335x-wdt" },
	{ },
};
MODULE_DEVICE_TABLE(of, learn_wdt_of_match);

static struct platform_driver learn_wdt_driver = {
	.probe = learn_wdt_probe,
	.driver = {
		.name = "learn-wdt",
		.of_match_table = learn_wdt_of_match,
	},
};

module_platform_driver(learn_wdt_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AM335x Watchdog learning driver");
```

---

## 5. Sử dụng từ Userspace

```bash
# Load driver
sudo insmod learn_wdt.ko

# Open watchdog → tự động start
exec 3>/dev/watchdog

# Ping (pet) watchdog
echo -n "V" >&3

# Hoặc dùng watchdog daemon
sudo apt install watchdog
# /etc/watchdog.conf: watchdog-device = /dev/watchdog

# Xem thông tin
cat /sys/class/watchdog/watchdog0/identity
# AM335x Learning WDT

cat /sys/class/watchdog/watchdog0/timeout
# 60

cat /sys/class/watchdog/watchdog0/timeleft
# 45

# Set timeout
wdctl /dev/watchdog
```

---

## 6. Magic Close

WDIOF_MAGICCLOSE: khi userspace ghi ký tự 'V' trước khi close → WDT dừng lại.
Nếu close mà không ghi 'V' → WDT tiếp tục chạy → system reset nếu không ping.

```bash
# Proper close (stop WDT)
echo -n "V" > /dev/watchdog

# Improper close (WDT keeps running → system reset!)
# Chỉ close file descriptor mà không ghi 'V'
```

---

## 7. Kiến thức cốt lõi

1. **Watchdog framework**: `watchdog_device` + `watchdog_ops`
2. **AM335x WDT**: magic sequence (0xBBBB/0x4444 start, 0xAAAA/0x5555 stop)
3. **Ping/Kick**: ghi WTGR reload counter từ WLDR
4. **Load value**: `0xFFFFFFFF - (32768 × timeout_sec) + 1`
5. **NOWAYOUT**: nếu set, WDT không thể stop từ userspace
6. **Magic close**: ghi 'V' trước close để gracefully stop WDT

---

## 8. Câu hỏi kiểm tra

1. AM335x WDT start/stop sequence là gì?
2. `watchdog_set_nowayout()` tác dụng gì?
3. Tại sao WDT ping bằng WTGR chứ không ghi trực tiếp WCRR?
4. Tính load value cho timeout 30 giây.
5. "Magic close" hoạt động thế nào?

---

## 9. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 30 - Device Tree Overlay**

Ta sẽ học cách thay đổi Device Tree tại runtime: DT overlay, configfs, cape support.
