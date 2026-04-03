# Bài 14 - GPIO Platform Driver cho AM335x

## 1. Mục tiêu bài học
- Viết gpio_chip driver hoàn chỉnh cho AM335x GPIO module
- Implement direction_input, direction_output, get, set
- Dùng ioremap truy cập trực tiếp GPIO registers
- Đăng ký gpio_chip vào gpiolib
- Test bằng sysfs và libgpiod

---

## 2. AM335x GPIO Registers

GPIO module có các thanh ghi quan trọng (offset từ base):

| Offset | Thanh ghi | Mô tả |
|--------|-----------|-------|
| 0x000 | GPIO_REVISION | Module revision |
| 0x010 | GPIO_SYSCONFIG | System configuration |
| 0x020 | GPIO_EOI | End of Interrupt |
| 0x02C | GPIO_IRQSTATUS_0 | IRQ status line 0 |
| 0x034 | GPIO_IRQSTATUS_SET_0 | Enable IRQ |
| 0x03C | GPIO_IRQSTATUS_CLR_0 | Disable IRQ |
| 0x130 | GPIO_CTRL | Module control |
| 0x134 | GPIO_OE | Output Enable (0=output, 1=input) |
| 0x138 | GPIO_DATAIN | Data input |
| 0x13C | GPIO_DATAOUT | Data output |
| 0x190 | GPIO_CLEARDATAOUT | Clear bits |
| 0x194 | GPIO_SETDATAOUT | Set bits |

Nguồn: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 25 - GPIO Registers

---

## 3. Driver hoàn chỉnh

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/gpio/driver.h>
#include <linux/spinlock.h>

/* AM335x GPIO register offsets */
#define GPIO_OE             0x134
#define GPIO_DATAIN         0x138
#define GPIO_DATAOUT        0x13C
#define GPIO_CLEARDATAOUT   0x190
#define GPIO_SETDATAOUT     0x194

struct am335x_gpio {
	void __iomem *base;
	struct gpio_chip chip;
	spinlock_t lock;
};

/* Đọc giá trị pin */
static int am335x_gpio_get(struct gpio_chip *gc, unsigned int offset)
{
	struct am335x_gpio *ag = gpiochip_get_data(gc);
	return !!(readl(ag->base + GPIO_DATAIN) & BIT(offset));
}

/* Ghi giá trị pin */
static void am335x_gpio_set(struct gpio_chip *gc, unsigned int offset,
                              int value)
{
	struct am335x_gpio *ag = gpiochip_get_data(gc);

	if (value)
		writel(BIT(offset), ag->base + GPIO_SETDATAOUT);
	else
		writel(BIT(offset), ag->base + GPIO_CLEARDATAOUT);
}

/* Đặt pin thành input */
static int am335x_gpio_direction_input(struct gpio_chip *gc,
                                        unsigned int offset)
{
	struct am335x_gpio *ag = gpiochip_get_data(gc);
	unsigned long flags;
	u32 oe;

	spin_lock_irqsave(&ag->lock, flags);

	oe = readl(ag->base + GPIO_OE);
	oe |= BIT(offset);          /* 1 = input */
	writel(oe, ag->base + GPIO_OE);

	spin_unlock_irqrestore(&ag->lock, flags);
	return 0;
}

/* Đặt pin thành output + ghi giá trị */
static int am335x_gpio_direction_output(struct gpio_chip *gc,
                                         unsigned int offset, int value)
{
	struct am335x_gpio *ag = gpiochip_get_data(gc);
	unsigned long flags;
	u32 oe;

	/* Ghi giá trị trước khi chuyển direction */
	am335x_gpio_set(gc, offset, value);

	spin_lock_irqsave(&ag->lock, flags);

	oe = readl(ag->base + GPIO_OE);
	oe &= ~BIT(offset);         /* 0 = output */
	writel(oe, ag->base + GPIO_OE);

	spin_unlock_irqrestore(&ag->lock, flags);
	return 0;
}

/* Đọc direction hiện tại */
static int am335x_gpio_get_direction(struct gpio_chip *gc,
                                      unsigned int offset)
{
	struct am335x_gpio *ag = gpiochip_get_data(gc);
	u32 oe = readl(ag->base + GPIO_OE);

	/* GPIO_OE: 1=input, 0=output */
	/* gpio_chip convention: 1=input, 0=output */
	return !!(oe & BIT(offset));
}

