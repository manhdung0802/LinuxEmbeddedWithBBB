# Bài 16 - Device Tree: Cấu Trúc, Overlay và Custom DTS

## 1. Mục tiêu bài học

- Hiểu Device Tree là gì và tại sao Linux cần nó
- Nắm cú pháp DTS: node, property, label, phandle
- Đọc và phân tích file DTS của BBB
- Viết Device Tree Overlay để cấu hình peripheral (I2C, SPI, UART, GPIO)
- Apply overlay runtime mà không cần reboot

---

## 2. Device Tree là gì?

**Device Tree** là cấu trúc dữ liệu mô tả phần cứng của hệ thống — cho phép kernel biết phần cứng nào có mặt mà **không cần hard-code** thông tin phần cứng vào kernel.

### Tại sao cần Device Tree?

Trước DT, mỗi board có file `board-xxx.c` riêng trong kernel. Khi thêm board mới → phải sửa kernel → rất khó bảo trì.

Với Device Tree:
```
Kernel ← một bản kernel chung
DT (.dtb) ← file riêng mô tả phần cứng của từng board
```

→ Cùng một kernel binary có thể chạy trên nhiều board khác nhau.

### Luồng hoạt động

```
.dts (source text) 
    │ dtc (Device Tree Compiler)
    ▼
.dtb (binary blob)
    │ U-Boot loads
    ▼
Kernel reads dtb → tạo platform devices → probe drivers
```

---

## 3. Cú Pháp DTS

### 3.1 Cấu trúc cơ bản

```dts
/dts-v1/;

/ {                             /* root node */
    model = "BeagleBone Black";
    compatible = "ti,am335x-bone-black", "ti,am335x-bone", "ti,am33xx";

    cpus {                      /* child node */
        cpu@0 {                 /* node tên "cpu", địa chỉ 0 */
            compatible = "arm,cortex-a8";
            reg = <0>;          /* "reg" property = address */
        };
    };

    memory@80000000 {           /* RAM at 0x80000000 */
        device_type = "memory";
        reg = <0x80000000 0x20000000>;  /* base, size */
    };
};
```

### 3.2 Các loại Property

```dts
/* String */
model = "BeagleBone Black";
compatible = "ti,am33xx";

/* Number (32-bit, big-endian) */
reg = <0x44E07000 0x1000>;      /* base=0x44E07000, size=0x1000 */
clock-frequency = <24000000>;

/* Array */
interrupts = <96 IRQ_TYPE_LEVEL_HIGH>;

/* Boolean (chỉ cần tên property, không cần value) */
gpio-controller;
#gpio-cells = <2>;

/* Phandle reference */
clocks = <&dpll_core_ck>;      /* & = reference đến label */
```

### 3.3 Label và Phandle

```dts
/* Đặt label cho node */
gpio1: gpio@4804C000 {
    compatible = "ti,omap4-gpio";
    reg = <0x4804C000 0x1000>;
    /* ... */
};

/* Reference đến node có label */
led_usr0 {
    gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;
    /*         ↑     ↑         ↑
               phandle  pin  flags  */
};
```

---

## 4. DTS của BeagleBone Black

File DTS của BBB nằm trong kernel source:
```
arch/arm/boot/dts/am335x-boneblack.dts
```

Cấu trúc include:
```
am335x-boneblack.dts
    └── includes am335x-bone-common.dtsi
        └── includes am33xx.dtsi
            └── includes am33xx-clocks.dtsi
```

### 4.1 Ví dụ node GPIO trong DTS BBB

```dts
/* am33xx.dtsi */
gpio1: gpio@4804c000 {
    compatible = "ti,omap4-gpio";
    ti,hwmods = "gpio1";
    gpio-controller;
    #gpio-cells = <2>;
    interrupt-controller;
    #interrupt-cells = <2>;
    reg = <0x4804c000 0x1000>;
    interrupts = <98 IRQ_TYPE_LEVEL_HIGH>;
};
```

### 4.2 Ví dụ node UART

```dts
uart1: serial@48022000 {
    compatible = "ti,am3352-uart", "ti,omap2-uart";
    ti,hwmods = "uart2";
    reg = <0x48022000 0x2000>;
    interrupts = <73 IRQ_TYPE_LEVEL_HIGH>;
    status = "disabled";        /* tắt mặc định */
};
```

---

## 5. Device Tree Overlay

**Overlay** cho phép thêm/sửa device tree **runtime** mà không cần reboot.

Dùng để:
- Cấu hình chân P8/P9 cho peripheral (I2C, SPI, UART)
- Thêm thiết bị ngoài (cảm biến, display)

### 5.1 Cấu trúc Overlay

