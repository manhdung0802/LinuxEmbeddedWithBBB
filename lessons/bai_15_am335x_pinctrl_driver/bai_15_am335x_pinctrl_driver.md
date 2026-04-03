# B�i 15 - AM335x Pinctrl & Pin Control Subsystem

> **Tham chi?u ph?n c?ng BBB:** Xem [pinmux.md](../mapping/pinmux.md)

## 0. BBB Connection � Control Module & Pin Muxing

```
AM335x Control Module @ 0x44E10000
-----------------------------------
 conf_xxx registers: offset 0x800 ... 0x9FF

 Format m?i pad register (32 bits, bits [6:0] d�ng):
 +--------------------------------------------------------+
 � Bit�    6     �   5    �    4     �    3     �  2:0    �
 �    � SLEWCTRL �RXACTIVE� PULLUDEN � PULLTYPE � MMODE   �
 �    � 0=fast   �1=enable�0=enable  �1=pullup  � 0-7     �
 �    � 1=slow   �0=disable�1=disable�0=pulldown�         �
 +--------------------------------------------------------+
```

### C�c pad thu?ng d�ng tr�n BBB

| BBB Pin | Pad Name | Offset | Mode 0 | Mode 7 (GPIO) |
|---------|----------|--------|--------|----------------|
| P9.14 | conf_gpmc_a2 | 0x848 | GPMC_A2 | GPIO1_18 |
| P9.16 | conf_gpmc_a3 | 0x84C | GPMC_A3 | GPIO1_19 |
| P9.19 | conf_i2c2_scl | 0x97C | I2C2_SCL | GPIO0_13 |
| P9.20 | conf_i2c2_sda | 0x978 | I2C2_SDA | GPIO0_12 |
| P9.24 | conf_uart1_txd | 0x984 | UART1_TX | GPIO0_15 |
| P9.26 | conf_uart1_rxd | 0x980 | UART1_RX | GPIO0_14 |

> **C?nh b�o pin conflicts:** eMMC d�ng P8.03-P8.06, P8.20-P8.25; HDMI d�ng P8.27-P8.46.

---

## 1. M?c ti�u b�i h?c
- Hi?u pinctrl subsystem trong Linux kernel
- N?m pinmux (ch?n ch?c nang pin) vs pinconf (c?u h�nh pin: pull-up/down, drive strength)
- �?c hi?u pinctrl DT bindings tr�n AM335x
- Vi?t DT overlay c?u h�nh pin cho driver
- Hi?u AM335x Control Module pad config registers

---

## 2. Pinctrl Architecture

### 2.1 V?n d?:

M?i ch�n SoC c� th? mang nhi?u ch?c nang (GPIO, UART, I2C, SPI, ...). C?n co ch?:
- **Pinmux**: Ch?n ch?c nang cho pin
- **Pinconf**: C?u h�nh di?n (pull-up, pull-down, drive strength, slew rate)

### 2.2 Ki?n tr�c:

```
+---------------------------------------------+
�  Driver (I2C, SPI, GPIO, ...)               �
�  pinctrl-names = "default";                 �
�  pinctrl-0 = <&my_pins>;                   �
+---------------------------------------------+
                   � request pin state
+---------------------------------------------+
�  Pinctrl Core (drivers/pinctrl/)            �
�  - Qu?n l� pin groups                       �
�  - Resolve pin conflicts                    �
+---------------------------------------------+
                   � configure hardware
+---------------------------------------------+
�  Pinctrl Driver (pinctrl-single.c)          �
�  - AM335x: ghi v�o Control Module registers �
�  - Base: 0x44E10000                         �
+---------------------------------------------+
```

---

## 3. AM335x Pad Configuration

### 3.1 Control Module registers:

M?i pad (ch�n) c� 1 thanh ghi 32-bit trong Control Module:

```
Control Module base: 0x44E10000

Pad register format (32-bit):
+---------------------------------------+
� 31 � .. �  6 �  5 �  4 �  3 �  2 � 1:0�
+----+----+----+----+----+----+----+----�
� reserved �slewctrl�rxactive�pulltypesel�pulluden�mmode    �
+---------------------------------------+

Bit [2:0] mmode:     Mode select (0-7)
          0 = primary function
          7 = GPIO mode
Bit [3]   pulluden:   0=pull enabled, 1=pull disabled
Bit [4]   pulltypesel: 0=pulldown, 1=pullup
Bit [5]   rxactive:   0=receiver disabled, 1=receiver enabled (c?n cho input)
Bit [6]   slewctrl:   0=fast, 1=slow
```

