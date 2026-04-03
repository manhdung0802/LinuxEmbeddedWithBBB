# Bài 9 - Platform Driver & Device Tree Binding

## 1. Mục tiêu bài học
- Hiểu platform bus model trong Linux kernel
- Viết platform_driver với probe/remove
- Kết nối driver với Device Tree qua `of_match_table`
- Hiểu flow: DT node → kernel match → probe() → driver hoạt động
- Dùng `platform_get_resource()` và `dev_*` API

---

## 2. Platform Bus Model

### 2.1 Vấn đề:

Peripheral trên SoC (GPIO, UART, I2C controller, ...) **không nằm trên bus discoverable** (không như USB hay PCI). CPU không thể "hỏi" bus để biết có device nào.

### 2.2 Giải pháp: Platform Bus

Linux tạo ra **platform bus** — một "bus ảo" để quản lý SoC peripheral:

```
┌─────────────────────────────────────────┐
│            Platform Bus                  │
│                                          │
│  Platform Device    Platform Device      │
│  (từ Device Tree)   (từ Device Tree)     │
│                                          │
│  ← match compatible →                   │
│                                          │
│  Platform Driver    Platform Driver      │
│  (trong kernel)     (module bạn viết)    │
└─────────────────────────────────────────┘
```

**Flow**:
1. Kernel parse Device Tree → tạo **platform_device** cho mỗi node
2. Driver đăng ký **platform_driver** với `compatible` string
3. Kernel **match** compatible → gọi **`probe()`**

---

## 3. Cấu trúc Platform Driver

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

/* Gọi khi DT node match driver */
static int my_probe(struct platform_device *pdev)
{
    dev_info(&pdev->dev, "Device probed!\n");
    /* Khởi tạo hardware, đăng ký chardev, etc. */
    return 0;
}

/* Gọi khi device bị remove */
static int my_remove(struct platform_device *pdev)
{
    dev_info(&pdev->dev, "Device removed!\n");
    /* Cleanup hardware, gỡ chardev, etc. */
    return 0;
}

/* Bảng compatible để match với DT */
static const struct of_device_id my_of_match[] = {
    { .compatible = "my,gpio-controller" },
    { },  /* Sentinel - kết thúc bảng */
};
MODULE_DEVICE_TABLE(of, my_of_match);

/* Đăng ký platform driver */
static struct platform_driver my_driver = {
    .probe  = my_probe,
    .remove = my_remove,
    .driver = {
        .name = "my-gpio-driver",
        .of_match_table = my_of_match,
    },
};

module_platform_driver(my_driver);
/* macro trên = module_init + module_exit tự động */

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("My GPIO Platform Driver");
```

### 3.1 `module_platform_driver()` macro:

```c
/* Tương đương với: */
static int __init my_init(void)
{
    return platform_driver_register(&my_driver);
}
static void __exit my_exit(void)
{
    platform_driver_unregister(&my_driver);
}
module_init(my_init);
module_exit(my_exit);
```

---

## 4. Device Tree Binding

### 4.1 DT node cho driver:

```dts
/* Trong board DTS */
/ {
    my_gpio: my-gpio@4804c000 {
        compatible = "my,gpio-controller";  /* Must match driver */
        reg = <0x4804c000 0x1000>;
        interrupts = <98>;
        clocks = <&gpio1_clk>;
        status = "okay";
    };
};
```

### 4.2 Quá trình matching:

```
Device Tree                           Driver Code
───────────                           ───────────
compatible = "my,gpio-controller"  ←→  .compatible = "my,gpio-controller"
          │                                      │
          └──── MATCH! ──── kernel gọi probe() ──┘