static int am335x_gpio_probe(struct platform_device *pdev)
{
	struct am335x_gpio *ag;
	struct resource *res;

	ag = devm_kzalloc(&pdev->dev, sizeof(*ag), GFP_KERNEL);
	if (!ag)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ag->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ag->base))
		return PTR_ERR(ag->base);

	spin_lock_init(&ag->lock);

	/* Cấu hình gpio_chip */
	ag->chip.label = dev_name(&pdev->dev);
	ag->chip.parent = &pdev->dev;
	ag->chip.owner = THIS_MODULE;
	ag->chip.base = -1;         /* Dynamic numbering */
	ag->chip.ngpio = 32;        /* 32 pins per module */
	ag->chip.direction_input = am335x_gpio_direction_input;
	ag->chip.direction_output = am335x_gpio_direction_output;
	ag->chip.get = am335x_gpio_get;
	ag->chip.set = am335x_gpio_set;
	ag->chip.get_direction = am335x_gpio_get_direction;

	/* Đăng ký vào gpiolib */
	return devm_gpiochip_add_data(&pdev->dev, &ag->chip, ag);
}

static const struct of_device_id am335x_gpio_of_match[] = {
	{ .compatible = "learn,am335x-gpio" },
	{ },
};
MODULE_DEVICE_TABLE(of, am335x_gpio_of_match);

static struct platform_driver am335x_gpio_driver = {
	.probe = am335x_gpio_probe,
	.driver = {
		.name = "am335x-gpio-learn",
		.of_match_table = am335x_gpio_of_match,
	},
};

module_platform_driver(am335x_gpio_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AM335x GPIO driver for learning");
```

---

## 4. Device Tree

```dts
/* Dùng compatible riêng để không conflict với driver có sẵn */
my_gpio1: my-gpio@4804c000 {
    compatible = "learn,am335x-gpio";
    reg = <0x4804c000 0x1000>;
    gpio-controller;
    #gpio-cells = <2>;
    status = "okay";
};
```

**Lưu ý**: Trong thực tế, AM335x đã có driver GPIO sẵn (`ti,omap4-gpio`). Driver này chỉ để học.

---

## 5. Test

```bash
# Load module
sudo insmod am335x_gpio_learn.ko

# Xem gpio chip mới
cat /sys/kernel/debug/gpio

# Dùng sysfs
echo N > /sys/class/gpio/export       # N = base + offset
echo out > /sys/class/gpio/gpioN/direction
echo 1 > /sys/class/gpio/gpioN/value  # Bật LED

# Dùng libgpiod
gpiodetect
gpioinfo my-gpio@4804c000
gpioset my-gpio@4804c000 21=1
```

---

## 6. Giải thích chi tiết

### 6.1 `gpiochip_get_data()`:

```c
/* Lấy private data đã truyền khi gpiochip_add_data() */
struct am335x_gpio *ag = gpiochip_get_data(gc);
```

### 6.2 Tại sao cần spinlock cho GPIO_OE?

GPIO_OE là thanh ghi read-modify-write. Nếu 2 context cùng đọc-sửa-ghi:

```
Context A: oe = readl(GPIO_OE)     → 0xFFFFFFFF
Context B: oe = readl(GPIO_OE)     → 0xFFFFFFFF
Context A: oe &= ~BIT(5); writel   → 0xFFFFFFDF
Context B: oe &= ~BIT(10); writel  → 0xFFFFFBFF  ← MẤT thay đổi của A!
```

Spinlock đảm bảo atomic read-modify-write.

### 6.3 Tại sao set() không cần spinlock?

`GPIO_SETDATAOUT` và `GPIO_CLEARDATAOUT` là **write-1-to-set/clear** register:
- Ghi `BIT(5)` vào SETDATAOUT → chỉ set bit 5, không ảnh hưởng bit khác
- Không cần read-modify-write → không cần lock

---

## 7. Kiến thức cốt lõi sau bài này

1. **gpio_chip driver** implement: direction_input/output, get, set
2. **RMW (read-modify-write)** trên GPIO_OE cần spinlock
3. **SET/CLEAR registers** không cần lock (write-1-to-set)
4. **`devm_gpiochip_add_data()`** đăng ký controller vào kernel
5. **`gpiochip_get_data()`** lấy private data trong callbacks

---

## 8. Câu hỏi kiểm tra

1. Tại sao direction_input/output cần spinlock nhưng set() thì không?
2. `GPIO_OE` bit = 1 nghĩa là gì? Bit = 0 nghĩa là gì?
3. `gpiochip_get_data()` trả về gì?
4. Nếu `ngpio = 32` và `base = -1`, kernel sẽ gán GPIO number như thế nào?
5. Viết `to_irq()` callback cho gpio_chip.

---

## 9. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 15 - GPIO Consumer & gpiod API**

Ta sẽ học cách driver "consumer" dùng GPIO: `devm_gpiod_get()`, DT binding `*-gpios`, viết LED/button driver.
