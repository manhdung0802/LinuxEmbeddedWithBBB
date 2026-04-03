# Bài 16 - Pin Control Subsystem

## 1. Mục tiêu bài học
- Hiểu pinctrl subsystem trong Linux kernel
- Nắm pinmux (chọn chức năng pin) vs pinconf (cấu hình pin: pull-up/down, drive strength)
- Đọc hiểu pinctrl DT bindings trên AM335x
- Viết DT overlay cấu hình pin cho driver
- Hiểu AM335x Control Module pad config registers

---

## 2. Pinctrl Architecture

### 2.1 Vấn đề:

Mỗi chân SoC có thể mang nhiều chức năng (GPIO, UART, I2C, SPI, ...). Cần cơ chế:
- **Pinmux**: Chọn chức năng cho pin
- **Pinconf**: Cấu hình điện (pull-up, pull-down, drive strength, slew rate)

### 2.2 Kiến trúc:

```
┌─────────────────────────────────────────────┐
│  Driver (I2C, SPI, GPIO, ...)               │
│  pinctrl-names = "default";                 │
│  pinctrl-0 = <&my_pins>;                   │
└──────────────────┬──────────────────────────┘
                   │ request pin state
┌──────────────────┴──────────────────────────┐
│  Pinctrl Core (drivers/pinctrl/)            │
│  - Quản lý pin groups                       │
│  - Resolve pin conflicts                    │
└──────────────────┬──────────────────────────┘
                   │ configure hardware
┌──────────────────┴──────────────────────────┐
│  Pinctrl Driver (pinctrl-single.c)          │
│  - AM335x: ghi vào Control Module registers │
│  - Base: 0x44E10000                         │
└─────────────────────────────────────────────┘
```

---

## 3. AM335x Pad Configuration

### 3.1 Control Module registers:

Mỗi pad (chân) có 1 thanh ghi 32-bit trong Control Module:

```
Control Module base: 0x44E10000

Pad register format (32-bit):
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ 31 │ .. │  6 │  5 │  4 │  3 │  2 │ 1:0│
├────┼────┼────┼────┼────┼────┼────┼────┤
│ reserved │slewctrl│rxactive│pulltypesel│pulluden│mmode    │
└────┴────┴────┴────┴────┴────┴────┴────┘

Bit [2:0] mmode:     Mode select (0-7)
          0 = primary function
          7 = GPIO mode
Bit [3]   pulluden:   0=pull enabled, 1=pull disabled
Bit [4]   pulltypesel: 0=pulldown, 1=pullup
Bit [5]   rxactive:   0=receiver disabled, 1=receiver enabled (cần cho input)
Bit [6]   slewctrl:   0=fast, 1=slow
```

Nguồn: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 9 - Control Module, Pad Configuration

### 3.2 Ví dụ pad config values:

| Cấu hình | Value | Ý nghĩa |
|-----------|-------|---------|
| GPIO output | `0x07` | mode7, pull disabled, RX off |
| GPIO input pull-up | `0x37` | mode7, pull-up, RX on |
| GPIO input pull-down | `0x27` | mode7, pull-down, RX on |
| I2C (mode 2) | `0x32` | mode2, pull-up, RX on |
| UART TX | `0x00` | mode0, pull disabled, RX off |
| UART RX | `0x20` | mode0, RX on |

---

## 4. Pinctrl trong Device Tree

### 4.1 Pinctrl driver cho AM335x:

```dts
/* Trong am33xx.dtsi */
am33xx_pinmux: pinmux@44e10800 {
    compatible = "pinctrl-single";
    reg = <0x44e10800 0x0238>;  /* Pad config register area */
    #pinctrl-cells = <1>;
    pinctrl-single,register-width = <32>;
    pinctrl-single,function-mask = <0x7f>;
};
```

### 4.2 Định nghĩa pin group:

