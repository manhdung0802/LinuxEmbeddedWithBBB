# Bài 18 - LED Subsystem

## 1. Mục tiêu bài học
- Hiểu LED class trong Linux kernel
- Đăng ký `led_classdev` và implement brightness_set callback
- Sử dụng LED triggers (heartbeat, timer, default-on)
- Viết driver cho onboard USR LEDs của BBB
- Dùng GPIO-based LED driver có sẵn trong kernel

---

## 2. LED Class Architecture

```
┌──────────────────────────────────────┐
│  Userspace                            │
│  /sys/class/leds/<name>/brightness    │
│  /sys/class/leds/<name>/trigger       │
└───────────────────┬──────────────────┘
                    │
┌───────────────────┴──────────────────┐
│  LED Core (drivers/leds/led-core.c)   │
│  - led_classdev management            │
│  - Trigger framework                  │
└───────────────────┬──────────────────┘
                    │
┌───────────────────┴──────────────────┐
│  LED Driver (your driver)             │
│  - led_classdev_register()            │
│  - brightness_set callback            │
└──────────────────────────────────────┘
```

---

## 3. `struct led_classdev`

```c
#include <linux/leds.h>

struct led_classdev {
    const char *name;                    /* Tên LED: "beaglebone:green:usr0" */
    unsigned int brightness;             /* Độ sáng hiện tại */
    unsigned int max_brightness;         /* Độ sáng tối đa */

    /* Callback bắt buộc */
    void (*brightness_set)(struct led_classdev *led_cdev,
                           enum led_brightness brightness);

    /* Hoặc version blocking (có thể sleep) */
    int (*brightness_set_blocking)(struct led_classdev *led_cdev,
                                   enum led_brightness brightness);

    const char *default_trigger;         /* Trigger mặc định */
};
```

---

## 4. Ví dụ: GPIO LED Driver

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>

struct my_led {
	struct led_classdev cdev;
	struct gpio_desc *gpio;
};

static void my_led_brightness_set(struct led_classdev *cdev,
                                   enum led_brightness brightness)
{
	struct my_led *led = container_of(cdev, struct my_led, cdev);
	gpiod_set_value(led->gpio, brightness > 0 ? 1 : 0);
}

static int my_led_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child;
	int ret;

	for_each_child_of_node(np, child) {
		struct my_led *led;
		const char *label;

		led = devm_kzalloc(&pdev->dev, sizeof(*led), GFP_KERNEL);
		if (!led)
			return -ENOMEM;

		led->gpio = devm_fwnode_gpiod_get(&pdev->dev,
		                                    of_fwnode_handle(child),
		                                    NULL, GPIOD_OUT_LOW, NULL);
		if (IS_ERR(led->gpio))
			return PTR_ERR(led->gpio);

		of_property_read_string(child, "label", &label);
		of_property_read_string(child, "linux,default-trigger",
		                        &led->cdev.default_trigger);

		led->cdev.name = label;
		led->cdev.brightness_set = my_led_brightness_set;
		led->cdev.max_brightness = 1;

		ret = devm_led_classdev_register(&pdev->dev, &led->cdev);
		if (ret) {
			dev_err(&pdev->dev, "Failed to register LED %s\n",
			        label);
			return ret;
		}
	}

	return 0;
}

static const struct of_device_id my_led_match[] = {
	{ .compatible = "my,gpio-leds" },
	{ },
};
MODULE_DEVICE_TABLE(of, my_led_match);

static struct platform_driver my_led_driver = {
	.probe = my_led_probe,
	.driver = {
		.name = "my-gpio-leds",
		.of_match_table = my_led_match,
	},
};

