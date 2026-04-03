# Bài 15 - GPIO Consumer & gpiod API

## 1. Mục tiêu bài học
- Sử dụng gpiod (GPIO descriptor) API trong driver consumer
- Viết Device Tree binding cho GPIO consumer (`*-gpios` property)
- Viết LED driver và button driver dùng gpiod
- Phân biệt gpiod_get vs devm_gpiod_get
- Hiểu GPIO active-low / active-high

---

## 2. gpiod API - Tổng quan

### 2.1 Tại sao dùng gpiod thay vì legacy GPIO API?

| Legacy (deprecated) | gpiod (khuyên dùng) |
|---------------------|---------------------|
| `gpio_request(53, "led")` | `devm_gpiod_get(dev, "led", ...)` |
| Global GPIO number | GPIO descriptor (opaque) |
| Không hiểu active-low | Xử lý active-low tự động |
| Manual free | devm tự free |

### 2.2 API chính:

```c
#include <linux/gpio/consumer.h>

/* Lấy GPIO từ DT (devm = tự free khi remove) */
struct gpio_desc *devm_gpiod_get(struct device *dev,
                                  const char *con_id,
                                  enum gpiod_flags flags);

/* Các flags */
GPIOD_IN              /* Input */
GPIOD_OUT_LOW         /* Output, init low */
GPIOD_OUT_HIGH        /* Output, init high */
GPIOD_OUT_LOW_OPEN_DRAIN   /* Open drain, low */

/* Đọc/ghi (tự xử lý active-low) */
int gpiod_get_value(const struct gpio_desc *desc);
void gpiod_set_value(struct gpio_desc *desc, int value);

/* Direction */
int gpiod_direction_input(struct gpio_desc *desc);
int gpiod_direction_output(struct gpio_desc *desc, int value);

/* Lấy IRQ từ GPIO */
int gpiod_to_irq(const struct gpio_desc *desc);
```

---

## 3. Device Tree Binding cho GPIO Consumer

### 3.1 Convention: `<function>-gpios`

```dts
my_device {
    compatible = "my,led-driver";

    /* Convention: <con_id>-gpios hoặc <con_id>-gpio */
    led-gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;
    /*           ↑       ↑  ↑
              phandle  offset  flags */

    reset-gpios = <&gpio0 15 GPIO_ACTIVE_LOW>;
    enable-gpios = <&gpio2 3 GPIO_ACTIVE_HIGH>;
};
```

### 3.2 Driver lấy GPIO:

```c
/* con_id "led" → DT tìm "led-gpios" property */
led_gpio = devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_LOW);

/* con_id "reset" → DT tìm "reset-gpios" */
reset_gpio = devm_gpiod_get(&pdev->dev, "reset", GPIOD_OUT_HIGH);
```

### 3.3 Nhiều GPIO cùng loại:

```dts
my_device {
    led-gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>,
                <&gpio1 22 GPIO_ACTIVE_HIGH>,
                <&gpio1 23 GPIO_ACTIVE_HIGH>;
};
```

```c
/* Lấy GPIO theo index */
led0 = devm_gpiod_get_index(&pdev->dev, "led", 0, GPIOD_OUT_LOW);
led1 = devm_gpiod_get_index(&pdev->dev, "led", 1, GPIOD_OUT_LOW);
led2 = devm_gpiod_get_index(&pdev->dev, "led", 2, GPIOD_OUT_LOW);
```

---

## 4. Active-Low vs Active-High

### 4.1 Concept:

```dts
/* Active HIGH: gpiod_set_value(1) → pin = HIGH → LED on */
led-gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;

/* Active LOW: gpiod_set_value(1) → pin = LOW → LED on */
led-gpios = <&gpio1 21 GPIO_ACTIVE_LOW>;
```

### 4.2 Trong driver:

```c
/* Driver chỉ cần quan tâm "logical value" */
gpiod_set_value(led_gpio, 1);  /* "Bật" LED */
gpiod_set_value(led_gpio, 0);  /* "Tắt" LED */

/* gpiolib tự đảo signal nếu GPIO_ACTIVE_LOW */
/* Driver không cần if/else cho polarity! */
```

---

## 5. Ví dụ: LED Driver dùng gpiod

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

	/* Lấy GPIO descriptors từ DT */
	for (i = 0; i < led->num_leds; i++) {
		led->gpios[i] = devm_gpiod_get_index(&pdev->dev,
		                                       "led", i,
		                                       GPIOD_OUT_LOW);
		if (IS_ERR(led->gpios[i])) {
			dev_err(&pdev->dev, "Failed to get LED GPIO %d\n", i);
			return PTR_ERR(led->gpios[i]);
		}
	}

	/* Đăng ký char device */
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

# Bật LED 0
echo "0 1" > /dev/simple_leds

# Tắt LED 0
echo "0 0" > /dev/simple_leds

# Bật LED 2
echo "2 1" > /dev/simple_leds
```

---

## 6. Optional GPIO

```c
/* Nếu GPIO không bắt buộc */
gpio = devm_gpiod_get_optional(&pdev->dev, "enable", GPIOD_OUT_HIGH);
if (IS_ERR(gpio))
    return PTR_ERR(gpio);  /* Real error */

if (!gpio)
    dev_info(dev, "No enable GPIO, continuing without\n");
else
    gpiod_set_value(gpio, 1);
```

---

## 7. GPIO → IRQ

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

## 8. Kiến thức cốt lõi sau bài này

1. **gpiod** API = descriptor-based, active-low aware, devm-compatible
2. **DT binding**: `<con_id>-gpios = <&chip offset flags>`
3. **`devm_gpiod_get()`** → con_id match DT property name (bỏ `-gpios`)
4. **Active-low** xử lý tự động: driver chỉ dùng logical value
5. **`gpiod_to_irq()`** chuyển GPIO → IRQ number

---

## 9. Câu hỏi kiểm tra

1. `devm_gpiod_get(dev, "led", GPIOD_OUT_LOW)` sẽ tìm property nào trong DT?
2. Sự khác nhau giữa `GPIO_ACTIVE_HIGH` và `GPIO_ACTIVE_LOW` ảnh hưởng driver thế nào?
3. Khi nào dùng `devm_gpiod_get_optional()`?
4. Viết DT node cho device có 2 LED GPIO và 1 button GPIO.
5. Làm sao lấy IRQ từ GPIO descriptor?

---

## 10. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 16 - Pin Control Subsystem**

Ta sẽ học pinctrl framework: cách kernel quản lý pinmux, pin configuration trên AM335x.
