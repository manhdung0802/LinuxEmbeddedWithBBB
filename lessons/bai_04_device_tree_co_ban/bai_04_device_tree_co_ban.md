# Bài 4 - Device Tree cơ bản

## 1. Mục tiêu bài học
- Hiểu Device Tree là gì và tại sao Linux cần nó
- Nắm cú pháp DTS: node, property, phandle, compatible
- Đọc hiểu Device Tree của BeagleBone Black
- Biên dịch DTS thành DTB bằng `dtc`
- Viết Device Tree node đơn giản cho peripheral

---

## 2. Tại sao cần Device Tree?

### 2.1 Vấn đề trước khi có Device Tree

Trước đây (kernel < 3.x), thông tin phần cứng được **hardcode** trong kernel source:

```c
/* Cách cũ: mã C trong kernel mô tả phần cứng */
static struct platform_device my_gpio = {
    .name = "am335x-gpio",
    .resource = {
        { .start = 0x4804C000, .end = 0x4804CFFF, .flags = IORESOURCE_MEM },
        { .start = 96, .flags = IORESOURCE_IRQ },
    },
};
```

**Vấn đề**: Mỗi board khác nhau phải sửa kernel source → không linh hoạt, khó bảo trì.

### 2.2 Device Tree giải quyết vấn đề

Device Tree tách biệt **mô tả phần cứng** ra khỏi kernel code:

```
┌──────────────────┐     ┌──────────────────┐
│  Linux Kernel    │     │  Device Tree     │
│  (generic code)  │  +  │  (board-specific)│
│  drivers/gpio/   │     │  am335x-bbb.dtb  │
└──────────────────┘     └──────────────────┘
         │                        │
         └────────────────────────┘
                    │
         Kernel đọc DT để biết:
         - Board có GPIO nào?
         - Ở địa chỉ nào?
         - Interrupt số mấy?
         - Dùng driver nào?
```

**Kết quả**: Cùng một kernel image có thể chạy trên nhiều board ARM khác nhau, chỉ cần DTB khác.

---

## 3. Cấu trúc Device Tree

### 3.1 File formats

| File | Extension | Mô tả |
|------|-----------|-------|
| DTS | `.dts` | Device Tree Source - file text người đọc được |
| DTSI | `.dtsi` | DTS Include - file chia sẻ giữa các board |
| DTB | `.dtb` | Device Tree Binary - kernel đọc file này |
| DTBO | `.dtbo` | DT Overlay Binary - patch thêm vào DTB |

### 3.2 Quan hệ giữa các file DTS/DTSI:

```
am33xx.dtsi            ← Mô tả SoC AM335x (chung cho mọi board AM335x)
  └── am335x-bone-common.dtsi  ← Chung cho các board BeagleBone
        └── am335x-boneblack.dts   ← Riêng cho BBB
```

Kernel source path: `arch/arm/boot/dts/am335x-boneblack.dts`

---

## 4. Cú pháp DTS

### 4.1 Cấu trúc cơ bản:

```dts
/dts-v1/;              /* Khai báo version */

/ {                    /* Root node */
    compatible = "ti,am335x-bone-black";
    model = "TI AM335x BeagleBone Black";

    /* Child node */
    memory@80000000 {
        device_type = "memory";
        reg = <0x80000000 0x20000000>;  /* 512MB @ 0x80000000 */
    };

    leds {
        compatible = "gpio-leds";

        led0 {
            label = "beaglebone:green:usr0";
            gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;
            default-state = "on";
        };
    };
};
```

### 4.2 Node (nút):

Node mô tả một thành phần phần cứng hoặc một nhóm logic:

```dts
node-name@unit-address {
    /* properties */
    /* child nodes */
};
```

- **node-name**: Tên mô tả chức năng (gpio, uart, i2c, ...)
- **unit-address**: Địa chỉ (thường là base address), phải khớp với `reg`
- **Label**: Tên tham chiếu, dùng `&label` để trỏ tới

```dts
/* Node có label */
gpio1: gpio@4804c000 {
    compatible = "ti,omap4-gpio";
    reg = <0x4804c000 0x1000>;    /* base=0x4804C000, size=4KB */
    interrupts = <98>;
    gpio-controller;
    #gpio-cells = <2>;
};
```

