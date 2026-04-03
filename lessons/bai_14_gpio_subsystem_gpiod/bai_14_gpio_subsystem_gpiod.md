# B�i 14 - GPIO Consumer & gpiod API

> **Tham chi?u ph?n c?ng BBB:** Xem [gpio.md](../mapping/gpio.md)

## 0. BBB Connection

> ⚠️ **AN TOÀN ĐIỆN:** AM335x GPIO là 3.3V logic, tối đa 6mA/pin. KHÔNG nối 5V. Dùng MOSFET cho tải lớn. � GPIO Consumer tr�n BBB

```
BeagleBone Black � GPIO Consumer targets
-----------------------------------------
 Onboard (kh�ng c?n n?i th�m):
   USR LED0 = GPIO1_21 (gpio53) ? Active HIGH
   USR LED1 = GPIO1_22 (gpio54) ? Active HIGH
   Button S2 = GPIO0_27 (gpio27) ? Active LOW (c� pull-up)

 Header P8/P9 (v� d? th�m):
   P9.12 = GPIO1_28 (gpio60) ? d�ng test output
   P9.15 = GPIO1_16 (gpio48) ? d�ng test input
   P8.07 = GPIO2_2  (gpio66) ? d�ng test interrupt

 DT binding v� d?:
   my-led-gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;
   my-btn-gpios = <&gpio0 27 GPIO_ACTIVE_LOW>;
```

| BBB Pin | AM335x Signal | GPIO# | Pad Offset | D�ng cho |
|---------|--------------|-------|-----------|----------|
| Onboard LED0 | GPIO1_21 | 53 | 0x854 | LED output |
| Onboard S2 | GPIO0_27 | 27 | 0x82C | Button input |
| P9.12 | GPIO1_28 | 60 | 0x878 | Test output |
| P8.07 | GPIO2_2 | 66 | 0x890 | Test IRQ |

---

## 1. M?c ti�u b�i h?c
- S? d?ng gpiod (GPIO descriptor) API trong driver consumer
- Vi?t Device Tree binding cho GPIO consumer (`*-gpios` property)
- Vi?t LED driver v� button driver d�ng gpiod
- Ph�n bi?t gpiod_get vs devm_gpiod_get
- Hi?u GPIO active-low / active-high

---

## 2. gpiod API - T?ng quan

### 2.1 T?i sao d�ng gpiod thay v� legacy GPIO API?

| Legacy (deprecated) | gpiod (khuy�n d�ng) |
|---------------------|---------------------|
| `gpio_request(53, "led")` | `devm_gpiod_get(dev, "led", ...)` |
| Global GPIO number | GPIO descriptor (opaque) |
| Kh�ng hi?u active-low | X? l� active-low t? d?ng |
| Manual free | devm t? free |

### 2.2 API ch�nh:

```c
#include <linux/gpio/consumer.h>

/* L?y GPIO t? DT (devm = t? free khi remove) */
struct gpio_desc *devm_gpiod_get(struct device *dev,
                                  const char *con_id,
                                  enum gpiod_flags flags);

/* C�c flags */
GPIOD_IN              /* Input */
GPIOD_OUT_LOW         /* Output, init low */
GPIOD_OUT_HIGH        /* Output, init high */
GPIOD_OUT_LOW_OPEN_DRAIN   /* Open drain, low */

/* �?c/ghi (t? x? l� active-low) */
int gpiod_get_value(const struct gpio_desc *desc);
void gpiod_set_value(struct gpio_desc *desc, int value);

/* Direction */
int gpiod_direction_input(struct gpio_desc *desc);
int gpiod_direction_output(struct gpio_desc *desc, int value);

/* L?y IRQ t? GPIO */
int gpiod_to_irq(const struct gpio_desc *desc);
```

---

## 3. Device Tree Binding cho GPIO Consumer

### 3.1 Convention: `<function>-gpios`

```dts
my_device {
    compatible = "my,led-driver";

    /* Convention: <con_id>-gpios ho?c <con_id>-gpio */
    led-gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;
    /*           ?       ?  ?
              phandle  offset  flags */

    reset-gpios = <&gpio0 15 GPIO_ACTIVE_LOW>;
    enable-gpios = <&gpio2 3 GPIO_ACTIVE_HIGH>;
};
```

### 3.2 Driver l?y GPIO:

```c
/* con_id "led" ? DT t�m "led-gpios" property */
led_gpio = devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_LOW);

/* con_id "reset" ? DT t�m "reset-gpios" */
reset_gpio = devm_gpiod_get(&pdev->dev, "reset", GPIOD_OUT_HIGH);
```

### 3.3 Nhi?u GPIO c�ng lo?i:

```dts
my_device {
    led-gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>,
                <&gpio1 22 GPIO_ACTIVE_HIGH>,
                <&gpio1 23 GPIO_ACTIVE_HIGH>;
};
```

```c
/* L?y GPIO theo index */
led0 = devm_gpiod_get_index(&pdev->dev, "led", 0, GPIOD_OUT_LOW);
led1 = devm_gpiod_get_index(&pdev->dev, "led", 1, GPIOD_OUT_LOW);
led2 = devm_gpiod_get_index(&pdev->dev, "led", 2, GPIOD_OUT_LOW);
```

---

## 4. Active-Low vs Active-High

### 4.1 Concept:

```dts
/* Active HIGH: gpiod_set_value(1) ? pin = HIGH ? LED on */
led-gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;

/* Active LOW: gpiod_set_value(1) ? pin = LOW ? LED on */
led-gpios = <&gpio1 21 GPIO_ACTIVE_LOW>;
```

### 4.2 Trong driver:

```c
/* Driver ch? c?n quan t�m "logical value" */
gpiod_set_value(led_gpio, 1);  /* "B?t" LED */
gpiod_set_value(led_gpio, 0);  /* "T?t" LED */

/* gpiolib t? d?o signal n?u GPIO_ACTIVE_LOW */
/* Driver kh�ng c?n if/else cho polarity! */
```

---

## 5. V� d?: LED Driver d�ng gpiod

### 5.1 Device Tree:

```dts
/ {
    my_leds {
        compatible = "my,simple-leds";
        pinctrl-names = "default";
        pinctrl-0 = <&my_led_pins>;

        led-gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>,
                    <&gpio1 22 GPIO_ACTIVE_HIGH>,
                    <&gpio1 23 GPIO_ACTIVE_HIGH>,
                    <&gpio1 24 GPIO_ACTIVE_HIGH>;
        num-leds = <4>;
    };
};

&am33xx_pinmux {
    my_led_pins: my_led_pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_GPMC_A5, PIN_OUTPUT, MUX_MODE7)
            AM33XX_PADCONF(AM335X_PIN_GPMC_A6, PIN_OUTPUT, MUX_MODE7)
            AM33XX_PADCONF(AM335X_PIN_GPMC_A7, PIN_OUTPUT, MUX_MODE7)
            AM33XX_PADCONF(AM335X_PIN_GPMC_A8, PIN_OUTPUT, MUX_MODE7)
        >;
    };
};
```

### 5.2 Driver:

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define MAX_LEDS 8

struct simple_led {
	struct gpio_desc *gpios[MAX_LEDS];
	int num_leds;
	dev_t devno;
	struct cdev cdev;
	struct class *cls;
};

static ssize_t led_write(struct file *f, const char __user *buf,
                          size_t count, loff_t *off)
{
	struct simple_led *led = f->private_data;
	char cmd[16];
	int idx, val;

	if (count > sizeof(cmd) - 1)
		return -EINVAL;

	if (copy_from_user(cmd, buf, count))
		return -EFAULT;
	cmd[count] = '\0';

	/* Format: "N V" (N=led index, V=0 or 1) */
	if (sscanf(cmd, "%d %d", &idx, &val) != 2)
		return -EINVAL;

	if (idx < 0 || idx >= led->num_leds)
		return -EINVAL;

	gpiod_set_value(led->gpios[idx], val);
	return count;
}

static int led_open(struct inode *i, struct file *f)
{
	f->private_data = container_of(i->i_cdev, struct simple_led, cdev);
	return 0;
}

static const struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.open = led_open,
	.write = led_write,
};

