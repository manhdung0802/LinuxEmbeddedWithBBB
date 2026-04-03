# Bài 35 - Capstone Project

## 1. Mục tiêu bài học
- Tích hợp kiến thức từ 31 bài trước vào một project hoàn chỉnh
- Viết platform driver kết hợp: GPIO output (LED), GPIO input (button), I2C sensor, interrupt, sysfs, debugfs
- Hoàn thiện Device Tree overlay
- Mô hình driver chuẩn: probe/remove, PM, error handling

---

## 2. Mô tả Project

### 2.1 Smart Sensor Hub Driver

Driver điều khiển "sensor hub" trên BBB:

```
┌──────────────────────────────────────────────────────┐
│                  BBB (AM335x)                        │
│                                                      │
│  ┌─────────┐    ┌───────────┐    ┌──────────┐       │
│  │ Button  │───►│  Driver   │───►│ LED      │       │
│  │(GPIO in)│    │ (sensor-  │    │(GPIO out)│       │
│  │ + IRQ   │    │  hub)     │    │          │       │
│  └─────────┘    │           │    └──────────┘       │
│                 │  sysfs    │                        │
│  ┌─────────┐   │  debugfs  │    ┌──────────┐       │
│  │ TMP102  │──►│  PM       │───►│ Userspace│       │
│  │(I2C)    │   │           │    │          │       │
│  └─────────┘   └───────────┘    └──────────┘       │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### 2.2 Chức năng

1. **Button input**: GPIO interrupt, debounce, đếm số lần nhấn
2. **LED output**: GPIO output, toggle khi nhấn button
3. **Temperature sensor**: đọc TMP102 qua I2C
4. **Sysfs interface**: export temperature, button count, LED state
5. **Debugfs**: register dump, IRQ statistics
6. **Runtime PM**: tắt I2C khi idle

---

## 3. Device Tree Overlay

```dts
/dts-v1/;
/plugin/;

#include <dt-bindings/gpio/gpio.h>
#include <dt-bindings/pinctrl/am33xx.h>
#include <dt-bindings/interrupt-controller/irq.h>

&am33xx_pinmux {
    sensor_hub_pins: sensor_hub_pins {
        pinctrl-single,pins = <
            /* P9.12 = GPIO1_28 = LED output */
            AM33XX_PADCONF(AM335X_PIN_GPMC_BEN1, PIN_OUTPUT, MUX_MODE7)
            /* P9.23 = GPIO1_17 = Button input */
            AM33XX_PADCONF(AM335X_PIN_GPMC_A1, PIN_INPUT_PULLUP, MUX_MODE7)
        >;
    };

    i2c2_pins: i2c2_pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_UART1_CTSN, PIN_INPUT_PULLUP, MUX_MODE3)
            AM33XX_PADCONF(AM335X_PIN_UART1_RTSN, PIN_INPUT_PULLUP, MUX_MODE3)
        >;
    };
};

&i2c2 {
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&i2c2_pins>;
    clock-frequency = <400000>;

    tmp102@48 {
        compatible = "ti,tmp102";
        reg = <0x48>;
    };
};

&{/} {
    sensor_hub {
        compatible = "learn,sensor-hub";
        pinctrl-names = "default";
        pinctrl-0 = <&sensor_hub_pins>;

        led-gpios = <&gpio1 28 GPIO_ACTIVE_HIGH>;
        button-gpios = <&gpio1 17 GPIO_ACTIVE_LOW>;

        i2c-bus = <&i2c2>;
        temp-sensor-addr = /bits/ 16 <0x48>;
    };
};
```

---

## 4. Driver Code

```c
/*
 * sensor_hub.c - Capstone project: Smart Sensor Hub driver
 *
 * Tích hợp: GPIO (LED + button), I2C (TMP102), interrupt,
 *           sysfs, debugfs, Runtime PM
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

#define DRIVER_NAME    "sensor-hub"
#define TMP102_REG_TEMP  0x00

struct sensor_hub {
	struct device *dev;
	struct gpio_desc *led_gpio;
	struct gpio_desc *button_gpio;
	int button_irq;

	struct i2c_client *temp_client;
	struct mutex i2c_lock;

	/* State */
	bool led_state;
	unsigned long button_count;
	unsigned long irq_count;
	int last_temp;  /* millidegrees */

	/* Work */
	struct delayed_work temp_work;

	/* debugfs */
	struct dentry *debugfs_dir;
};

