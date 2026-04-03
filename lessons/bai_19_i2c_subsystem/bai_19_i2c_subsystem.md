# Bài 19 - I2C Subsystem & Client Driver

## 1. Mục tiêu bài học
- Hiểu I2C bus protocol và Linux I2C subsystem
- Viết I2C client driver với `i2c_driver`, `probe/remove`
- Sử dụng `i2c_transfer()`, `i2c_smbus_*()` API
- Device Tree binding cho I2C device
- AM335x I2C adapter overview

---

## 2. I2C Protocol tóm tắt

```
     SDA ──────┬──────┬──────────────┐
               │      │              │
     SCL ──────┤      │              │
               │      │              │
          ┌────┴───┐ ┌┴────────┐ ┌──┴────────┐
          │ Master │ │ Slave 1 │ │ Slave 2   │
          │ (AM335x│ │ 0x48    │ │ 0x68      │
          │  I2C)  │ │ TMP102  │ │ MPU6050   │
          └────────┘ └─────────┘ └───────────┘
```

- **2 dây**: SDA (data), SCL (clock)
- **Master**: AM335x I2C controller — tạo clock, initiate transfer
- **Slave**: Sensor, EEPROM, ... — có 7-bit address (ví dụ 0x48)
- **Speed**: Standard (100kHz), Fast (400kHz)

---

## 3. Linux I2C Architecture

```
┌─────────────────────────────────────────────┐
│  I2C Client Driver (your driver)            │
│  compatible = "ti,tmp102"                   │
│  i2c_smbus_read_byte_data()                 │
└──────────────────┬──────────────────────────┘
                   │
┌──────────────────┴──────────────────────────┐
│  I2C Core (drivers/i2c/i2c-core.c)         │
│  - Match client driver ↔ DT node           │
│  - i2c_transfer() dispatch                  │
└──────────────────┬──────────────────────────┘
                   │
┌──────────────────┴──────────────────────────┐
│  I2C Adapter Driver (i2c-omap.c)            │
│  - AM335x I2C controller driver             │
│  - Thao tác register-level I2C hardware     │
└─────────────────────────────────────────────┘
```

---

## 4. Device Tree cho I2C

### 4.1 I2C adapter (controller):

```dts
/* Trong am33xx.dtsi */
i2c0: i2c@44e0b000 {
    compatible = "ti,omap4-i2c";
    reg = <0x44e0b000 0x1000>;
    interrupts = <70>;
    #address-cells = <1>;
    #size-cells = <0>;
    clock-frequency = <100000>;
    status = "okay";
};
```

### 4.2 I2C client device:

```dts
&i2c2 {
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&i2c2_pins>;
    clock-frequency = <400000>;

    /* Temperature sensor */
    tmp102: tmp102@48 {
        compatible = "ti,tmp102";
        reg = <0x48>;
    };

    /* IMU sensor */
    mpu6050: mpu6050@68 {
        compatible = "invensense,mpu6050";
        reg = <0x68>;
        interrupt-parent = <&gpio2>;
        interrupts = <8 IRQ_TYPE_EDGE_FALLING>;
    };
};
```

**Matching flow**: `compatible = "ti,tmp102"` → kernel tìm `i2c_driver` có cùng compatible → gọi `probe()`.

---

## 5. Viết I2C Client Driver

### 5.1 Cấu trúc:

```c
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>

struct my_sensor {
    struct i2c_client *client;
    /* sensor-specific data */
};

static int my_sensor_probe(struct i2c_client *client)
{
    struct my_sensor *sensor;

    sensor = devm_kzalloc(&client->dev, sizeof(*sensor), GFP_KERNEL);
    if (!sensor)
        return -ENOMEM;

    sensor->client = client;
    i2c_set_clientdata(client, sensor);

    /* Đọc chip ID để verify */
    int id = i2c_smbus_read_byte_data(client, 0x00);  /* Register 0x00 */
    dev_info(&client->dev, "Chip ID: 0x%02x\n", id);

    return 0;
}

static void my_sensor_remove(struct i2c_client *client)
{
    dev_info(&client->dev, "Removed\n");
}

static const struct of_device_id my_sensor_of_match[] = {
    { .compatible = "my,sensor" },
    { },
};
MODULE_DEVICE_TABLE(of, my_sensor_of_match);

static const struct i2c_device_id my_sensor_id[] = {
    { "my-sensor", 0 },
    { },
};
MODULE_DEVICE_TABLE(i2c, my_sensor_id);

static struct i2c_driver my_sensor_driver = {
    .driver = {
        .name = "my-sensor",
        .of_match_table = my_sensor_of_match,
    },
    .probe = my_sensor_probe,
    .remove = my_sensor_remove,
    .id_table = my_sensor_id,
};

module_i2c_driver(my_sensor_driver);
MODULE_LICENSE("GPL");
```