module_platform_driver(my_led_driver);
MODULE_LICENSE("GPL");
```

### Device Tree:

```dts
my_leds {
    compatible = "my,gpio-leds";

    led0 {
        gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;
        label = "bbb:green:usr0";
        linux,default-trigger = "heartbeat";
    };

    led1 {
        gpios = <&gpio1 22 GPIO_ACTIVE_HIGH>;
        label = "bbb:green:usr1";
        linux,default-trigger = "none";
    };
};
```

---

## 5. LED Triggers

Trigger = tự động điều khiển LED dựa trên sự kiện kernel.

### 5.1 Built-in triggers:

| Trigger | Mô tả |
|---------|-------|
| `none` | Không trigger, điều khiển thủ công |
| `default-on` | LED bật ngay khi probe |
| `heartbeat` | Nhấp nháy theo nhịp CPU |
| `timer` | Nhấp nháy theo delay_on/delay_off |
| `disk-activity` | Sáng khi có disk I/O |
| `cpu` | Sáng khi CPU active |
| `gpio` | Trigger bởi GPIO input |

### 5.2 Dùng trigger từ userspace:

```bash
# Xem triggers khả dụng
cat /sys/class/leds/bbb:green:usr0/trigger
# [none] rc-feedback nand-disk mmc0 mmc1 timer ...

# Đặt trigger
echo heartbeat > /sys/class/leds/bbb:green:usr0/trigger

# Dùng timer trigger
echo timer > /sys/class/leds/bbb:green:usr0/trigger
echo 100 > /sys/class/leds/bbb:green:usr0/delay_on    # ms
echo 900 > /sys/class/leds/bbb:green:usr0/delay_off   # ms

# Tắt trigger, điều khiển thủ công
echo none > /sys/class/leds/bbb:green:usr0/trigger
echo 1 > /sys/class/leds/bbb:green:usr0/brightness
echo 0 > /sys/class/leds/bbb:green:usr0/brightness
```

---

## 6. BBB Onboard LEDs

BeagleBone Black có 4 USR LEDs:

| LED | GPIO | Default Trigger |
|-----|------|----------------|
| USR0 | GPIO1_21 | heartbeat |
| USR1 | GPIO1_22 | mmc0 |
| USR2 | GPIO1_23 | cpu0 |
| USR3 | GPIO1_24 | mmc1 |

Trong default DTS:
```dts
leds {
    compatible = "gpio-leds";

    led2 {
        label = "beaglebone:green:usr0";
        gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;
        linux,default-trigger = "heartbeat";
        default-state = "off";
    };
    /* ... 3 LEDs nữa ... */
};
```

---

## 7. `gpio-leds` driver có sẵn

Kernel đã có driver `gpio-leds` (`drivers/leds/leds-gpio.c`). Chỉ cần DT đúng, không cần viết code:

```dts
leds {
    compatible = "gpio-leds";

    my_custom_led {
        gpios = <&gpio2 3 GPIO_ACTIVE_HIGH>;
        label = "my:led:custom";
        linux,default-trigger = "timer";
        default-state = "off";
    };
};
```

---

## 8. Kiến thức cốt lõi sau bài này

1. **led_classdev** = abstraction cho LED, tự tạo sysfs `/sys/class/leds/`
2. **brightness_set** callback = hàm bật/tắt LED thực sự
3. **LED trigger** = tự động blink dựa trên kernel events
4. **devm_led_classdev_register()** = đăng ký + devm cleanup
5. Kernel `gpio-leds` driver sẵn có, chỉ cần DT binding

---

## 9. Câu hỏi kiểm tra

1. `brightness_set` vs `brightness_set_blocking`: khi nào dùng cái nào?
2. LED trigger "heartbeat" hoạt động dựa trên gì?
3. Viết DT cho 2 LED: một heartbeat, một timer (200ms on, 800ms off).
4. Tại sao kernel cung cấp `gpio-leds` driver sẵn?
5. Làm sao tạo custom LED trigger?

---

## 10. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 19 - I2C Subsystem & Client Driver**

Ta sẽ học I2C framework: `i2c_driver`, `i2c_transfer`, Device Tree binding cho I2C device.
