# Bài 25 - Device Tree Chuyên Sâu: Custom DTS, Binding Driver, Overlay Runtime

## 1. Mục tiêu bài học

- Viết custom DTS từ đầu cho device tự thiết kế
- Hiểu DT binding: cách driver đọc properties
- Tạo và apply overlay runtime qua configfs
- Debug DT: kiểm tra merge, conflict
- Kết nối DTS node ↔ platform driver (bài 19) ↔ char device (bài 18)

---

## 2. Cú pháp DTS Nâng Cao

### 2.1 #address-cells và #size-cells

```dts
/* Mỗi child node có reg = <địa chỉ, kích thước>
 * #address-cells = 1 → 1 cell (32-bit) cho địa chỉ
 * #size-cells    = 1 → 1 cell (32-bit) cho kích thước
 */
soc {
    #address-cells = <1>;
    #size-cells = <1>;

    gpio0: gpio@44e07000 {
        reg = <0x44e07000 0x1000>;  /* base=0x44e07000, size=0x1000 */
    };
};

/* Nếu size-cells = 0 (không có kích thước, chỉ địa chỉ — ví dụ I2C device) */
i2c1: i2c@4802a000 {
    #address-cells = <1>;
    #size-cells = <0>;    /* I2C child chỉ có địa chỉ, không có size */

    tmp102@48 {
        reg = <0x48>;     /* chỉ địa chỉ I2C */
    };
};
```

### 2.2 Interrupt Specifier

```dts
intc: interrupt-controller@48200000 {
    compatible = "ti,omap4-intc";
    reg = <0x48200000 0x1000>;
    interrupt-controller;       /* đây là interrupt controller */
    #interrupt-cells = <1>;     /* 1 cell = interrupt number */
};

gpio1: gpio@4804c000 {
    compatible = "ti,omap4-gpio";
    /* GPIO1 IRQ = channel 98 trong INTC */
    interrupts = <98>;
    interrupt-parent = <&intc>;  /* thuộc intc */

    /* GPIO1 chính nó cũng là interrupt controller cho các GPIO pin */
    interrupt-controller;
    #interrupt-cells = <2>;      /* 2 cells: pin number + type (edge/level) */
};

/* Device sử dụng GPIO interrupt */
button@0 {
    interrupt-parent = <&gpio1>;
    interrupts = <14 IRQ_TYPE_EDGE_FALLING>;  /* GPIO1_14, falling edge */
};
```

### 2.3 Clock Specifier

```dts
/* AM335x clock provider */
clkctrl: clock@44e00000 {
    #clock-cells = <2>;     /* 2 cells: module offset + type */
};

/* Device yêu cầu clock */
uart0: serial@44e09000 {
    clocks = <&clkctrl 0x6C 0>,  /* UART0_CLKCTRL offset=0x6C, type=0 */
             <&clkctrl 0x6C 1>;
    clock-names = "fck", "ick";  /* tên tương ứng */
};
```

---

## 3. Viết Custom DTS: Multi-sensor Board

Giả sử bạn thiết kế board có:
- 3 LED điều khiển qua GPIO1
- 1 button trên GPIO1_14
- 1 cảm biến I2C (TMP102 @ 0x48) trên I2C1
- 1 SPI ADC (MCP3204) trên SPI0

```dts
/*
 * bbb-custom-sensor-board.dts
 * Custom sensor board cho BeagleBone Black
 */
/dts-v1/;
#include "am335x-boneblack.dts"   /* kế thừa BBB base */

/ {
    model = "BBB Custom Sensor Board v1.0";
    compatible = "mycompany,bbb-sensor-board", "ti,am335x-bone-black",
                 "ti,am335x-bone", "ti,am33xx";

    /* ── LED Group ── */
    leds {
        compatible = "gpio-leds";
        pinctrl-names = "default";
        pinctrl-0 = <&led_pins>;

        led0 {
            label = "sensor-board:red:status";
            gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;   /* GPIO1_21 */
            default-state = "off";
            linux,default-trigger = "heartbeat";    /* blink theo load */
        };

        led1 {
            label = "sensor-board:green:tx";
            gpios = <&gpio1 22 GPIO_ACTIVE_HIGH>;   /* GPIO1_22 */
            linux,default-trigger = "none";
        };

        led2 {
            label = "sensor-board:blue:rx";
            gpios = <&gpio1 23 GPIO_ACTIVE_HIGH>;   /* GPIO1_23 */
            linux,default-trigger = "none";
        };
    };

    /* ── Button ── */
    buttons {
        compatible = "gpio-keys";
        pinctrl-names = "default";
        pinctrl-0 = <&button_pins>;

        button0 {
            label = "user-button";
            gpios = <&gpio1 14 GPIO_ACTIVE_LOW>;  /* GPIO1_14, active-low */
            linux,code = <KEY_ENTER>;              /* KEY code */
            wakeup-source;                         /* có thể wake từ suspend */
        };
    };
};

/* ── Pinmux cho LED và Button ── */
&am33xx_pinmux {
    led_pins: pinmux_led_pins {
        pinctrl-single,pins = <
            /* GPIO1_21, 22, 23 làm output */
            0x054 (PIN_OUTPUT | MUX_MODE7)   /* gpmc_a5 → GPIO1_21 */
            0x058 (PIN_OUTPUT | MUX_MODE7)   /* gpmc_a6 → GPIO1_22 */
            0x05C (PIN_OUTPUT | MUX_MODE7)   /* gpmc_a7 → GPIO1_23 */
        >;
    };

    button_pins: pinmux_button_pins {
        pinctrl-single,pins = <
            /* GPIO1_14 làm input với pull-up */
            0x038 (PIN_INPUT_PULLUP | MUX_MODE7)  /* gpmc_ad14 → GPIO1_14 */
        >;
    };
};

/* ── I2C1 với TMP102 ── */
&i2c1 {
    status = "okay";
    clock-frequency = <100000>;

    tmp102@48 {
        compatible = "ti,tmp102";
        reg = <0x48>;
    };
};

/* ── SPI0 với MCP3204 ADC ── */
&spi0 {
    status = "okay";
    #address-cells = <1>;
    #size-cells = <0>;

    mcp3204@0 {
        compatible = "microchip,mcp3204";
        reg = <0>;                   /* CS0 */
        spi-max-frequency = <1000000>;
        vref-supply = <&vdd_3v3>;   /* 3.3V reference */
    };
};
```