```dts
&am33xx_pinmux {
    /* Nhóm pin cho I2C2 */
    i2c2_pins: i2c2_pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_UART1_CTSN, PIN_INPUT_PULLUP, MUX_MODE3)
            AM33XX_PADCONF(AM335X_PIN_UART1_RTSN, PIN_INPUT_PULLUP, MUX_MODE3)
        >;
    };

    /* Nhóm pin cho GPIO LEDs */
    led_pins: led_pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_GPMC_A5, PIN_OUTPUT, MUX_MODE7)
            AM33XX_PADCONF(AM335X_PIN_GPMC_A6, PIN_OUTPUT, MUX_MODE7)
        >;
    };

    /* Nhóm pin cho SPI0 */
    spi0_pins: spi0_pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_SPI0_SCLK, PIN_INPUT_PULLUP, MUX_MODE0)
            AM33XX_PADCONF(AM335X_PIN_SPI0_D0, PIN_INPUT_PULLUP, MUX_MODE0)
            AM33XX_PADCONF(AM335X_PIN_SPI0_D1, PIN_OUTPUT, MUX_MODE0)
            AM33XX_PADCONF(AM335X_PIN_SPI0_CS0, PIN_OUTPUT, MUX_MODE0)
        >;
    };
};
```

### 4.3 Consumer dùng pinctrl:

```dts
&i2c2 {
    status = "okay";
    pinctrl-names = "default";     /* Tên state */
    pinctrl-0 = <&i2c2_pins>;      /* Pin group cho state "default" */
    clock-frequency = <100000>;
};
```

### 4.4 Multiple pin states:

```dts
my_device {
    pinctrl-names = "default", "sleep";
    pinctrl-0 = <&my_pins_active>;   /* State "default" */
    pinctrl-1 = <&my_pins_sleep>;    /* State "sleep" */
};
```

Driver chuyển state:
```c
struct pinctrl *pctrl;
struct pinctrl_state *active, *sleep;

pctrl = devm_pinctrl_get(&pdev->dev);
active = pinctrl_lookup_state(pctrl, "default");
sleep = pinctrl_lookup_state(pctrl, "sleep");

pinctrl_select_state(pctrl, active);   /* Chuyển sang active */
pinctrl_select_state(pctrl, sleep);    /* Chuyển sang sleep */
```

---

## 5. Tra cứu Pin Mapping

### 5.1 Từ P8/P9 header → pad register:

1. Tra bảng P8/P9 header: `BBB_docs/datasheets/BeagleboneBlackP8HeaderTable.pdf`
2. Tìm "PROC BALL" hoặc "Pin Name" → đó là tên pad trên SoC
3. Tra TRM → tìm offset của `conf_<pin_name>` register

**Ví dụ**: P8.3 → GPMC_AD6 → `conf_gpmc_ad6` → offset `0x818`

### 5.2 Trong DT:

```dts
/* P8.3 → GPIO1_6, mode 7 (GPIO), output */
AM33XX_PADCONF(AM335X_PIN_GPMC_AD6, PIN_OUTPUT, MUX_MODE7)
/* Tương đương: offset 0x818, value 0x07 */
```

---

## 6. Xem Pinctrl từ Debugfs

```bash
# Xem tất cả pin groups
cat /sys/kernel/debug/pinctrl/44e10800.pinmux/pingroups

# Xem pin config
cat /sys/kernel/debug/pinctrl/44e10800.pinmux/pins

# Xem pinmux mapping
cat /sys/kernel/debug/pinctrl/44e10800.pinmux/pinmux-pins
```

---

## 7. Kiến thức cốt lõi sau bài này

1. **Pinctrl subsystem** quản lý pinmux + pinconf cho tất cả pin trên SoC
2. **AM335x** dùng `pinctrl-single` driver, ghi vào Control Module registers
3. **Pad register** 32-bit: mode (bit 0-2), pull (bit 3-4), RX (bit 5), slew (bit 6)
4. **DT binding**: `pinctrl-names` + `pinctrl-0..N` → driver tự áp dụng "default" khi probe
5. **Multiple states**: driver có thể chuyển pin state runtime (active/sleep)

---

## 8. Câu hỏi kiểm tra

1. Pinmux và pinconf khác nhau ở điểm nào?
2. Bit [2:0] trong pad register AM335x dùng để làm gì?
3. Tại sao input pin cần `rxactive = 1`?
4. Viết DT pinctrl cho UART1 TX/RX trên P9 header.
5. Làm sao kiểm tra cấu hình pin hiện tại trên BBB?

---

## 9. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 17 - Input Subsystem**

Ta sẽ học input framework: `input_dev`, `input_report_key`, viết button driver với interrupt và debounce.