```

### 4.3 Compatible string convention:

```
"vendor,device-name"
```

Ví dụ:
- `"ti,omap4-gpio"` — Texas Instruments, OMAP4 GPIO
- `"ti,am3352-uart"` — TI, AM3352 UART
- `"my,led-driver"` — Custom driver

---

## 5. Probe Function - Chi tiết

`probe()` là nơi driver **khởi tạo mọi thứ**:

```c
static int my_probe(struct platform_device *pdev)
{
    struct resource *res;
    void __iomem *base;
    int irq, ret;
    struct my_device *mydev;

    /* 1. Allocate device struct */
    mydev = devm_kzalloc(&pdev->dev, sizeof(*mydev), GFP_KERNEL);
    if (!mydev)
        return -ENOMEM;

    /* 2. Lấy memory resource từ DT "reg" */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(base))
        return PTR_ERR(base);
    mydev->base = base;

    /* 3. Lấy IRQ từ DT "interrupts" */
    irq = platform_get_irq(pdev, 0);
    if (irq < 0)
        return irq;
    mydev->irq = irq;

    /* 4. Đọc property từ DT */
    of_property_read_u32(pdev->dev.of_node, "my-gpio-num",
                         &mydev->gpio_num);

    /* 5. Đăng ký chardev hoặc subsystem */
    ret = register_my_chardev(mydev);
    if (ret)
        return ret;

    /* 6. Lưu driver data */
    platform_set_drvdata(pdev, mydev);

    dev_info(&pdev->dev, "Probed successfully, base=%pR\n", res);
    return 0;
}
```

### 5.1 `platform_get_resource()`:

```c
/* Lấy resource thứ index, loại type từ DT */
struct resource *platform_get_resource(
    struct platform_device *pdev,
    unsigned int type,    /* IORESOURCE_MEM, IORESOURCE_IRQ */
    unsigned int index    /* 0 = first, 1 = second, ... */
);

/* res->start = 0x4804C000 (từ DT reg) */
/* res->end   = 0x4804CFFF */
/* resource_size(res) = 0x1000 */
```

### 5.2 `platform_get_irq()`:

```c
/* Lấy IRQ number từ DT "interrupts" */
int irq = platform_get_irq(pdev, 0);
/* irq = Linux IRQ number (đã qua IRQ domain mapping) */
```

### 5.3 Đọc DT property:

```c
#include <linux/of.h>

struct device_node *np = pdev->dev.of_node;

/* Đọc u32 */
u32 val;
of_property_read_u32(np, "clock-frequency", &val);

/* Đọc string */
const char *name;
of_property_read_string(np, "label", &name);

/* Kiểm tra boolean property */
bool active = of_property_read_bool(np, "active-low");

/* Đọc GPIO */
int gpio = of_get_named_gpio(np, "my-gpio", 0);
```

---

## 6. Remove Function

```c
static int my_remove(struct platform_device *pdev)
{
    struct my_device *mydev = platform_get_drvdata(pdev);

    /* Cleanup ngược thứ tự probe */
    unregister_my_chardev(mydev);

    /* Nếu dùng devm_*, nhiều thứ tự động cleanup */
    dev_info(&pdev->dev, "Removed\n");
    return 0;
}
```

---

## 7. `platform_set/get_drvdata`

```c
/* Lưu pointer vào platform_device (trong probe) */
platform_set_drvdata(pdev, mydev);

