# Bài 8 - Platform Driver & Device Tree Binding

## 1. M?c tiêu bài h?c
- Hi?u platform bus model trong Linux kernel
- Vi?t platform_driver v?i probe/remove
- K?t n?i driver v?i Device Tree qua `of_match_table`
- Hi?u flow: DT node ? kernel match ? probe() ? driver ho?t d?ng
- Dùng `platform_get_resource()` và `dev_*` API

---

## 2. Platform Bus Model

### 2.1 V?n d?:

Peripheral trên SoC (GPIO, UART, I2C controller, ...) **không n?m trên bus discoverable** (không nhu USB hay PCI). CPU không th? "h?i" bus d? bi?t có device nào.

### 2.2 Gi?i pháp: Platform Bus

Linux t?o ra **platform bus** — m?t "bus ?o" d? qu?n lý SoC peripheral:

```
+-----------------------------------------+
¦            Platform Bus                  ¦
¦                                          ¦
¦  Platform Device    Platform Device      ¦
¦  (t? Device Tree)   (t? Device Tree)     ¦
¦                                          ¦
¦  ? match compatible ?                   ¦
¦                                          ¦
¦  Platform Driver    Platform Driver      ¦
¦  (trong kernel)     (module b?n vi?t)    ¦
+-----------------------------------------+
```

**Flow**:
1. Kernel parse Device Tree ? t?o **platform_device** cho m?i node
2. Driver dang ký **platform_driver** v?i `compatible` string
3. Kernel **match** compatible ? g?i **`probe()`**

---

## 3. C?u trúc Platform Driver

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

/* G?i khi DT node match driver */
static int my_probe(struct platform_device *pdev)
{
    dev_info(&pdev->dev, "Device probed!\n");
    /* Kh?i t?o hardware, dang ký chardev, etc. */
    return 0;
}

/* G?i khi device b? remove */
static int my_remove(struct platform_device *pdev)
{
    dev_info(&pdev->dev, "Device removed!\n");
    /* Cleanup hardware, g? chardev, etc. */
    return 0;
}

/* B?ng compatible d? match v?i DT */
static const struct of_device_id my_of_match[] = {
    { .compatible = "my,gpio-controller" },
    { },  /* Sentinel - k?t thúc b?ng */
};
MODULE_DEVICE_TABLE(of, my_of_match);

/* Ðang ký platform driver */
static struct platform_driver my_driver = {
    .probe  = my_probe,
    .remove = my_remove,
    .driver = {
        .name = "my-gpio-driver",
        .of_match_table = my_of_match,
    },
};

module_platform_driver(my_driver);
/* macro trên = module_init + module_exit t? d?ng */

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("My GPIO Platform Driver");
```

### 3.1 `module_platform_driver()` macro:

```c
/* Tuong duong v?i: */
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
-----------                           -----------
compatible = "my,gpio-controller"  ??  .compatible = "my,gpio-controller"
          ¦                                      ¦
          +---- MATCH! ---- kernel g?i probe() --+
```

### 4.3 Compatible string convention:

```
"vendor,device-name"
```

Ví d?:
- `"ti,omap4-gpio"` — Texas Instruments, OMAP4 GPIO
- `"ti,am3352-uart"` — TI, AM3352 UART
- `"my,led-driver"` — Custom driver

---

## 5. Probe Function - Chi ti?t

`probe()` là noi driver **kh?i t?o m?i th?**:

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

    /* 2. L?y memory resource t? DT "reg" */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(base))
        return PTR_ERR(base);
    mydev->base = base;

    /* 3. L?y IRQ t? DT "interrupts" */
    irq = platform_get_irq(pdev, 0);
    if (irq < 0)
        return irq;
    mydev->irq = irq;

    /* 4. Ð?c property t? DT */
    of_property_read_u32(pdev->dev.of_node, "my-gpio-num",
                         &mydev->gpio_num);

    /* 5. Ðang ký chardev ho?c subsystem */
    ret = register_my_chardev(mydev);
    if (ret)
        return ret;

    /* 6. Luu driver data */
    platform_set_drvdata(pdev, mydev);

    dev_info(&pdev->dev, "Probed successfully, base=%pR\n", res);
    return 0;
}
```