static int simple_led_probe(struct platform_device *pdev)
{
	struct simple_led *led;
	int i, ret;

	led = devm_kzalloc(&pdev->dev, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	of_property_read_u32(pdev->dev.of_node, "num-leds", &led->num_leds);
	if (led->num_leds > MAX_LEDS)
		led->num_leds = MAX_LEDS;

	/* L?y GPIO descriptors t? DT */
	for (i = 0; i < led->num_leds; i++) {
		led->gpios[i] = devm_gpiod_get_index(&pdev->dev,
		                                       "led", i,
		                                       GPIOD_OUT_LOW);
		if (IS_ERR(led->gpios[i])) {
			dev_err(&pdev->dev, "Failed to get LED GPIO %d\n", i);
			return PTR_ERR(led->gpios[i]);
		}
	}

	/* �ang k� char device */
	ret = alloc_chrdev_region(&led->devno, 0, 1, "simple_leds");
	if (ret)
		return ret;

	cdev_init(&led->cdev, &led_fops);
	ret = cdev_add(&led->cdev, led->devno, 1);
	if (ret)
		goto err_chrdev;

	led->cls = class_create("simple_leds");
	if (IS_ERR(led->cls)) {
		ret = PTR_ERR(led->cls);
		goto err_cdev;
	}
	device_create(led->cls, &pdev->dev, led->devno, NULL, "simple_leds");

	platform_set_drvdata(pdev, led);
	dev_info(&pdev->dev, "%d LEDs registered\n", led->num_leds);
	return 0;

err_cdev:
	cdev_del(&led->cdev);
err_chrdev:
	unregister_chrdev_region(led->devno, 1);
	return ret;
}

static int simple_led_remove(struct platform_device *pdev)
{
	struct simple_led *led = platform_get_drvdata(pdev);

	device_destroy(led->cls, led->devno);
	class_destroy(led->cls);
	cdev_del(&led->cdev);
	unregister_chrdev_region(led->devno, 1);
	return 0;
}

static const struct of_device_id simple_led_match[] = {
	{ .compatible = "my,simple-leds" },
	{ },
};
MODULE_DEVICE_TABLE(of, simple_led_match);

static struct platform_driver simple_led_driver = {
	.probe = simple_led_probe,
	.remove = simple_led_remove,
	.driver = {
		.name = "simple-leds",
		.of_match_table = simple_led_match,
	},
};

module_platform_driver(simple_led_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple GPIO LED driver using gpiod");
```

### 5.3 Test:

```bash
sudo insmod simple_leds.ko

# B?t LED 0
echo "0 1" > /dev/simple_leds

# T?t LED 0
echo "0 0" > /dev/simple_leds

# B?t LED 2
echo "2 1" > /dev/simple_leds
```

---

## 6. Optional GPIO

```c
/* N?u GPIO kh�ng b?t bu?c */
gpio = devm_gpiod_get_optional(&pdev->dev, "enable", GPIOD_OUT_HIGH);
if (IS_ERR(gpio))
    return PTR_ERR(gpio);  /* Real error */

if (!gpio)
    dev_info(dev, "No enable GPIO, continuing without\n");
else
    gpiod_set_value(gpio, 1);
```

---

## 7. GPIO ? IRQ

```c
struct gpio_desc *btn_gpio;
int irq;

btn_gpio = devm_gpiod_get(&pdev->dev, "button", GPIOD_IN);
irq = gpiod_to_irq(btn_gpio);

devm_request_threaded_irq(&pdev->dev, irq,
                           NULL, button_thread_handler,
                           IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                           "my-button", dev);
```

---

## 8. Ki?n th?c c?t l�i sau b�i n�y

1. **gpiod** API = descriptor-based, active-low aware, devm-compatible
2. **DT binding**: `<con_id>-gpios = <&chip offset flags>`
3. **`devm_gpiod_get()`** ? con_id match DT property name (b? `-gpios`)
4. **Active-low** x? l� t? d?ng: driver ch? d�ng logical value
5. **`gpiod_to_irq()`** chuy?n GPIO ? IRQ number

---

## 9. C�u h?i ki?m tra

1. `devm_gpiod_get(dev, "led", GPIOD_OUT_LOW)` s? t�m property n�o trong DT?
2. S? kh�c nhau gi?a `GPIO_ACTIVE_HIGH` v� `GPIO_ACTIVE_LOW` ?nh hu?ng driver th? n�o?
3. Khi n�o d�ng `devm_gpiod_get_optional()`?
4. Vi?t DT node cho device c� 2 LED GPIO v� 1 button GPIO.
5. L�m sao l?y IRQ t? GPIO descriptor?

---

## 10. Chu?n b? cho b�i sau

B�i ti?p theo: **B�i 16 - Pin Control Subsystem**

Ta s? h?c pinctrl framework: c�ch kernel qu?n l� pinmux, pin configuration tr�n AM335x.
