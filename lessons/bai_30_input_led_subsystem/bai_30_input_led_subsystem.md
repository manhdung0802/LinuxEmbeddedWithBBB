# B�i 30 - Input & LED Subsystem

## 1. M?c ti�u b�i h?c
- Hi?u input subsystem trong Linux kernel
- �ang k� input_dev cho button/keyboard/touchscreen
- D�ng `input_report_key()`, `input_report_abs()`, `input_sync()`
- Vi?t button driver v?i GPIO interrupt
- Implement debounce b?ng kernel timer

---

## 2. Input Subsystem Architecture

```
+------------------------------------------+
�  Userspace                                �
�  /dev/input/event0   evtest   libevdev    �
+------------------------------------------+
                    �
+------------------------------------------+
�  Input Core (drivers/input/input.c)       �
�  - Dispatch events                        �
�  - /dev/input/eventN management           �
+------------------------------------------+
                    �
+------------------------------------------+
�  Input Driver (your driver)               �
�  - input_allocate_device()                �
�  - input_register_device()                �
�  - input_report_key()                     �
+------------------------------------------+
```

Input subsystem cung c?p:
- Giao di?n th?ng nh?t cho t?t c? input device
- Event-based: `EV_KEY`, `EV_ABS`, `EV_REL`, ...
- T? t?o `/dev/input/eventN`

---

## 3. �ang k� Input Device

```c
#include <linux/input.h>

struct input_dev *input;

/* Allocate */
input = devm_input_allocate_device(&pdev->dev);
if (!input)
    return -ENOMEM;

/* C?u h�nh */
input->name = "BBB Button";
input->phys = "bbb-button/input0";
input->id.bustype = BUS_HOST;

/* Khai b�o lo?i event */
set_bit(EV_KEY, input->evbit);        /* Key events */
set_bit(KEY_ENTER, input->keybit);    /* Ph�m Enter */

/* �ang k� */
ret = input_register_device(input);
if (ret)
    return ret;
```

---

## 4. Report Events

```c
/* Button pressed */
input_report_key(input, KEY_ENTER, 1);  /* 1 = pressed */
input_sync(input);                       /* K?t th�c 1 event */

/* Button released */
input_report_key(input, KEY_ENTER, 0);  /* 0 = released */
input_sync(input);

/* QUAN TR?NG: input_sync() ph?i g?i sau m?i nh�m event */
```

### 4.1 Event types:

| Type | Macro | V� d? |
|------|-------|-------|
| Key/button | `EV_KEY` | KEY_ENTER, BTN_LEFT |
| Relative | `EV_REL` | REL_X, REL_Y (mouse) |
| Absolute | `EV_ABS` | ABS_X, ABS_Y (touchscreen) |
| Switch | `EV_SW` | SW_LID |

---

## 5. Button Driver ho�n ch?nh

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define DEBOUNCE_MS 50

struct bbb_button {
	struct input_dev *input;
	struct gpio_desc *gpio;
	int irq;
	struct timer_list debounce_timer;
	int keycode;
};

/* Timer callback cho debounce */
static void debounce_timer_fn(struct timer_list *t)
{
	struct bbb_button *btn = from_timer(btn, t, debounce_timer);
	int val;

	val = gpiod_get_value(btn->gpio);
	input_report_key(btn->input, btn->keycode, val);
	input_sync(btn->input);
}

/* IRQ handler � ch? restart timer */
static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
	struct bbb_button *btn = dev_id;

	mod_timer(&btn->debounce_timer,
	          jiffies + msecs_to_jiffies(DEBOUNCE_MS));

	return IRQ_HANDLED;
}

static int bbb_button_probe(struct platform_device *pdev)
{
	struct bbb_button *btn;
	int ret;

	btn = devm_kzalloc(&pdev->dev, sizeof(*btn), GFP_KERNEL);
	if (!btn)
		return -ENOMEM;

	/* GPIO */
	btn->gpio = devm_gpiod_get(&pdev->dev, "button", GPIOD_IN);
	if (IS_ERR(btn->gpio))
		return PTR_ERR(btn->gpio);

	/* IRQ t? GPIO */
	btn->irq = gpiod_to_irq(btn->gpio);
	if (btn->irq < 0)
		return btn->irq;

	/* Keycode t? DT */
	of_property_read_u32(pdev->dev.of_node, "linux,code", &btn->keycode);
	if (!btn->keycode)
		btn->keycode = KEY_ENTER;

	/* Input device */
	btn->input = devm_input_allocate_device(&pdev->dev);
	if (!btn->input)
		return -ENOMEM;

	btn->input->name = "BBB GPIO Button";
	btn->input->phys = "bbb-button/input0";
	btn->input->id.bustype = BUS_HOST;
	btn->input->dev.parent = &pdev->dev;

	set_bit(EV_KEY, btn->input->evbit);
	set_bit(btn->keycode, btn->input->keybit);

	/* Debounce timer */
	timer_setup(&btn->debounce_timer, debounce_timer_fn, 0);

	/* IRQ */
	ret = devm_request_irq(&pdev->dev, btn->irq,
	                        button_irq_handler,
	                        IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
	                        "bbb-button", btn);
	if (ret)
		return ret;

	/* Register input */
	ret = input_register_device(btn->input);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, btn);
	dev_info(&pdev->dev, "Button registered, keycode=%d\n", btn->keycode);
	return 0;
}