/* ========== I2C: Đọc temperature ========== */
static int sensor_hub_read_temp(struct sensor_hub *hub)
{
	int raw;

	mutex_lock(&hub->i2c_lock);

	pm_runtime_get_sync(hub->dev);

	raw = i2c_smbus_read_word_swapped(hub->temp_client, TMP102_REG_TEMP);

	pm_runtime_put_autosuspend(hub->dev);

	mutex_unlock(&hub->i2c_lock);

	if (raw < 0)
		return raw;

	raw >>= 4;
	if (raw & 0x800)
		raw |= 0xFFFFF000;

	return (raw * 625) / 10;
}

/* ========== Interrupt handler ========== */
static irqreturn_t sensor_hub_button_irq(int irq, void *data)
{
	struct sensor_hub *hub = data;

	hub->irq_count++;
	hub->button_count++;

	/* Toggle LED */
	hub->led_state = !hub->led_state;
	gpiod_set_value(hub->led_gpio, hub->led_state);

	dev_dbg(hub->dev, "Button press #%lu, LED=%d\n",
	        hub->button_count, hub->led_state);

	return IRQ_HANDLED;
}

/* ========== Periodic temperature work ========== */
static void sensor_hub_temp_work(struct work_struct *work)
{
	struct sensor_hub *hub = container_of(work, struct sensor_hub,
	                                       temp_work.work);
	hub->last_temp = sensor_hub_read_temp(hub);

	schedule_delayed_work(&hub->temp_work, 5 * HZ);
}

/* ========== Sysfs ========== */
static ssize_t temperature_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
	struct sensor_hub *hub = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", hub->last_temp);
}
static DEVICE_ATTR_RO(temperature);

static ssize_t button_count_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
	struct sensor_hub *hub = dev_get_drvdata(dev);
	return sprintf(buf, "%lu\n", hub->button_count);
}
static DEVICE_ATTR_RO(button_count);

static ssize_t led_state_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
	struct sensor_hub *hub = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", hub->led_state);
}

static ssize_t led_state_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
	struct sensor_hub *hub = dev_get_drvdata(dev);
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	hub->led_state = val;
	gpiod_set_value(hub->led_gpio, val);
	return count;
}
static DEVICE_ATTR_RW(led_state);

static struct attribute *sensor_hub_attrs[] = {
	&dev_attr_temperature.attr,
	&dev_attr_button_count.attr,
	&dev_attr_led_state.attr,
	NULL,
};

static const struct attribute_group sensor_hub_attr_group = {
	.attrs = sensor_hub_attrs,
};