---

## 4. DT Binding Documentation

Mỗi `compatible` string phải có **binding document** mô tả các property:

```
Documentation/devicetree/bindings/leds/gpio-leds.yaml
Documentation/devicetree/bindings/input/gpio-keys.yaml
Documentation/devicetree/bindings/iio/adc/microchip,mcp320x.yaml
```

Ví dụ binding tự viết cho custom driver:

```yaml
# Documentation/devicetree/bindings/misc/mycompany,myled.yaml
%YAML 1.2
---
$id: http://devicetree.org/schemas/misc/mycompany,myled.yaml#
$schema: http://devicetree.org/meta-schemas/core.yaml#

title: MyCompany LED controller

properties:
  compatible:
    const: mycompany,myled

  reg:
    maxItems: 1

  my-gpio-pin:
    $ref: /schemas/types.yaml#/definitions/uint32
    description: GPIO pin number (0-31) within the GPIO bank
    minimum: 0
    maximum: 31

required:
  - compatible
  - reg
  - my-gpio-pin
```

---

## 5. Apply Overlay Runtime qua ConfigFS

Kernel 4.14+ hỗ trợ apply overlay qua configfs (không cần reboot).

```bash
# Mount configfs (thường đã mounted)
mount | grep configfs
mount -t configfs none /sys/kernel/config

# Apply overlay:
mkdir /sys/kernel/config/device-tree/overlays/my-overlay
cat BB-I2C1-00A0.dtbo > /sys/kernel/config/device-tree/overlays/my-overlay/dtbo
echo 1 > /sys/kernel/config/device-tree/overlays/my-overlay/status

# Xác nhận apply thành công
ls /sys/kernel/config/device-tree/overlays/my-overlay/
# status: Applied

# Remove overlay
rmdir /sys/kernel/config/device-tree/overlays/my-overlay
```

---

## 6. Debug Device Tree

```bash
# Xem DT hiện tại đang chạy
ls /proc/device-tree/

# Decompile DTB đang dùng
dtc -I dtb -O dts /boot/dtbs/$(uname -r)/am335x-boneblack.dtb \
    -o /tmp/current.dts 2>/dev/null
less /tmp/current.dts

# Kiểm tra node cụ thể
cat /proc/device-tree/ocp/i2c@4802a000/status
# okay

# Tìm node theo compatible
grep -r "ti,tmp102" /proc/device-tree/

# Xem merge DT với overlay (dùng dtmerge nếu có)
dtmerge base.dtb merged.dtb overlay.dtbo
dtc -I dtb -O dts merged.dtb | grep -A10 "tmp102"

# Validate DTS với dtc
dtc -I dts -O dtb -o /dev/null -W no-unit_address_vs_reg my.dts
```

---

## 7. Câu hỏi kiểm tra

1. `#address-cells = <1>` và `#size-cells = <0>` thường thấy ở đâu? Tại sao?
2. `interrupt-controller` và `#interrupt-cells` trong một node có ý nghĩa gì?
3. Tại sao `gpio-leds` driver có thể dùng bất kỳ GPIO nào mà không cần sửa driver?
4. Overlay configfs vs overlay qua `/boot/uEnv.txt` — khác nhau thế nào?
5. Binding YAML documentation dùng để làm gì? Ai enforce nó?

---

## 8. Tài liệu tham khảo

| Nội dung | Tài liệu | Ghi chú |
|----------|----------|---------|
| DT spec | devicetree-specification-v0.3.pdf | nodes, properties, phandles |
| AM335x DTS | arch/arm/boot/dts/am33xx.dtsi | GPIO, UART, I2C, SPI nodes |
| gpio-leds binding | Documentation/devicetree/bindings/leds/gpio-leds.txt | gpio-leds compatible |
| Pinmux offsets | spruh73q.pdf, Chapter 9 (Control Module) | Table 9-7 |
| configfs overlay | Documentation/devicetree/overlay-notes.txt | Runtime apply |