static int bbb_button_remove(struct platform_device *pdev)
{
	struct bbb_button *btn = platform_get_drvdata(pdev);
	del_timer_sync(&btn->debounce_timer);
	return 0;
}

static const struct of_device_id bbb_button_match[] = {
	{ .compatible = "bbb,gpio-button" },
	{ },
};
MODULE_DEVICE_TABLE(of, bbb_button_match);

static struct platform_driver bbb_button_driver = {
	.probe = bbb_button_probe,
	.remove = bbb_button_remove,
	.driver = {
		.name = "bbb-button",
		.of_match_table = bbb_button_match,
	},
};

module_platform_driver(bbb_button_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BBB GPIO Button with debounce (Input subsystem)");
```

### Device Tree:

```dts
bbb_button {
    compatible = "bbb,gpio-button";
    button-gpios = <&gpio2 8 GPIO_ACTIVE_LOW>;
    linux,code = <28>;  /* KEY_ENTER */
    pinctrl-names = "default";
    pinctrl-0 = <&button_pin>;
};
```

---

## 6. Debounce

### 6.1 V?n d?:

Button co h?c t?o "bounce" � nhi?u signal thay d?i trong v�i ms:

```
Th?c t? khi nh?n n�t:
    -----+   ++ ++ +------
         +---++-++-+
         ?bounce?
         (~1-10ms)
```

### 6.2 Gi?i ph�p: Timer debounce

```
IRQ ? restart timer (50ms)
      ?
Timer expires ? d?c GPIO ? report event
```

N?u bounce g�y nhi?u IRQ li�n ti?p, timer ch? expire **m?t l?n** sau khi signal ?n d?nh.

---

## 7. Test input device

```bash
# C�i evtest
apt install evtest

# Li?t k� input devices
cat /proc/bus/input/devices

# Test event
evtest /dev/input/event0
# Nh?n n�t ? th?y EV_KEY KEY_ENTER 1/0
```

---

## 8. Ki?n th?c c?t l�i sau b�i n�y

1. **Input subsystem**: `input_allocate_device` ? `set_bit` ? `input_register_device`
2. **Report**: `input_report_key(input, code, value)` + `input_sync(input)`
3. **Debounce**: D�ng kernel timer, IRQ ch? restart timer
4. **DT binding**: `linux,code` cho keycode, `button-gpios` cho GPIO
5. Userspace d?c event qua `/dev/input/eventN`

---

## 9. C�u h?i ki?m tra

1. `input_sync()` d�ng d? l�m g�? Khi n�o g?i?
2. Debounce b?ng timer ho?t d?ng ra sao?
3. S? kh�c nhau gi?a `EV_KEY` v� `EV_ABS`?
4. Vi?t DT node cho 3 button, m?i button keycode kh�c nhau.
5. L�m sao bi?t input device d� du?c dang k� th�nh c�ng?

---

---

# PH?N 2 � LED Subsystem

---

## 10. BBB Connection � USR LEDs

```
BeagleBone Black � Onboard USR LEDs
------------------------------------
                    +-----------------+
 USR LED0 (green) --� GPIO1_21 (gpio53)�  /sys/class/leds/beaglebone:green:usr0
 USR LED1 (green) --� GPIO1_22 (gpio54)�  /sys/class/leds/beaglebone:green:usr1
 USR LED2 (green) --� GPIO1_23 (gpio55)�  /sys/class/leds/beaglebone:green:usr2
 USR LED3 (green) --� GPIO1_24 (gpio56)�  /sys/class/leds/beaglebone:green:usr3
                    +-----------------+
         �                                      �
    Active HIGH -- resistor+LED -- GND (tr�n board)

 Default triggers (BBB stock DT):
   USR0 = heartbeat    USR1 = mmc0
   USR2 = cpu0         USR3 = mmc1
```

> **Tham chiếu phần cứng BBB:** Xem [gpio.md](../mapping/gpio.md)

---

## 11. LED Subsystem Architecture

```
+------------------------------------------+
�  Userspace                                �
�  /sys/class/leds/my-led/                  �
�    brightness   trigger   max_brightness  �
+------------------------------------------+
                    �
+------------------------------------------+
�  LED Core (drivers/leds/led-core.c)       �
�  - Manages led_classdev                   �
�  - Trigger dispatching                    �
�  - Brightness clamping                    �
+------------------------------------------+
                    �
+------------------------------------------+
�  LED Class Driver (your driver)           �
�  - led_classdev_register()                �
�  - brightness_set / brightness_set_blocking�
�  - Optional: blink_set                    �
+------------------------------------------+
                    �
              Hardware (GPIO, PWM, I2C LED controller)
```

---

## 12. `led_classdev` � LED Class Driver

### 12.1 C?u tr�c co b?n

```c
#include <linux/leds.h>

struct my_led {
    struct led_classdev cdev;
    void __iomem *gpio_base;
    u32 bit;
};
```

### 12.2 Implement brightness_set

```c
/* G?i trong atomic context (timer, workqueue) */
static void my_led_brightness_set(struct led_classdev *cdev,
                                  enum led_brightness brightness)
{
    struct my_led *led = container_of(cdev, struct my_led, cdev);

    if (brightness)
        writel(BIT(led->bit), led->gpio_base + 0x194);  /* GPIO_SETDATAOUT */
    else
        writel(BIT(led->bit), led->gpio_base + 0x190);  /* GPIO_CLEARDATAOUT */
}

/* Ho?c d�ng blocking version (c� th? sleep, v� d? I2C LED) */
static int my_led_brightness_set_blocking(struct led_classdev *cdev,
                                          enum led_brightness brightness)
{
    struct my_led *led = container_of(cdev, struct my_led, cdev);

    /* V� d?: ghi qua I2C (c?n sleep) */
    /* i2c_smbus_write_byte_data(client, REG_LED, brightness); */

    return 0;
}
```

### 12.3 �ang k� LED

```c
static int my_led_probe(struct platform_device *pdev)
{
    struct my_led *led;

    led = devm_kzalloc(&pdev->dev, sizeof(*led), GFP_KERNEL);
    if (!led)
        return -ENOMEM;

    led->cdev.name             = "bbb:green:custom";
    led->cdev.brightness       = LED_OFF;
    led->cdev.max_brightness   = 1;       /* on/off */
    led->cdev.brightness_set   = my_led_brightness_set;
    led->cdev.default_trigger  = "none";  /* ho?c "heartbeat", "timer" */

    /* Map GPIO1 */
    led->gpio_base = devm_ioremap(&pdev->dev, 0x4804C000, 0x1000);
    if (!led->gpio_base)
        return -ENOMEM;
    led->bit = 21;  /* USR LED0 = GPIO1_21 */

    return devm_led_classdev_register(&pdev->dev, &led->cdev);
}
```

---

## 13. LED Triggers

Trigger l� callback t? d?ng di?u khi?n LED (heartbeat, timer, disk, network, etc.).

### 13.1 S? d?ng built-in triggers

```bash
# Tr�n BBB:
echo heartbeat > /sys/class/leds/beaglebone:green:usr0/trigger
echo timer     > /sys/class/leds/beaglebone:green:usr3/trigger

# Timer trigger � c�i d?t on/off period (ms)
echo 100 > /sys/class/leds/beaglebone:green:usr3/delay_on
echo 900 > /sys/class/leds/beaglebone:green:usr3/delay_off

# Danh s�ch triggers available
cat /sys/class/leds/beaglebone:green:usr0/trigger
# [none] rc-feedback kbd-scrolllock ... heartbeat timer ...
```

### 13.2 Vi?t custom trigger

```c
#include <linux/leds.h>

static int my_trigger_activate(struct led_classdev *cdev)
{
    /* Kh?i t?o khi trigger du?c g�n cho LED */
    led_set_brightness(cdev, LED_FULL);
    return 0;
}

static void my_trigger_deactivate(struct led_classdev *cdev)
{
    led_set_brightness(cdev, LED_OFF);
}

static struct led_trigger my_trigger = {
    .name       = "my-custom",
    .activate   = my_trigger_activate,
    .deactivate = my_trigger_deactivate,
};

/* Trong module_init */
led_trigger_register(&my_trigger);

/* K�ch ho?t t? driver kh�c */
led_trigger_event(&my_trigger, LED_FULL);  /* b?t */
led_trigger_event(&my_trigger, LED_OFF);   /* t?t */

/* Trong module_exit */
led_trigger_unregister(&my_trigger);
```

---

## 14. V� d? ho�n ch?nh � BBB USR LED Driver

```c
/* bbb_led_driver.c � LED class driver cho 4 USR LEDs tr�n BBB */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/leds.h>

#define GPIO1_BASE  0x4804C000
#define GPIO1_SIZE  0x1000

struct bbb_led {
    struct led_classdev cdev;
    void __iomem *base;
    u32 bit;
};

struct bbb_leds {
    void __iomem *base;
    struct bbb_led leds[4];
};

static void bbb_led_set(struct led_classdev *cdev, enum led_brightness value)
{
    struct bbb_led *led = container_of(cdev, struct bbb_led, cdev);

    if (value)
        writel(BIT(led->bit), led->base + 0x194);
    else
        writel(BIT(led->bit), led->base + 0x190);
}

static const char * const led_names[] = {
    "bbb:green:usr0",
    "bbb:green:usr1",
    "bbb:green:usr2",
    "bbb:green:usr3",
};

static const char * const led_triggers[] = {
    "heartbeat",
    "none",
    "none",
    "timer",
};

static int bbb_leds_probe(struct platform_device *pdev)
{
    struct bbb_leds *priv;
    int i, ret;

    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->base = devm_ioremap(&pdev->dev, GPIO1_BASE, GPIO1_SIZE);
    if (!priv->base)
        return -ENOMEM;

    /* Set GPIO1_21-24 as output */
    u32 oe = readl(priv->base + 0x134);
    oe &= ~(0xF << 21);
    writel(oe, priv->base + 0x134);

    for (i = 0; i < 4; i++) {
        priv->leds[i].base = priv->base;
        priv->leds[i].bit = 21 + i;
        priv->leds[i].cdev.name = led_names[i];
        priv->leds[i].cdev.brightness = LED_OFF;
        priv->leds[i].cdev.max_brightness = 1;
        priv->leds[i].cdev.brightness_set = bbb_led_set;
        priv->leds[i].cdev.default_trigger = led_triggers[i];

        ret = devm_led_classdev_register(&pdev->dev, &priv->leds[i].cdev);
        if (ret) {
            dev_err(&pdev->dev, "Failed to register LED %d\n", i);
            return ret;
        }
    }

    dev_info(&pdev->dev, "4 USR LEDs registered\n");
    return 0;
}

static const struct of_device_id bbb_leds_of_match[] = {
    { .compatible = "ti,bbb-usr-leds" },
    { },
};
MODULE_DEVICE_TABLE(of, bbb_leds_of_match);

static struct platform_driver bbb_leds_driver = {
    .driver = {
        .name = "bbb-usr-leds",
        .of_match_table = bbb_leds_of_match,
    },
    .probe = bbb_leds_probe,
};
module_platform_driver(bbb_leds_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BBB USR LED class driver");
```

### 14.1 Device Tree

```dts
bbb_leds: bbb-usr-leds {
    compatible = "ti,bbb-usr-leds";
    status = "okay";
};
```

### 14.2 Test

```bash
# Load
insmod bbb_led_driver.ko

# Control individual LEDs
echo 1 > /sys/class/leds/bbb:green:usr0/brightness      # ON
echo 0 > /sys/class/leds/bbb:green:usr0/brightness      # OFF

# Set trigger
echo timer > /sys/class/leds/bbb:green:usr1/trigger
echo 200 > /sys/class/leds/bbb:green:usr1/delay_on
echo 800 > /sys/class/leds/bbb:green:usr1/delay_off

# List all registered LEDs
ls /sys/class/leds/
# bbb:green:usr0  bbb:green:usr1  bbb:green:usr2  bbb:green:usr3
```

---

## 15. Kernel DT `gpio-leds` driver (tham kh?o)

BBB stock DT d�ng `gpio-leds` driver c� s?n trong kernel � kh�ng c?n vi?t driver:

```dts
/* �� c� trong am335x-boneblack.dts */
leds {
    pinctrl-names = "default";
    pinctrl-0 = <&user_leds_default>;
    compatible = "gpio-leds";

    led2 {
        label = "beaglebone:green:usr0";
        gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;
        linux,default-trigger = "heartbeat";
        default-state = "off";
    };
    led3 {
        label = "beaglebone:green:usr1";
        gpios = <&gpio1 22 GPIO_ACTIVE_HIGH>;
        linux,default-trigger = "mmc0";
        default-state = "off";
    };
    /* ... usr2, usr3 ... */
};
```

> **Luu �:** �? d�ng custom LED driver, b?n c?n disable `gpio-leds` node trong DT overlay tru?c.

---

## 16. C�u h?i t?ng h?p (Input + LED)

1. So s�nh `input_report_key()` v?i `led_set_brightness()` � callback direction?
2. Vi?t driver k?t h?p: button press (input) ? toggle LED (led_classdev).
3. T?i sao `brightness_set` c� th? ch?y trong atomic context nhung `brightness_set_blocking` th� kh�ng?
4. `gpio-leds` driver d�ng `gpiod_set_value()` � so s�nh v?i c�ch b?n vi?t `writel()` tr?c ti?p.
5. Vi?t custom trigger "button-indicator" � LED s�ng 200ms m?i khi button pressed.

---

B�i ti?p theo: **B�i 28 - Sysfs, Debugfs & Procfs Interface**