/* ========== Debugfs ========== */
static int debug_stats_show(struct seq_file *s, void *data)
{
	struct sensor_hub *hub = s->private;

	seq_printf(s, "Button presses: %lu\n", hub->button_count);
	seq_printf(s, "IRQ count:      %lu\n", hub->irq_count);
	seq_printf(s, "LED state:      %s\n", hub->led_state ? "ON" : "OFF");
	seq_printf(s, "Temperature:    %d.%03d°C\n",
	           hub->last_temp / 1000, abs(hub->last_temp % 1000));

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(debug_stats);

static void sensor_hub_debugfs_init(struct sensor_hub *hub)
{
	hub->debugfs_dir = debugfs_create_dir(DRIVER_NAME, NULL);
	debugfs_create_file("stats", 0444, hub->debugfs_dir,
	                     hub, &debug_stats_fops);
	debugfs_create_bool("led_state", 0644, hub->debugfs_dir,
	                     &hub->led_state);
}

/* ========== Probe / Remove ========== */
static int sensor_hub_probe(struct platform_device *pdev)
{
	struct sensor_hub *hub;
	int ret;

	hub = devm_kzalloc(&pdev->dev, sizeof(*hub), GFP_KERNEL);
	if (!hub)
		return -ENOMEM;

	hub->dev = &pdev->dev;
	mutex_init(&hub->i2c_lock);
	platform_set_drvdata(pdev, hub);

	/* 1. GPIO LED */
	hub->led_gpio = devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_LOW);
	if (IS_ERR(hub->led_gpio)) {
		dev_err(&pdev->dev, "Failed to get LED GPIO\n");
		return PTR_ERR(hub->led_gpio);
	}

	/* 2. GPIO Button + IRQ */
	hub->button_gpio = devm_gpiod_get(&pdev->dev, "button", GPIOD_IN);
	if (IS_ERR(hub->button_gpio)) {
		dev_err(&pdev->dev, "Failed to get button GPIO\n");
		return PTR_ERR(hub->button_gpio);
	}

	hub->button_irq = gpiod_to_irq(hub->button_gpio);
	if (hub->button_irq < 0) {
		dev_err(&pdev->dev, "Failed to get IRQ for button\n");
		return hub->button_irq;
	}

	ret = devm_request_irq(&pdev->dev, hub->button_irq,
	                        sensor_hub_button_irq,
	                        IRQF_TRIGGER_FALLING,
	                        "sensor-hub-btn", hub);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ: %d\n", ret);
		return ret;
	}

	/* 3. I2C client cho TMP102 */
	/* Trong thực tế: dùng i2c_get_adapter() + i2c_new_client_device()
	   hoặc để DT binding tự match. Ở đây giả lập: */
	/* hub->temp_client = i2c_client_from_dt_or_manual(); */

	/* 4. Runtime PM */
	pm_runtime_enable(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, 5000);
	pm_runtime_use_autosuspend(&pdev->dev);

	/* 5. Sysfs */
	ret = devm_device_add_group(&pdev->dev, &sensor_hub_attr_group);
	if (ret)
		goto err_pm;

	/* 6. Debugfs */
	sensor_hub_debugfs_init(hub);

	/* 7. Periodic temperature reading */
	INIT_DELAYED_WORK(&hub->temp_work, sensor_hub_temp_work);
	schedule_delayed_work(&hub->temp_work, HZ);

	dev_info(&pdev->dev, "Sensor Hub initialized (IRQ=%d)\n",
	         hub->button_irq);
	return 0;

err_pm:
	pm_runtime_disable(&pdev->dev);
	return ret;
}

static int sensor_hub_remove(struct platform_device *pdev)
{
	struct sensor_hub *hub = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&hub->temp_work);
	debugfs_remove_recursive(hub->debugfs_dir);
	pm_runtime_dont_use_autosuspend(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	/* LED off */
	gpiod_set_value(hub->led_gpio, 0);

	dev_info(&pdev->dev, "Sensor Hub removed\n");
	return 0;
}

/* ========== PM ========== */
static int sensor_hub_suspend(struct device *dev)
{
	struct sensor_hub *hub = dev_get_drvdata(dev);

	cancel_delayed_work_sync(&hub->temp_work);
	gpiod_set_value(hub->led_gpio, 0);
	return 0;
}

static int sensor_hub_resume(struct device *dev)
{
	struct sensor_hub *hub = dev_get_drvdata(dev);

	schedule_delayed_work(&hub->temp_work, HZ);
	return 0;
}

static const struct dev_pm_ops sensor_hub_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sensor_hub_suspend, sensor_hub_resume)
};

/* ========== Match ========== */
static const struct of_device_id sensor_hub_of_match[] = {
	{ .compatible = "learn,sensor-hub" },
	{ },
};
MODULE_DEVICE_TABLE(of, sensor_hub_of_match);

static struct platform_driver sensor_hub_driver = {
	.probe = sensor_hub_probe,
	.remove = sensor_hub_remove,
	.driver = {
		.name = DRIVER_NAME,
		.pm = &sensor_hub_pm_ops,
		.of_match_table = sensor_hub_of_match,
	},
};