### 4.3 Property (thuộc tính):

| Property | Ý nghĩa | Ví dụ |
|----------|---------|-------|
| `compatible` | Xác định driver phù hợp | `"ti,omap4-gpio"` |
| `reg` | Vùng nhớ (base + size) | `<0x4804c000 0x1000>` |
| `interrupts` | Số interrupt | `<98>` |
| `status` | Trạng thái node | `"okay"` hoặc `"disabled"` |
| `#address-cells` | Số cell cho địa chỉ child | `<1>` |
| `#size-cells` | Số cell cho size child | `<1>` |

### 4.4 Property `compatible` - rất quan trọng:

```dts
compatible = "ti,omap4-gpio";
```

Kernel dùng `compatible` để **tìm driver phù hợp**:

```c
/* Trong driver code */
static const struct of_device_id omap_gpio_match[] = {
    { .compatible = "ti,omap4-gpio" },  /* Khớp với DT! */
    { },
};
MODULE_DEVICE_TABLE(of, omap_gpio_match);
```

**Flow**: DTB chứa `compatible` → kernel so khớp với `of_match_table` của driver → gọi `probe()`.

### 4.5 Phandle - tham chiếu giữa các node:

```dts
/* Định nghĩa node với label */
gpio1: gpio@4804c000 {
    ...
};

/* Tham chiếu bằng phandle */
leds {
    led0 {
        gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;
        /*       ^^^^^^ phandle trỏ tới gpio1 */
    };
};
```

---

## 5. Device Tree của BeagleBone Black

### 5.1 File chính: `am335x-boneblack.dts`

```dts
/dts-v1/;

#include "am33xx.dtsi"
#include "am335x-bone-common.dtsi"

/ {
    model = "TI AM335x BeagleBone Black";
    compatible = "ti,am335x-bone-black", "ti,am335x-bone", "ti,am33xx";
};

/* Override eMMC */
&mmc2 {
    status = "okay";
    vmmc-supply = <&vmmcsd_fixed>;
    bus-width = <8>;
};

/* HDMI */
&am33xx_pinmux {
    nxp_hdmi_bonelt_pins: nxp_hdmi_bonelt_pins {
        pinctrl-names = "default";
        pinctrl-0 = <&nxp_hdmi_bonelt_pins>;
    };
};
```

### 5.2 SoC-level: `am33xx.dtsi` (trích)

```dts
/ {
    compatible = "ti,am33xx";

    cpus {
        cpu@0 {
            compatible = "arm,cortex-a8";
            clock-frequency = <1000000000>;
        };
    };

    ocp {    /* On-Chip Peripherals */
        compatible = "simple-bus";
        #address-cells = <1>;
        #size-cells = <1>;

        gpio0: gpio@44e07000 {
            compatible = "ti,omap4-gpio";
            reg = <0x44e07000 0x1000>;
            interrupts = <96>;
            gpio-controller;
            #gpio-cells = <2>;
        };

        gpio1: gpio@4804c000 {
            compatible = "ti,omap4-gpio";
            reg = <0x4804c000 0x1000>;
            interrupts = <98>;
            gpio-controller;
            #gpio-cells = <2>;
        };

        i2c0: i2c@44e0b000 {
            compatible = "ti,omap4-i2c";
            reg = <0x44e0b000 0x1000>;
            interrupts = <70>;
            #address-cells = <1>;
            #size-cells = <0>;
        };

        uart0: serial@44e09000 {
            compatible = "ti,am3352-uart", "ti,omap3-uart";
            reg = <0x44e09000 0x2000>;
            interrupts = <72>;
        };
    };
};
```

Chú ý: Mỗi node có `reg` khớp với memory map trong TRM.

Nguồn: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 2 - Memory Map

---

## 6. Biên dịch Device Tree

### 6.1 Dùng dtc (Device Tree Compiler):

```bash
# Compile DTS → DTB
dtc -I dts -O dtb -o output.dtb input.dts

# Decompile DTB → DTS (để đọc)
dtc -I dtb -O dts -o output.dts input.dtb
```

### 6.2 Build trong kernel tree:

```bash
cd ~/bbb-kernel.

# Build tất cả DTB cho AM335x
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- dtbs

# Output: arch/arm/boot/dts/am335x-boneblack.dtb
```

### 6.3 Xem DT đang chạy trên BBB:

```bash
# Trên BBB:
# Xem toàn bộ Device Tree đang dùng
dtc -I fs /sys/firmware/devicetree/base

# Xem từng node
ls /sys/firmware/devicetree/base/
cat /sys/firmware/devicetree/base/model

# Xem node GPIO1
ls /sys/firmware/devicetree/base/ocp/gpio@4804c000/
cat /sys/firmware/devicetree/base/ocp/gpio@4804c000/compatible
```

---

## 7. Viết Device Tree node cho peripheral

### 7.1 Ví dụ: Thêm node cho LED trên GPIO1_21

```dts
/ {
    /* Node mô tả LED */
    my_leds {
        compatible = "gpio-leds";
        pinctrl-names = "default";
        pinctrl-0 = <&my_led_pins>;

        my_led0 {
            label = "my-driver:led0";
            gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;
            default-state = "off";
        };
    };
};

/* Cấu hình pinmux cho LED */
&am33xx_pinmux {
    my_led_pins: my_led_pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_GPMC_A5, PIN_OUTPUT, MUX_MODE7)
        >;
    };
};
```

### 7.2 Ví dụ: Node cho I2C device (sensor)

```dts
&i2c2 {
    status = "okay";
    clock-frequency = <100000>;    /* 100 kHz */

    /* Temperature sensor TMP102 tại address 0x48 */
    tmp102@48 {
        compatible = "ti,tmp102";
        reg = <0x48>;
    };
};
```

---

## 8. Các property thường gặp trong DT cho AM335x

| Property | Ý nghĩa | Dùng ở đâu |
|----------|---------|-------------|
| `compatible` | Tên driver | Mọi node |
| `reg` | Base address + size | Peripheral node |
| `interrupts` | IRQ number | Node có interrupt |
| `clocks` | Clock source | Node cần clock |
| `pinctrl-0` | Pin configuration | Node dùng chân I/O |
| `gpios` | GPIO reference | Node dùng GPIO |
| `status` | "okay" / "disabled" | Enable/disable node |
| `#address-cells` | Address format cho child | Parent node |
| `#size-cells` | Size format cho child | Parent node |

---

## 9. Lỗi thường gặp khi viết DT

### 9.1 Thiếu `#address-cells` / `#size-cells`
```
Error: /ocp/mydevice: reg property requires #address-cells and #size-cells
```

### 9.2 `compatible` không khớp driver
→ Kernel không gọi `probe()`, device không hoạt động.
→ Kiểm tra bằng: `cat /sys/firmware/devicetree/base/ocp/mydevice/compatible`

### 9.3 `reg` sai address
→ Driver map vào vùng nhớ sai → crash hoặc không hoạt động.
→ Luôn kiểm tra TRM cho base address chính xác.

---

## 10. Kiến thức cốt lõi sau bài này

1. **Device Tree** tách mô tả phần cứng ra khỏi kernel code
2. **DTS** dùng cú pháp cây: node, property, phandle
3. **`compatible`** là property quan trọng nhất — kernel dùng nó để match driver
4. **`reg`** mô tả vùng nhớ (base address + size)
5. **DTSI** chứa phần chung (SoC-level), **DTS** chứa phần riêng (board-level)
6. **dtc** biên dịch DTS↔DTB, `/sys/firmware/devicetree/` để xem DT đang chạy

---

## 11. Câu hỏi kiểm tra

1. Device Tree giải quyết vấn đề gì mà trước đây kernel phải hardcode?
2. `compatible` property được dùng như thế nào trong quá trình probe driver?
3. Sự khác nhau giữa `.dts` và `.dtsi` là gì?
4. Làm sao xem Device Tree đang chạy trên BBB?
5. Viết một node DT cho GPIO module 2 (base `0x481AC000`, size `0x1000`, IRQ `32`).

---

## 12. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 5 - Linux Kernel Module cơ bản**

Ta sẽ viết kernel module đầu tiên: `module_init`, `module_exit`, `printk`, module parameters — nền tảng để viết mọi driver.