### 5.1 `platform_get_resource()`:

```c
/* L?y resource th? index, lo?i type t? DT */
struct resource *platform_get_resource(
    struct platform_device *pdev,
    unsigned int type,    /* IORESOURCE_MEM, IORESOURCE_IRQ */
    unsigned int index    /* 0 = first, 1 = second, ... */
);

/* res->start = 0x4804C000 (t? DT reg) */
/* res->end   = 0x4804CFFF */
/* resource_size(res) = 0x1000 */
```

### 5.2 `platform_get_irq()`:

```c
/* L?y IRQ number t? DT "interrupts" */
int irq = platform_get_irq(pdev, 0);
/* irq = Linux IRQ number (dã qua IRQ domain mapping) */
```

### 5.3 Ð?c DT property:

```c
#include <linux/of.h>

struct device_node *np = pdev->dev.of_node;

/* Ð?c u32 */
u32 val;
of_property_read_u32(np, "clock-frequency", &val);

/* Ð?c string */
const char *name;
of_property_read_string(np, "label", &name);

/* Ki?m tra boolean property */
bool active = of_property_read_bool(np, "active-low");

/* Ð?c GPIO */
int gpio = of_get_named_gpio(np, "my-gpio", 0);
```

---

## 6. Remove Function

```c
static int my_remove(struct platform_device *pdev)
{
    struct my_device *mydev = platform_get_drvdata(pdev);

    /* Cleanup ngu?c th? t? probe */
    unregister_my_chardev(mydev);

    /* N?u dùng devm_*, nhi?u th? t? d?ng cleanup */
    dev_info(&pdev->dev, "Removed\n");
    return 0;
}
```

---

## 7. `platform_set/get_drvdata`

```c
/* Luu pointer vào platform_device (trong probe) */
platform_set_drvdata(pdev, mydev);

/* L?y pointer ra (trong remove, ho?c các callback khác) */
struct my_device *mydev = platform_get_drvdata(pdev);
```

Bên du?i, nó luu vào `pdev->dev.driver_data`.

---

## 8. Ví d? hoàn ch?nh: GPIO Platform Driver

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

	/* C?u hình GPIO output */
	val = readl(led->base + GPIO_OE);
	val &= ~(1 << led->gpio_bit);
	writel(val, led->base + GPIO_OE);

	/* Ðang ký char device */
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

## 9. Ki?n th?c c?t lõi sau bài này

1. **Platform bus** = bus ?o cho SoC peripheral, không discoverable
2. **`compatible`** trong DT ? `of_match_table` trong driver ? trigger `probe()`
3. **`probe()`** = noi kh?i t?o t?t c?: ioremap, register chardev, setup hardware
4. **`remove()`** = cleanup ngu?c probe
5. **`platform_set/get_drvdata`** = luu/l?y device state
6. **`module_platform_driver()`** = shortcut dang ký platform driver

---

## 10. Câu h?i ki?m tra

1. T?i sao SoC peripheral c?n platform bus thay vì PCI/USB bus?
2. Khi nào kernel g?i `probe()`? Ði?u ki?n gì c?n th?a mãn?
3. `devm_ioremap_resource()` khác gì `ioremap()`?
4. Vi?t DT node cho UART controller t?i `0x44E09000`, size `0x2000`, IRQ `72`.
5. N?u DT có `status = "disabled"`, probe() có du?c g?i không?

---

## 11. Chu?n b? cho bài sau

Bài ti?p theo: **Bài 10 - Managed Resources (devm)**

Ta s? h?c `devm_*` API — t? d?ng cleanup resource khi driver remove, giúp code g?n hon và ít bug hon.
