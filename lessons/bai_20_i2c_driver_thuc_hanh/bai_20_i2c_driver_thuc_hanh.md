# Bài 20 - I2C Driver thực hành

## 1. Mục tiêu bài học
- Viết driver hoàn chỉnh cho sensor I2C trên BBB
- Ví dụ chính: TMP102 temperature sensor (address 0x48)
- Tích hợp: I2C + sysfs + Device Tree
- Đọc datasheet sensor và map vào kernel API

---

## 2. TMP102 Temperature Sensor

### 2.1 Thông số:

- Chip: TI TMP102
- Interface: I2C, address 0x48 (mặc định)
- Resolution: 12-bit, 0.0625°C/LSB
- Range: -40°C tới +125°C
- Register map:

| Register | Address | Mô tả |
|----------|---------|-------|
| Temperature | 0x00 | 12-bit temperature (read-only) |
| Configuration | 0x01 | Config register |
| T_LOW | 0x02 | Low temperature threshold |
| T_HIGH | 0x03 | High temperature threshold |

### 2.2 Đọc temperature:

Temperature register (0x00) là 16-bit, MSB first:
```
Byte0: [D11 D10 D9 D8 D7 D6 D5 D4]
Byte1: [D3  D2  D1 D0  0  0  0  0]
```

Temperature = (Byte0 << 4 | Byte1 >> 4) × 0.0625°C

---

## 3. Driver hoàn chỉnh

```c
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>

#define TMP102_REG_TEMP    0x00
#define TMP102_REG_CONF    0x01

struct tmp102_data {
	struct i2c_client *client;
	struct mutex lock;
};

/* Đọc nhiệt độ (millidegree Celsius) */
static int tmp102_read_temp(struct tmp102_data *data)
{
	int raw;

	mutex_lock(&data->lock);
	raw = i2c_smbus_read_word_swapped(data->client, TMP102_REG_TEMP);
	mutex_unlock(&data->lock);

	if (raw < 0)
		return raw;

	/* 12-bit, shift right 4, multiply by 62.5 (= 625/10) */
	raw >>= 4;

	/* Xử lý số âm (12-bit signed) */
	if (raw & 0x800)
		raw |= 0xFFFFF000;

	/* Trả về millidegree: raw * 62.5 = raw * 625 / 10 */
	return (raw * 625) / 10;
}

/* Sysfs: đọc temperature */
static ssize_t temp_show(struct device *dev,
                          struct device_attribute *attr, char *buf)
{
	struct tmp102_data *data = dev_get_drvdata(dev);
	int temp = tmp102_read_temp(data);

	if (temp < 0)
		return temp;

	return sprintf(buf, "%d\n", temp);
}

static DEVICE_ATTR_RO(temp);

static struct attribute *tmp102_attrs[] = {
	&dev_attr_temp.attr,
	NULL,
};

static const struct attribute_group tmp102_attr_group = {
	.attrs = tmp102_attrs,
};

static int tmp102_probe(struct i2c_client *client)
{
	struct tmp102_data *data;
	int conf;

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;
	mutex_init(&data->lock);
	i2c_set_clientdata(client, data);

	/* Đọc config register để verify chip */
	conf = i2c_smbus_read_word_swapped(client, TMP102_REG_CONF);
	if (conf < 0) {
		dev_err(&client->dev, "Failed to read config: %d\n", conf);
		return conf;
	}
	dev_info(&client->dev, "TMP102 config: 0x%04x\n", conf);

	/* Đọc temperature thử */
	int temp = tmp102_read_temp(data);
	if (temp >= 0)
		dev_info(&client->dev, "Temperature: %d.%03d°C\n",
		         temp / 1000, abs(temp % 1000));

	/* Tạo sysfs attributes */
	return devm_device_add_group(&client->dev, &tmp102_attr_group);
}

static void tmp102_remove(struct i2c_client *client)
{
	dev_info(&client->dev, "TMP102 removed\n");
}

static const struct of_device_id tmp102_of_match[] = {
	{ .compatible = "learn,tmp102" },
	{ },
};
MODULE_DEVICE_TABLE(of, tmp102_of_match);

static const struct i2c_device_id tmp102_id[] = {
	{ "learn-tmp102", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, tmp102_id);

static struct i2c_driver tmp102_driver = {
	.driver = {
		.name = "learn-tmp102",
		.of_match_table = tmp102_of_match,
	},
	.probe = tmp102_probe,
	.remove = tmp102_remove,
	.id_table = tmp102_id,
};

module_i2c_driver(tmp102_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TMP102 I2C Temperature Sensor Driver (learning)");
```

---

## 4. Device Tree

```dts
&i2c2 {
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&i2c2_pins>;
    clock-frequency = <400000>;

    tmp102@48 {
        compatible = "learn,tmp102";
        reg = <0x48>;
    };
};

&am33xx_pinmux {
    i2c2_pins: i2c2_pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_UART1_CTSN, PIN_INPUT_PULLUP, MUX_MODE3)
            AM33XX_PADCONF(AM335X_PIN_UART1_RTSN, PIN_INPUT_PULLUP, MUX_MODE3)
        >;
    };
};
```

---

## 5. Test

```bash
# Load driver
sudo insmod learn_tmp102.ko

# Xem temperature (millidegree)
cat /sys/bus/i2c/devices/2-0048/temp
# 25625 (= 25.625°C)

# Debug I2C bus
i2cdetect -y 2
# Thấy 48 = TMP102 detected

# Đọc register trực tiếp
i2cget -y 2 0x48 0x00 w
```

---

## 6. Hwmon Integration (nâng cao)

Thay vì sysfs thủ công, dùng hwmon framework:

```c
static int tmp102_read(struct device *dev, enum hwmon_sensor_types type,
                        u32 attr, int channel, long *val)
{
    struct tmp102_data *data = dev_get_drvdata(dev);
    *val = tmp102_read_temp(data);
    return 0;
}

static const struct hwmon_channel_info *tmp102_info[] = {
    HWMON_CHANNEL_INFO(temp, HWMON_T_INPUT),
    NULL,
};

static const struct hwmon_ops tmp102_ops = {
    .read = tmp102_read,
    .is_visible = tmp102_is_visible,
};

static const struct hwmon_chip_info tmp102_chip_info = {
    .ops = &tmp102_ops,
    .info = tmp102_info,
};

/* Trong probe: */
hwmon = devm_hwmon_device_register_with_info(&client->dev,
    "tmp102", data, &tmp102_chip_info, NULL);
```

---

## 7. Kiến thức cốt lõi sau bài này

1. **Đọc datasheet sensor** → register map → I2C API
2. **`i2c_smbus_read_word_swapped()`** cho big-endian 16-bit register
3. **Sysfs attributes** để export data cho userspace
4. **Mutex** bảo vệ I2C transaction
5. **Hwmon** framework cho temperature/voltage/fan sensor

---

## 8. Câu hỏi kiểm tra

1. TMP102 temperature register 12-bit được decode như thế nào?
2. Tại sao dùng `i2c_smbus_read_word_swapped` thay vì `i2c_smbus_read_word_data`?
3. Tại sao cần mutex trong `tmp102_read_temp()`?
4. Viết hàm đọc 16-bit register bằng `i2c_transfer()` thay vì SMBus.
5. Hwmon framework có lợi gì so với sysfs thủ công?

---

## 9. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 21 - SPI Subsystem & Driver**

Ta sẽ học SPI framework: `spi_driver`, `spi_transfer`, `spi_message`, DT binding cho SPI device.
