# B�i 22 - I2C Client Driver th?c h�nh

> **Tham chi?u ph?n c?ng BBB:** Xem [i2c.md](../mapping/i2c.md)

## 0. BBB Connection

> ⚠️ **AN TOÀN ĐIỆN:** I2C2 BBB = 3.3V logic + onboard pull-up. KHÔNG nối sensor 5V trực tiếp. � I2C Client wiring

```
BeagleBone Black � I2C2 + TMP102 sensor
---------------------------------------
  +---------+         +------------+
  �   BBB   �         �  TMP102   �
  �  P9.19  +-- SCL --� SCL       �  addr=0x48
  �  P9.20  +-- SDA --� SDA       �  (ADD0=GND)
  �  3.3V   +---------� V+        �
  �  GND    +---------� GND       �
  +---------+         +------------+
  I2C2 bus: /dev/i2c-2
  Pull-up: 4.7kO l�n 3.3V (c� s?n tr�n BBB cho I2C2)
```

### Ki?m tra nhanh
```bash
i2cdetect -y -r 2
# Expected: address 0x48 hi?n "48"
```

---

## 1. M?c ti�u b�i h?c
- Vi?t driver ho�n ch?nh cho sensor I2C tr�n BBB
- V� d? ch�nh: TMP102 temperature sensor (address 0x48)
- T�ch h?p: I2C + sysfs + Device Tree
- �?c datasheet sensor v� map v�o kernel API

---

## 2. TMP102 Temperature Sensor

### 2.1 Th�ng s?:

- Chip: TI TMP102
- Interface: I2C, address 0x48 (m?c d?nh)
- Resolution: 12-bit, 0.0625�C/LSB
- Range: -40�C t?i +125�C
- Register map:

| Register | Address | M� t? |
|----------|---------|-------|
| Temperature | 0x00 | 12-bit temperature (read-only) |
| Configuration | 0x01 | Config register |
| T_LOW | 0x02 | Low temperature threshold |
| T_HIGH | 0x03 | High temperature threshold |

### 2.2 �?c temperature:

Temperature register (0x00) l� 16-bit, MSB first:
```
Byte0: [D11 D10 D9 D8 D7 D6 D5 D4]
Byte1: [D3  D2  D1 D0  0  0  0  0]
```

Temperature = (Byte0 << 4 | Byte1 >> 4) � 0.0625�C

---

## 3. Driver ho�n ch?nh

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

/* �?c nhi?t d? (millidegree Celsius) */
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

	/* X? l� s? �m (12-bit signed) */
	if (raw & 0x800)
		raw |= 0xFFFFF000;

	/* Tr? v? millidegree: raw * 62.5 = raw * 625 / 10 */
	return (raw * 625) / 10;
}

/* Sysfs: d?c temperature */
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

	/* �?c config register d? verify chip */
	conf = i2c_smbus_read_word_swapped(client, TMP102_REG_CONF);
	if (conf < 0) {
		dev_err(&client->dev, "Failed to read config: %d\n", conf);
		return conf;
	}
	dev_info(&client->dev, "TMP102 config: 0x%04x\n", conf);

	/* �?c temperature th? */
	int temp = tmp102_read_temp(data);
	if (temp >= 0)
		dev_info(&client->dev, "Temperature: %d.%03d�C\n",
		         temp / 1000, abs(temp % 1000));

	/* T?o sysfs attributes */
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
# 25625 (= 25.625�C)

# Debug I2C bus
i2cdetect -y 2
# Th?y 48 = TMP102 detected

# �?c register tr?c ti?p
i2cget -y 2 0x48 0x00 w
```

---

## 6. Hwmon Integration (n�ng cao)

Thay v� sysfs th? c�ng, d�ng hwmon framework:

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

## 7. Ki?n th?c c?t l�i sau b�i n�y

1. **�?c datasheet sensor** ? register map ? I2C API
2. **`i2c_smbus_read_word_swapped()`** cho big-endian 16-bit register
3. **Sysfs attributes** d? export data cho userspace
4. **Mutex** b?o v? I2C transaction
5. **Hwmon** framework cho temperature/voltage/fan sensor

---

## 8. C�u h?i ki?m tra

1. TMP102 temperature register 12-bit du?c decode nhu th? n�o?
2. T?i sao d�ng `i2c_smbus_read_word_swapped` thay v� `i2c_smbus_read_word_data`?
3. T?i sao c?n mutex trong `tmp102_read_temp()`?
4. Vi?t h�m d?c 16-bit register b?ng `i2c_transfer()` thay v� SMBus.
5. Hwmon framework c� l?i g� so v?i sysfs th? c�ng?

---

## 9. Chu?n b? cho b�i sau

B�i ti?p theo: **B�i 21 - SPI Subsystem & Driver**

Ta s? h?c SPI framework: `spi_driver`, `spi_transfer`, `spi_message`, DT binding cho SPI device.