---

## 6. I2C Transfer API

### 6.1 SMBus API (đơn giản, khuyên dùng):

```c
/* Đọc 1 byte từ register */
int val = i2c_smbus_read_byte_data(client, reg_addr);

/* Ghi 1 byte vào register */
i2c_smbus_write_byte_data(client, reg_addr, value);

/* Đọc 2 bytes (word) */
int val = i2c_smbus_read_word_data(client, reg_addr);

/* Ghi 2 bytes */
i2c_smbus_write_word_data(client, reg_addr, value);

/* Đọc block data */
i2c_smbus_read_i2c_block_data(client, reg_addr, length, buffer);
```

### 6.2 Raw I2C transfer (phức tạp hơn):

```c
/* Đọc N bytes từ register addr */
static int my_i2c_read(struct i2c_client *client,
                        u8 reg, u8 *buf, int len)
{
    struct i2c_msg msgs[2];

    /* Message 1: ghi register address */
    msgs[0].addr = client->addr;
    msgs[0].flags = 0;            /* Write */
    msgs[0].len = 1;
    msgs[0].buf = &reg;

    /* Message 2: đọc data */
    msgs[1].addr = client->addr;
    msgs[1].flags = I2C_M_RD;    /* Read */
    msgs[1].len = len;
    msgs[1].buf = buf;

    return i2c_transfer(client->adapter, msgs, 2);
    /* Return: 2 = success (2 messages), < 0 = error */
}
```

### 6.3 Khi nào dùng gì?

| API | Dùng khi |
|-----|---------|
| `i2c_smbus_*` | Device hỗ trợ SMBus standard |
| `i2c_transfer` | Cần multi-message, bulk read |

---

## 7. AM335x I2C Adapters

AM335x có 3 I2C adapter:

| Adapter | Base Address | DT node | Mô tả |
|---------|-------------|---------|-------|
| I2C0 | 0x44E0B000 | `i2c0` | Cape EEPROM, PMIC (TPS65217) |
| I2C1 | 0x4802A000 | `i2c1` | Header P9 (17, 18, 24, 26) |
| I2C2 | 0x4819C000 | `i2c2` | Header P9 (19, 20) |

Nguồn: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 21 - I2C

```bash
# Liệt kê I2C bus
ls /dev/i2c-*

# Scan I2C bus 2
i2cdetect -y 2
```

---

## 8. Kiến thức cốt lõi sau bài này

1. **I2C client driver**: `i2c_driver` + `probe/remove` + `of_match_table`
2. **SMBus API**: `i2c_smbus_read/write_byte_data()` — đơn giản nhất
3. **`i2c_transfer()`**: raw multi-message transfer
4. **DT**: child node trong I2C adapter, `reg = <slave_addr>`
5. **`module_i2c_driver()`** = shortcut đăng ký

---

## 9. Câu hỏi kiểm tra

1. I2C adapter driver và client driver khác nhau ở điểm nào?
2. `i2c_smbus_read_byte_data(client, 0x0F)` làm gì trên bus?
3. Viết DT node cho sensor I2C tại address 0x76 trên I2C2.
4. `i2c_transfer()` return 2 nghĩa là gì?
5. Khi I2C transfer fail, nguyên nhân thường gặp nhất là gì?

---

## 10. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 20 - I2C Driver thực hành**

Ta sẽ viết driver hoàn chỉnh cho sensor TMP102 (temperature) hoặc MPU6050 (IMU) trên BBB.