/* Lấy pointer ra (trong remove, hoặc các callback khác) */
struct my_device *mydev = platform_get_drvdata(pdev);
```

Bên dưới, nó lưu vào `pdev->dev.driver_data`.

---

## 8. Ví dụ hoàn chỉnh: GPIO Platform Driver

### 8.1 Device Tree:

```dts
/ {
    my_led_ctrl: my-led@4804c000 {
        compatible = "bbb,led-controller";
        reg = <0x4804c000 0x1000>;
        led-gpio-bit = <21>;
        status = "okay";
    };
};
```

### 8.2 Driver code:

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define GPIO_OE           0x134
#define GPIO_SETDATAOUT   0x194
#define GPIO_CLEARDATAOUT 0x190

struct led_device {
	void __iomem *base;
	u32 gpio_bit;
	int state;
	dev_t devno;
	struct cdev cdev;
	struct class *cls;
};

/* File operations */
static ssize_t led_write(struct file *f, const char __user *buf,
                          size_t count, loff_t *off)
{
	struct led_device *led = f->private_data;
	char val;

	if (copy_from_user(&val, buf, 1))
		return -EFAULT;

	if (val == '1') {
		writel(1 << led->gpio_bit, led->base + GPIO_SETDATAOUT);
		led->state = 1;
	} else {
		writel(1 << led->gpio_bit, led->base + GPIO_CLEARDATAOUT);
		led->state = 0;
	}

	return count;
}

static ssize_t led_read(struct file *f, char __user *buf,
                         size_t count, loff_t *off)
{
	struct led_device *led = f->private_data;
	char val;

	if (*off > 0)
		return 0;

	val = led->state ? '1' : '0';
	if (copy_to_user(buf, &val, 1))
		return -EFAULT;

	*off += 1;
	return 1;
}

static int led_open(struct inode *i, struct file *f)
{
	f->private_data = container_of(i->i_cdev, struct led_device, cdev);
	return 0;
}

static const struct file_operations led_fops = {
	.owner   = THIS_MODULE,
	.open    = led_open,
	.read    = led_read,
	.write   = led_write,
};

static int led_probe(struct platform_device *pdev)
{
	struct led_device *led;
	struct resource *res;
	u32 val;
	int ret;

	led = devm_kzalloc(&pdev->dev, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	led->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(led->base))
		return PTR_ERR(led->base);

	of_property_read_u32(pdev->dev.of_node, "led-gpio-bit", &led->gpio_bit);

	/* Cấu hình GPIO output */
	val = readl(led->base + GPIO_OE);
	val &= ~(1 << led->gpio_bit);
	writel(val, led->base + GPIO_OE);

	/* Đăng ký char device */
	ret = alloc_chrdev_region(&led->devno, 0, 1, "bbb_led");
	if (ret)
		return ret;

	cdev_init(&led->cdev, &led_fops);
	ret = cdev_add(&led->cdev, led->devno, 1);
	if (ret)
		goto err_chrdev;

	led->cls = class_create("bbb_led");
	if (IS_ERR(led->cls)) {
		ret = PTR_ERR(led->cls);
		goto err_cdev;
	}

	device_create(led->cls, &pdev->dev, led->devno, NULL, "bbb_led0");

	platform_set_drvdata(pdev, led);
	dev_info(&pdev->dev, "LED driver probed, GPIO bit %d\n", led->gpio_bit);
	return 0;

err_cdev:
	cdev_del(&led->cdev);
err_chrdev:
	unregister_chrdev_region(led->devno, 1);
	return ret;
}

static int led_remove(struct platform_device *pdev)
{
	struct led_device *led = platform_get_drvdata(pdev);

	device_destroy(led->cls, led->devno);
	class_destroy(led->cls);
	cdev_del(&led->cdev);
	unregister_chrdev_region(led->devno, 1);
	dev_info(&pdev->dev, "LED driver removed\n");
	return 0;
}

static const struct of_device_id led_of_match[] = {
	{ .compatible = "bbb,led-controller" },
	{ },
};
MODULE_DEVICE_TABLE(of, led_of_match);

static struct platform_driver led_platform_driver = {
	.probe  = led_probe,
	.remove = led_remove,
	.driver = {
		.name = "bbb-led",
		.of_match_table = led_of_match,
	},
};

module_platform_driver(led_platform_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BBB LED Platform Driver");
```

---

## 9. Kiến thức cốt lõi sau bài này

1. **Platform bus** = bus ảo cho SoC peripheral, không discoverable
2. **`compatible`** trong DT ↔ `of_match_table` trong driver → trigger `probe()`
3. **`probe()`** = nơi khởi tạo tất cả: ioremap, register chardev, setup hardware
4. **`remove()`** = cleanup ngược probe
5. **`platform_set/get_drvdata`** = lưu/lấy device state
6. **`module_platform_driver()`** = shortcut đăng ký platform driver

---

## 10. Câu hỏi kiểm tra

1. Tại sao SoC peripheral cần platform bus thay vì PCI/USB bus?
2. Khi nào kernel gọi `probe()`? Điều kiện gì cần thỏa mãn?
3. `devm_ioremap_resource()` khác gì `ioremap()`?
4. Viết DT node cho UART controller tại `0x44E09000`, size `0x2000`, IRQ `72`.
5. Nếu DT có `status = "disabled"`, probe() có được gọi không?

---

## 11. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 10 - Managed Resources (devm)**

Ta sẽ học `devm_*` API — tự động cleanup resource khi driver remove, giúp code gọn hơn và ít bug hơn.
