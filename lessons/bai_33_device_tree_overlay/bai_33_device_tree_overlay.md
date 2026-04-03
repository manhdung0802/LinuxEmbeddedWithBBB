# Bài 33 - Device Tree Overlay

## 1. Mục tiêu bài học
- Hiểu Device Tree Overlay mechanism
- Cách tạo và áp dụng overlay tại runtime
- Cape support trên BeagleBone Black
- ConfigFS interface cho DT overlay
- Thực hành viết overlay cho GPIO, I2C, SPI

---

## 2. Tại sao cần DT Overlay?

### 2.1 Vấn đề

Base Device Tree (DTB) được compile lúc build time, mô tả phần cứng cố định.
Nhưng BBB có các cape (expansion board) gắn vào P8/P9 header → phần cứng thay đổi.

### 2.2 Giải pháp: Overlay

```
┌──────────────────────┐
│   Base DTB           │  (Hardware cố định trên board)
│   am335x-boneblack   │
└──────────┬───────────┘
           │  apply overlay
           ▼
┌──────────────────────┐
│   Base DTB + Overlay │  (Thêm cape, sensor, LED...)
│   (merged at runtime)│
└──────────────────────┘
```

---

## 3. DT Overlay Syntax

### 3.1 Cấu trúc overlay

```dts
/dts-v1/;
/plugin/;

/* Fragment: mỗi fragment modify một node trong base DT */
&{/} {
    /* Thêm node mới vào root */
    my_leds {
        compatible = "gpio-leds";
        my_led {
            gpios = <&gpio1 28 GPIO_ACTIVE_HIGH>;
            label = "my-led";
            default-state = "off";
        };
    };
};

/* Modify node có sẵn */
&i2c2 {
    status = "okay";

    /* Thêm device con */
    tmp102@48 {
        compatible = "ti,tmp102";
        reg = <0x48>;
    };
};

/* Modify pinmux */
&am33xx_pinmux {
    my_pins: my_pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_GPMC_A5, PIN_OUTPUT, MUX_MODE7)
        >;
    };
};
```

### 3.2 Syntax quan trọng

| Syntax | Mô tả |
|--------|-------|
| `/plugin/;` | Đánh dấu file là overlay |
| `&node_label` | Reference tới label trong base DT |
| `&{/path}` | Reference tới node bằng path |
| `status = "okay"` | Enable node bị disable |
| `status = "disabled"` | Disable node |

---

## 4. Compile và Apply Overlay

### 4.1 Compile (trên host)

```bash
# Compile overlay thành .dtbo
dtc -@ -I dts -O dtb -o my_overlay.dtbo my_overlay.dts

# -@ : generate symbols (cần cho overlay resolution)
# Output: .dtbo (Device Tree Blob Overlay)
```

### 4.2 Apply qua ConfigFS

```bash
# Trên BBB, kiểm tra configfs
mount | grep configfs
# configfs on /sys/kernel/config type configfs

# Tạo overlay slot
sudo mkdir /sys/kernel/config/device-tree/overlays/my_overlay

# Apply overlay
sudo cp my_overlay.dtbo /lib/firmware/
echo my_overlay.dtbo > /sys/kernel/config/device-tree/overlays/my_overlay/path

# Kiểm tra status
cat /sys/kernel/config/device-tree/overlays/my_overlay/status
# applied

# Remove overlay
sudo rmdir /sys/kernel/config/device-tree/overlays/my_overlay
```

### 4.3 Apply qua U-Boot

```
# Trong /boot/uEnv.txt
dtb_overlay=/lib/firmware/my_overlay.dtbo
```

---

## 5. Ví dụ thực tế

### 5.1 Overlay cho LED trên GPIO

