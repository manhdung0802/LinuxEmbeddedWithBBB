# B�i 21 - AM335x I2C Hardware & I2C Subsystem

> **Tham chi?u ph?n c?ng BBB:** Xem [i2c.md](../mapping/i2c.md)

## 0. BBB Connection � I2C tr�n BeagleBone Black

> :warning: **AN TOAN DIEN:**
> - I2C tren BBB la **3.3V logic** - KHONG noi sensor 5V truc tiep (dung level-shifter)
> - Pull-up resistors: da co onboard cho I2C2 (4.7k ohm) - khong can them tru khi day dai >30cm
> - Kiem tra dia chi slave bang `i2cdetect -y -r 2` truoc khi code

### 0.1 So d? k?t n?i th?c t?

```
BeagleBone Black P9 header
                     +-----------------------------+
  P9.1  = GND  -----?�  Sensor / Slave Device      �
  P9.3  = 3.3V -----?�  V� d?: TMP102 (0x48)       �
  P9.19 = SCL  -----?�         MPU6050 (0x68)       �
  P9.20 = SDA  -----?�         OLED SSD1306 (0x3C)  �
                     +-----------------------------+
                     (Pull-up 4.7kO l�n 3.3V b?t bu?c
                      n?u kh�ng c� onboard pull-up)
```

### 0.2 B?ng I2C ? BBB Pin

| I2C Bus | Base Address | SCL Pin | SDA Pin | M?c d�ch BBB | D�ng cho h?c? |
|---------|-------------|---------|---------|-------------|--------------|
| I2C0 | 0x44E0B000 | - | - | PMIC TPS65217 (n?i b?) | ? KH�NG � crash board |
| I2C1 | 0x4802A000 | P9.17 | P9.18 | HDMI cape (CD24S2/DVI) | ?? C?n th?n |
| **I2C2** | **0x4819C000** | **P9.19** | **P9.20** | **D�NG CHO H?C** | ? |

### 0.3 Pinmux I2C2 (P9.19=SCL, P9.20=SDA)

```dts
/* DT pinmux cho I2C2 */
i2c2_pins: i2c2_pins {
    pinctrl-single,pins = <
        /* P9.19 = i2c2_scl, ball D18, mode 3 */
        AM33XX_PADCONF(AM335X_PIN_UART1_RTSN, PIN_INPUT_PULLUP, MUX_MODE3)
        /* P9.20 = i2c2_sda, ball D17, mode 3 */
        AM33XX_PADCONF(AM335X_PIN_UART1_CTSN, PIN_INPUT_PULLUP, MUX_MODE3)
    >;
};
```

### 0.4 Test I2C2 tr�n BBB (scan bus)

```bash
# Tr�n BBB sau khi boot:
i2cdetect -y -r 2   # Scan I2C2 bus (bus number 2)
# Output v� d? khi c� TMP102:
#      0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
# 40: -- -- -- -- -- -- -- -- 48 -- -- -- -- -- -- --
```

> **Ngu?n**: BeagleboneBlackP9HeaderTable.pdf (P9.19/P9.20); spruh73q.pdf Chapter 21 (I2C)

---

## 1. M?c ti�u b�i h?c
- Hi?u I2C bus protocol v� Linux I2C subsystem
- Vi?t I2C client driver v?i `i2c_driver`, `probe/remove`
- S? d?ng `i2c_transfer()`, `i2c_smbus_*()` API
- Device Tree binding cho I2C device tr�n I2C2 BBB
- AM335x I2C adapter registers v� clock config

---

## 2. I2C Protocol t�m t?t

```
     SDA ----------------------------+
               �      �              �
     SCL ------�      �              �
               �      �              �
          +--------+ +---------+ +-----------+
          � Master � � Slave 1 � � Slave 2   �
          � (AM335x� � 0x48    � � 0x68      �
          �  I2C2  � � TMP102  � � MPU6050   �
          � P9.19  � � P9.19   � � P9.19     �
          � P9.20) � � /P9.20  � � /P9.20    �
          +--------+ +---------+ +-----------+
```

- **2 d�y**: SDA (P9.20), SCL (P9.19) � d�ng I2C2 tr�n BBB
- **Master**: AM335x I2C2 controller (0x4819C000) � t?o clock, initiate transfer
- **Slave**: Sensor TMP102 (addr 0x48) ho?c MPU6050 (addr 0x68)
- **Speed**: Standard (100kHz), Fast (400kHz)

---

## 3. Linux I2C Architecture