Ngu?n: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 9 - Control Module, Pad Configuration

### 3.2 V� d? pad config values:

| C?u h�nh | Value | � nghia |
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

### 4.2 �?nh nghia pin group:

```dts
&am33xx_pinmux {
    /* Nh�m pin cho I2C2 */
    i2c2_pins: i2c2_pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_UART1_CTSN, PIN_INPUT_PULLUP, MUX_MODE3)
            AM33XX_PADCONF(AM335X_PIN_UART1_RTSN, PIN_INPUT_PULLUP, MUX_MODE3)
        >;
    };

    /* Nh�m pin cho GPIO LEDs */
    led_pins: led_pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_GPMC_A5, PIN_OUTPUT, MUX_MODE7)
            AM33XX_PADCONF(AM335X_PIN_GPMC_A6, PIN_OUTPUT, MUX_MODE7)
        >;
    };

    /* Nh�m pin cho SPI0 */
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

### 4.3 Consumer d�ng pinctrl:

```dts
&i2c2 {
    status = "okay";
    pinctrl-names = "default";     /* T�n state */
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

Driver chuy?n state:
```c
struct pinctrl *pctrl;
struct pinctrl_state *active, *sleep;

pctrl = devm_pinctrl_get(&pdev->dev);
active = pinctrl_lookup_state(pctrl, "default");
sleep = pinctrl_lookup_state(pctrl, "sleep");

pinctrl_select_state(pctrl, active);   /* Chuy?n sang active */
pinctrl_select_state(pctrl, sleep);    /* Chuy?n sang sleep */
```

---

## 5. Tra c?u Pin Mapping

### 5.1 T? P8/P9 header ? pad register:

1. Tra b?ng P8/P9 header: `BBB_docs/datasheets/BeagleboneBlackP8HeaderTable.pdf`
2. T�m "PROC BALL" ho?c "Pin Name" ? d� l� t�n pad tr�n SoC
3. Tra TRM ? t�m offset c?a `conf_<pin_name>` register

**V� d?**: P8.3 ? GPMC_AD6 ? `conf_gpmc_ad6` ? offset `0x818`

### 5.2 Trong DT:

```dts
/* P8.3 ? GPIO1_6, mode 7 (GPIO), output */
AM33XX_PADCONF(AM335X_PIN_GPMC_AD6, PIN_OUTPUT, MUX_MODE7)
/* Tuong duong: offset 0x818, value 0x07 */
```

---

## 6. Xem Pinctrl t? Debugfs

```bash
# Xem t?t c? pin groups
cat /sys/kernel/debug/pinctrl/44e10800.pinmux/pingroups

# Xem pin config
cat /sys/kernel/debug/pinctrl/44e10800.pinmux/pins

# Xem pinmux mapping
cat /sys/kernel/debug/pinctrl/44e10800.pinmux/pinmux-pins
```

---

## 7. Ki?n th?c c?t l�i sau b�i n�y

1. **Pinctrl subsystem** qu?n l� pinmux + pinconf cho t?t c? pin tr�n SoC
2. **AM335x** d�ng `pinctrl-single` driver, ghi v�o Control Module registers
3. **Pad register** 32-bit: mode (bit 0-2), pull (bit 3-4), RX (bit 5), slew (bit 6)
4. **DT binding**: `pinctrl-names` + `pinctrl-0..N` ? driver t? �p d?ng "default" khi probe
5. **Multiple states**: driver c� th? chuy?n pin state runtime (active/sleep)

---

## 8. C�u h?i ki?m tra

1. Pinmux v� pinconf kh�c nhau ? di?m n�o?
2. Bit [2:0] trong pad register AM335x d�ng d? l�m g�?
3. T?i sao input pin c?n `rxactive = 1`?
4. Vi?t DT pinctrl cho UART1 TX/RX tr�n P9 header.
5. L�m sao ki?m tra c?u h�nh pin hi?n t?i tr�n BBB?

---

## 9. Chu?n b? cho b�i sau

B�i ti?p theo: **B�i 17 - Input Subsystem**

Ta s? h?c input framework: `input_dev`, `input_report_key`, vi?t button driver v?i interrupt v� debounce.