```dts
/dts-v1/;
/plugin/;

#include <dt-bindings/gpio/gpio.h>
#include <dt-bindings/pinctrl/am33xx.h>

&am33xx_pinmux {
    user_led_pins: user_led_pins {
        pinctrl-single,pins = <
            /* P9.12 = GPIO1_28, Mode 7 = GPIO */
            AM33XX_PADCONF(AM335X_PIN_GPMC_BEN1, PIN_OUTPUT, MUX_MODE7)
        >;
    };
};

&{/} {
    user_leds {
        compatible = "gpio-leds";
        pinctrl-names = "default";
        pinctrl-0 = <&user_led_pins>;

        ext_led {
            gpios = <&gpio1 28 GPIO_ACTIVE_HIGH>;
            label = "ext-led";
            linux,default-trigger = "heartbeat";
        };
    };
};
```

### 5.2 Overlay cho I2C sensor

```dts
/dts-v1/;
/plugin/;

#include <dt-bindings/pinctrl/am33xx.h>

&am33xx_pinmux {
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

    bmp280@76 {
        compatible = "bosch,bmp280";
        reg = <0x76>;
    };
};
```

### 5.3 Overlay cho SPI device

```dts
/dts-v1/;
/plugin/;

#include <dt-bindings/pinctrl/am33xx.h>

&am33xx_pinmux {
    spi1_pins: spi1_pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_MCASP0_ACLKX, PIN_INPUT_PULLUP, MUX_MODE3)
            AM33XX_PADCONF(AM335X_PIN_MCASP0_FSX, PIN_OUTPUT_PULLUP, MUX_MODE3)
            AM33XX_PADCONF(AM335X_PIN_MCASP0_AXR0, PIN_INPUT_PULLUP, MUX_MODE3)
            AM33XX_PADCONF(AM335X_PIN_MCASP0_AHCLKR, PIN_OUTPUT_PULLUP, MUX_MODE3)
        >;
    };
};

&spi1 {
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&spi1_pins>;

    spidev@0 {
        compatible = "linux,spidev";
        reg = <0>;
        spi-max-frequency = <16000000>;
    };
};
```

---

## 6. Cape Manager (BBB specific)

### 6.1 Cape EEPROM

BBB cape có EEPROM (I2C) chứa thông tin:

| Field | Mô tả |
|-------|-------|
| Board Name | Tên cape |
| Version | Hardware version |
| Manufacturer | Tên nhà sản xuất |
| Part Number | Mã sản phẩm |
| Serial Number | Số serial |

### 6.2 Auto-load overlay

```bash
# Liệt kê cape slots
cat /sys/devices/platform/bone_capemgr/slots

# Load cape overlay
echo "BB-GPIO-LED" > /sys/devices/platform/bone_capemgr/slots
```

---

## 7. DT Overlay trong code (kernel)

```c
#include <linux/of_fdt.h>

/* Kernel API cho overlay (thường không cần trong driver) */
int of_overlay_fdt_apply(void *overlay_fdt, u32 overlay_fdt_size,
                          int *ret_ovcs_id, struct device_node *target);

void of_overlay_remove(int *ovcs_id);
```

---

## 8. Kiến thức cốt lõi

1. **DT Overlay**: modify base DTB tại runtime, `/plugin/` directive
2. **Fragment**: mỗi fragment target một node (qua label hoặc path)
3. **Compile**: `dtc -@ -I dts -O dtb -o file.dtbo file.dts`
4. **Apply**: ConfigFS (`/sys/kernel/config/device-tree/overlays/`) hoặc U-Boot
5. **BBB cape**: auto-detect qua EEPROM, load overlay tương ứng
6. **Pinmux + device + driver**: overlay cấu hình pins + add device → trigger driver probe

---

## 9. Câu hỏi kiểm tra

1. `/plugin/` trong DTS có ý nghĩa gì?
2. Cách refer tới node base DT trong overlay?
3. `-@` flag khi compile overlay dùng để làm gì?
4. Cách apply/remove overlay tại runtime?
5. Viết overlay enable UART4 (P9.11 TX, P9.13 RX) trên BBB.

---

## 10. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 31 - Debug Techniques**

Ta sẽ học các kỹ thuật debug kernel driver: printk, dynamic debug, ftrace, debugfs.