```
+---------------------------------------------+
�  I2C Client Driver (your driver)            �
�  compatible = "ti,tmp102"                   �
�  i2c_smbus_read_byte_data()                 �
+---------------------------------------------+
                   �
+---------------------------------------------+
�  I2C Core (drivers/i2c/i2c-core.c)         �
�  - Match client driver ? DT node           �
�  - i2c_transfer() dispatch                  �
+---------------------------------------------+
                   �
+---------------------------------------------+
�  I2C Adapter Driver (i2c-omap.c)            �
�  - AM335x I2C controller driver             �
�  - Thao t�c register-level I2C hardware     �
+---------------------------------------------+
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

**Matching flow**: `compatible = "ti,tmp102"` ? kernel t�m `i2c_driver` c� c�ng compatible ? g?i `probe()`.

---

## 5. Vi?t I2C Client Driver

### 5.1 C?u tr�c:

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

    /* �?c chip ID d? verify */
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

### 6.1 SMBus API (don gi?n, khuy�n d�ng):

```c
/* �?c 1 byte t? register */
int val = i2c_smbus_read_byte_data(client, reg_addr);

/* Ghi 1 byte v�o register */
i2c_smbus_write_byte_data(client, reg_addr, value);

/* �?c 2 bytes (word) */
int val = i2c_smbus_read_word_data(client, reg_addr);

/* Ghi 2 bytes */
i2c_smbus_write_word_data(client, reg_addr, value);

/* �?c block data */
i2c_smbus_read_i2c_block_data(client, reg_addr, length, buffer);
```

### 6.2 Raw I2C transfer (ph?c t?p hon):

```c
/* �?c N bytes t? register addr */
static int my_i2c_read(struct i2c_client *client,
                        u8 reg, u8 *buf, int len)
{
    struct i2c_msg msgs[2];

    /* Message 1: ghi register address */
    msgs[0].addr = client->addr;
    msgs[0].flags = 0;            /* Write */
    msgs[0].len = 1;
    msgs[0].buf = &reg;

    /* Message 2: d?c data */
    msgs[1].addr = client->addr;
    msgs[1].flags = I2C_M_RD;    /* Read */
    msgs[1].len = len;
    msgs[1].buf = buf;

    return i2c_transfer(client->adapter, msgs, 2);
    /* Return: 2 = success (2 messages), < 0 = error */
}
```

### 6.3 Khi n�o d�ng g�?

| API | D�ng khi |
|-----|---------|
| `i2c_smbus_*` | Device h? tr? SMBus standard |
| `i2c_transfer` | C?n multi-message, bulk read |

---

## 7. AM335x I2C Adapters

AM335x c� 3 I2C adapter:

| Adapter | Base Address | DT node | M� t? |
|---------|-------------|---------|-------|
| I2C0 | 0x44E0B000 | `i2c0` | Cape EEPROM, PMIC (TPS65217) |
| I2C1 | 0x4802A000 | `i2c1` | Header P9 (17, 18, 24, 26) |
| I2C2 | 0x4819C000 | `i2c2` | Header P9 (19, 20) |

Ngu?n: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 21 - I2C

```bash
# Li?t k� I2C bus
ls /dev/i2c-*

# Scan I2C bus 2
i2cdetect -y 2
```

---

## 8. Ki?n th?c c?t l�i sau b�i n�y

1. **I2C client driver**: `i2c_driver` + `probe/remove` + `of_match_table`
2. **SMBus API**: `i2c_smbus_read/write_byte_data()` � don gi?n nh?t
3. **`i2c_transfer()`**: raw multi-message transfer
4. **DT**: child node trong I2C adapter, `reg = <slave_addr>`
5. **`module_i2c_driver()`** = shortcut dang k�

---

## 9. C�u h?i ki?m tra

1. I2C adapter driver v� client driver kh�c nhau ? di?m n�o?
2. `i2c_smbus_read_byte_data(client, 0x0F)` l�m g� tr�n bus?
3. Vi?t DT node cho sensor I2C t?i address 0x76 tr�n I2C2.
4. `i2c_transfer()` return 2 nghia l� g�?
5. Khi I2C transfer fail, nguy�n nh�n thu?ng g?p nh?t l� g�?

---

## 10. Chu?n b? cho b�i sau

B�i ti?p theo: **B�i 20 - I2C Driver th?c h�nh**

Ta s? vi?t driver ho�n ch?nh cho sensor TMP102 (temperature) ho?c MPU6050 (IMU) tr�n BBB.