module_platform_driver(sensor_hub_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Capstone: Smart Sensor Hub driver");
MODULE_AUTHOR("Linux Driver Student");
```

---

## 5. Makefile

```makefile
obj-m += sensor_hub.o

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
BBB_IP ?= 192.168.7.2

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean

deploy:
	scp sensor_hub.ko debian@$(BBB_IP):~/
	scp sensor_hub.dtbo debian@$(BBB_IP):~/

# Cross compile
CROSS_COMPILE ?= arm-linux-gnueabihf-
ARCH ?= arm

cross:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) \
		-C $(KERNEL_DIR) M=$(PWD) modules
```

---

## 6. Test Plan

```bash
# 1. Deploy
scp sensor_hub.ko sensor_hub.dtbo debian@192.168.7.2:~/

# 2. Apply DT overlay
sudo cp sensor_hub.dtbo /lib/firmware/
sudo mkdir -p /sys/kernel/config/device-tree/overlays/sensor_hub
echo sensor_hub.dtbo > /sys/kernel/config/device-tree/overlays/sensor_hub/path

# 3. Load driver
sudo insmod sensor_hub.ko

# 4. Test sysfs
cat /sys/devices/platform/sensor_hub/temperature
cat /sys/devices/platform/sensor_hub/button_count
cat /sys/devices/platform/sensor_hub/led_state
echo 1 > /sys/devices/platform/sensor_hub/led_state

# 5. Test debugfs
cat /sys/kernel/debug/sensor-hub/stats

# 6. Test button
# Nhấn button → LED toggle, button_count tăng

# 7. Test PM
echo mem > /sys/power/state
# Wake up → check temperature work resumes

# 8. Unload
sudo rmmod sensor_hub
sudo rmdir /sys/kernel/config/device-tree/overlays/sensor_hub
```

---

## 7. Kiến thức tổng hợp sử dụng

| Bài | Kiến thức áp dụng |
|-----|-------------------|
| 5 | Module init/exit, module_platform_driver |
| 7-8 | Sysfs attributes (DEVICE_ATTR_RO/RW) |
| 9 | Platform driver, DT matching |
| 10 | devm_* managed resources |
| 11 | Mutex cho I2C access |
| 12 | GPIO interrupt, IRQF_TRIGGER_FALLING |
| 15 | gpiod API (devm_gpiod_get) |
| 19-20 | I2C client, i2c_smbus_read_word_swapped |
| 28 | Runtime PM, autosuspend |
| 30 | Device Tree overlay |
| 31 | debugfs, DEFINE_SHOW_ATTRIBUTE |

---

## 8. Câu hỏi đánh giá cuối khóa

1. Giải thích data flow khi userspace đọc `/sys/.../temperature`.
2. Tại sao cần `cancel_delayed_work_sync()` trong `remove()`?
3. Nếu TMP102 bị ngắt kết nối I2C, driver xử lý thế nào?
4. Thêm tính năng: alert khi temperature > threshold (dùng sysfs + uevent).
5. Refactor driver để hỗ trợ nhiều instance (multi-device).

---

## 9. Kết thúc khóa học

Chúc mừng bạn đã hoàn thành **32 bài học Linux Device Driver cho BeagleBone Black AM335x**!

### Các bước tiếp theo:
1. **Đọc kernel source**: `drivers/gpio/`, `drivers/i2c/`, `drivers/spi/`
2. **Contributing**: upstream kernel patches
3. **Advanced topics**: DRM/KMS, Network drivers, USB gadget, FPGA
4. **Real-world practice**: port driver cho sensor mới, viết custom cape driver

### Tài nguyên bổ sung:
- Linux Device Drivers, 3rd Edition (LDD3) - free online
- Linux Kernel Documentation: `Documentation/driver-api/`
- kernel.org mailing lists (LKML)
- AM335x TRM (spruh73q.pdf) - luôn là tài liệu tham khảo chính