```dts
/dts-v1/;
/plugin/;

/ {
    compatible = "ti,beaglebone", "ti,beaglebone-black", "ti,beaglebone-green";
    part-number = "BB-I2C1";
    version = "00A0";

    /* Pinmux fragment */
    fragment@0 {
        target = <&am33xx_pinmux>;
        __overlay__ {
            bb_i2c1_pins: pinmux_bb_i2c1_pins {
                pinctrl-single,pins = <
                    /* P9.17 = spi0_cs0 → I2C1_SCL (mode 2) */
                    0x15C (PIN_INPUT_PULLUP | MUX_MODE2)
                    /* P9.18 = spi0_d1  → I2C1_SDA (mode 2) */
                    0x158 (PIN_INPUT_PULLUP | MUX_MODE2)
                >;
            };
        };
    };

    /* I2C1 enable fragment */
    fragment@1 {
        target = <&i2c1>;
        __overlay__ {
            status = "okay";
            pinctrl-names = "default";
            pinctrl-0 = <&bb_i2c1_pins>;
            clock-frequency = <100000>;
        };
    };
};
```

### 5.2 Biên dịch Overlay

```bash
# Biên dịch .dts → .dtbo
dtc -O dtb -o BB-I2C1-00A0.dtbo -b 0 -@ BB-I2C1-00A0.dts

# Copy vào /lib/firmware
cp BB-I2C1-00A0.dtbo /lib/firmware/
```

### 5.3 Apply Overlay (BBB Debian)

```bash
# Cách 1: config-pin (đơn giản nhất)
config-pin P9.17 i2c
config-pin P9.18 i2c

# Cách 2: cape-manager (kernel ≤ 4.x)
echo BB-I2C1:00A0 > /sys/devices/platform/bone_capemgr/slots

# Cách 3: udev/overlays trong /boot/uEnv.txt (persistent)
# Thêm vào /boot/uEnv.txt:
# dtb_overlay=/lib/firmware/BB-I2C1-00A0.dtbo
```

---

## 6. Ví dụ: Custom Device Tree cho cảm biến TMP102

```dts
/*
 * bbb-tmp102-overlay.dts
 * Thêm cảm biến nhiệt độ TMP102 trên I2C1 (P9.17/P9.18)
 */
/dts-v1/;
/plugin/;

/ {
    compatible = "ti,beaglebone-black";
    part-number = "BBB-TMP102";
    version = "00A0";

    fragment@0 {
        target = <&am33xx_pinmux>;
        __overlay__ {
            bbb_tmp102_pins: pinmux_bbb_tmp102_pins {
                pinctrl-single,pins = <
                    0x15C (PIN_INPUT_PULLUP | MUX_MODE2)  /* I2C1_SCL */
                    0x158 (PIN_INPUT_PULLUP | MUX_MODE2)  /* I2C1_SDA */
                >;
            };
        };
    };

    fragment@1 {
        target = <&i2c1>;
        __overlay__ {
            status = "okay";
            pinctrl-names = "default";
            pinctrl-0 = <&bbb_tmp102_pins>;
            clock-frequency = <100000>;
            #address-cells = <1>;
            #size-cells = <0>;

            /* TMP102 tại địa chỉ 0x48 */
            tmp102: tmp102@48 {
                compatible = "ti,tmp102";
                reg = <0x48>;
                /* Driver sẽ tự động tạo /sys/class/thermal/thermal_zone* */
            };
        };
    };
};
```

Sau khi apply overlay, đọc nhiệt độ:
```bash
cat /sys/class/thermal/thermal_zone*/temp
# Output: 27500 (= 27.5°C)
```

---

## 7. Câu hỏi kiểm tra

1. DTS và DTB khác nhau thế nào? Lệnh nào chuyển đổi DTS → DTB?
2. `compatible` property dùng để làm gì trong Device Tree?
3. Trong overlay fragment, `target = <&i2c1>` có nghĩa là gì?
4. Tại sao nhiều node trong DTS mặc định có `status = "disabled"`?
5. Lệnh `config-pin P9.17 i2c` làm gì ở mức overlay/pinmux?

---

## 8. Tài liệu tham khảo

| Nội dung | Tài liệu | Section |
|----------|----------|---------|
| AM335x Device Tree | Linux kernel source | arch/arm/boot/dts/am33xx.dtsi |
| BBB DTS | Linux kernel source | arch/arm/boot/dts/am335x-boneblack.dts |
| Pinmux offsets | spruh73q.pdf | Table 9-7 (Control Module) |
| Device Tree spec | devicetree.org | devicetree-specification-v0.3.pdf |
| BBB cape overlays | github.com/beagleboard/bb.org-overlays | — |
