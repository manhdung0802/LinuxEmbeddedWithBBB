# B�i 28 - AM335x Watchdog Driver

> **Tham chiếu phần cứng BBB:** Xem [watchdog.md](../mapping/watchdog.md)

## 0. BBB Connection � Watchdog tr�n BBB

```
AM335x WDT1 @ 0x44E35000, IRQ 91
----------------------------------
 Clock: CLK_32KHZ (32.768 kHz)
 Clock enable: CM_WKUP_WDT1_CLKCTRL @ 0x44E004D4

 H�nh vi: Khi counter overflow ? RESET to�n b? AM335x ? BBB reboot

 Write-protected sequence:
   Start: ghi 0xBBBB ? WSPR, ghi 0x4444 ? WSPR
   Stop:  ghi 0xAAAA ? WSPR, ghi 0x5555 ? WSPR

 BBB m?c d?nh: WDT1 KH�NG active (c?n driver enable)
 Test: insmod watchdog driver ? kh�ng ping ? BBB reboot sau timeout
```

> **C?nh b�o:** Watchdog s? **reset to�n b? BBB** n?u driver kh�ng ping k?p. Test c?n th?n.

---

## 1. M?c ti�u b�i h?c
- Hi?u Watchdog framework trong Linux kernel
- `watchdog_device`, `watchdog_ops`, `watchdog_info`
- AM335x WDT (Watchdog Timer) registers
- Vi?t watchdog driver ho�n ch?nh

---

## 2. AM335x Watchdog Timer

### 2.1 Th�ng s?

| Th�ng s? | Gi� tr? | Ngu?n: spruh73q.pdf |
|----------|---------|---------------------|
| Base Address | 0x44E35000 | Chapter 20 |
| Clock | 32KHz (32768 Hz) | Chapter 20 |
| Counter | 32-bit up counter | Chapter 20 |
| Prescaler | C� h? tr? | Chapter 20 |

### 2.2 Thanh ghi WDT

| Register | Offset | M� t? |
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

AM335x WDT d�ng c?p gi� tr? magic d? start/stop:

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
Write b?t k? gi� tr? kh�c current value v�o WTGR
? Counter reload t? WLDR
```

---

## 3. Linux Watchdog Framework

### 3.1 Ki?n tr�c

```
+-----------------------------------------+
� Userspace: /dev/watchdog                �
�  open() ? start watchdog                �
�  write() ? pet/kick watchdog            �
�  ioctl() ? set timeout, get info        �
�  close() ? stop (n?u NOWAYOUT=0)        �
+-----------------------------------------�
� Watchdog Core (drivers/watchdog/watchdog_core.c) �
�  watchdog_register_device()             �
+-----------------------------------------�
� Watchdog Driver (watchdog_ops)          �
�  .start(), .stop(), .ping(), .set_timeout() �
+-----------------------------------------�
� Hardware (AM335x WDT)                   �
+-----------------------------------------+
```

### 3.2 C?u tr�c ch�nh

```c
struct watchdog_info {
    __u32 options;              /* Capabilities */
    __u32 firmware_version;
    __u8  identity[32];         /* T�n watchdog */
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

	/* Kick: ghi WTGR v?i gi� tr? kh�c current */
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

	/* T�nh load value: counter overflow sau timeout gi�y
	 * ticks = 32768 * timeout
	 * load = 0xFFFFFFFF - ticks + 1 */
	load_val = 0xFFFFFFFF - (WDT_CLK_HZ * timeout) + 1;

	/* Stop ? set load ? start */
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

	/* �?m b?o WDT stopped khi probe */
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

## 5. S? d?ng t? Userspace

```bash
# Load driver
sudo insmod learn_wdt.ko

# Open watchdog ? t? d?ng start
exec 3>/dev/watchdog

# Ping (pet) watchdog
echo -n "V" >&3

# Ho?c d�ng watchdog daemon
sudo apt install watchdog
# /etc/watchdog.conf: watchdog-device = /dev/watchdog

# Xem th�ng tin
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

WDIOF_MAGICCLOSE: khi userspace ghi k� t? 'V' tru?c khi close ? WDT d?ng l?i.
N?u close m� kh�ng ghi 'V' ? WDT ti?p t?c ch?y ? system reset n?u kh�ng ping.

```bash
# Proper close (stop WDT)
echo -n "V" > /dev/watchdog

# Improper close (WDT keeps running ? system reset!)
# Ch? close file descriptor m� kh�ng ghi 'V'
```

---

## 7. Ki?n th?c c?t l�i

1. **Watchdog framework**: `watchdog_device` + `watchdog_ops`
2. **AM335x WDT**: magic sequence (0xBBBB/0x4444 start, 0xAAAA/0x5555 stop)
3. **Ping/Kick**: ghi WTGR reload counter t? WLDR
4. **Load value**: `0xFFFFFFFF - (32768 � timeout_sec) + 1`
5. **NOWAYOUT**: n?u set, WDT kh�ng th? stop t? userspace
6. **Magic close**: ghi 'V' tru?c close d? gracefully stop WDT

---

## 8. C�u h?i ki?m tra

1. AM335x WDT start/stop sequence l� g�?
2. `watchdog_set_nowayout()` t�c d?ng g�?
3. T?i sao WDT ping b?ng WTGR ch? kh�ng ghi tr?c ti?p WCRR?
4. T�nh load value cho timeout 30 gi�y.
5. "Magic close" ho?t d?ng th? n�o?

---

## 9. Chu?n b? cho b�i sau

B�i ti?p theo: **B�i 30 - Device Tree Overlay**

Ta s? h?c c�ch thay d?i Device Tree t?i runtime: DT overlay, configfs, cape support.
