## 14. Device Tree Snippets

### 14.1 GPIO — USR LED0 và User Button

```dts
/* Trong am335x-boneblack.dts hoặc overlay */
&am33xx_pinmux {
    usr_leds_pins: usr-leds-pins {
        pinctrl-single,pins = <
            /* GPIO1_21 Mode7 output nopull */
            AM33XX_CONF(CONF_GPMC_A5,  MUX_MODE7)
            /* GPIO1_22, 23, 24 */
            AM33XX_CONF(CONF_GPMC_A6,  MUX_MODE7)
            AM33XX_CONF(CONF_GPMC_A7,  MUX_MODE7)
            AM33XX_CONF(CONF_GPMC_A8,  MUX_MODE7)
        >;
    };
    user_btn_pins: user-btn-pins {
        pinctrl-single,pins = <
            /* GPIO0_27 input pullup */
            AM33XX_IOPAD(0x82c, PIN_INPUT_PULLUP | MUX_MODE7)
        >;
    };
};

leds {
    compatible = "gpio-leds";
    pinctrl-names = "default";
    pinctrl-0 = <&usr_leds_pins>;

    led0 {
        label = "beaglebone:green:usr0";
        gpios = <&gpio1 21 GPIO_ACTIVE_HIGH>;
        linux,default-trigger = "heartbeat";
    };
    led1 {
        label = "beaglebone:green:usr1";
        gpios = <&gpio1 22 GPIO_ACTIVE_HIGH>;
        linux,default-trigger = "mmc0";
    };
};

keys {
    compatible = "gpio-keys";
    pinctrl-names = "default";
    pinctrl-0 = <&user_btn_pins>;

    user-button {
        label = "User Button";
        gpios = <&gpio0 27 GPIO_ACTIVE_LOW>;
        linux,code = <KEY_PROG1>;
    };
};
```

### 14.2 I2C2 — TMP102 Sensor

```dts
&am33xx_pinmux {
    i2c2_pins: i2c2-pins {
        pinctrl-single,pins = <
            /* P9.19 I2C2_SCL: conf_uart1_rtsn, mode3, input, pullup */
            AM33XX_IOPAD(0x97c, PIN_INPUT_PULLUP | MUX_MODE3)
            /* P9.20 I2C2_SDA: conf_uart1_ctsn, mode3, input, pullup */
            AM33XX_IOPAD(0x978, PIN_INPUT_PULLUP | MUX_MODE3)
        >;
    };
};

&i2c2 {
    /* I2C2 base: 0x4819C000, IRQ 30 */
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&i2c2_pins>;
    clock-frequency = <100000>;  /* 100kHz */

    tmp102@48 {
        compatible = "ti,tmp102";
        reg = <0x48>;  /* ADDR pin → GND */
    };
};
```

### 14.3 SPI0 — MCP3008

```dts
&am33xx_pinmux {
    spi0_pins: spi0-pins {
        pinctrl-single,pins = <
            /* P9.22 SPI0_CLK  mode0 nopull */
            AM33XX_IOPAD(0x950, PIN_INPUT_PULLUP | MUX_MODE0)
            /* P9.21 SPI0_MISO mode0 input */
            AM33XX_IOPAD(0x954, PIN_INPUT_PULLUP | MUX_MODE0)
            /* P9.18 SPI0_MOSI mode0 output */
            AM33XX_IOPAD(0x958, PIN_OUTPUT_PULLUP | MUX_MODE0)
            /* P9.17 SPI0_CS0  mode0 output */
            AM33XX_IOPAD(0x95c, PIN_OUTPUT_PULLUP | MUX_MODE0)
        >;
    };
};

&spi0 {
    /* SPI0 base: 0x48030000, IRQ 65 */
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&spi0_pins>;
    ti,pindir-d0-out-d1-in;

    mcp3008@0 {
        compatible = "microchip,mcp3008";
        reg = <0>;           /* CS0 */
        spi-max-frequency = <1000000>;   /* 1MHz */
    };
};
```

### 14.4 UART1 — Serial Port

```dts
&am33xx_pinmux {
    uart1_pins: uart1-pins {
        pinctrl-single,pins = <
            /* P9.24 UART1_TXD mode0 output nopull */
            AM33XX_IOPAD(0x984, PIN_OUTPUT | MUX_MODE0)
            /* P9.26 UART1_RXD mode0 input pullup */
            AM33XX_IOPAD(0x980, PIN_INPUT_PULLUP | MUX_MODE0)
        >;
    };
};

&uart1 {
    /* UART1 base: 0x48022000, IRQ 73 */
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&uart1_pins>;
};
```

### 14.5 EHRPWM1 — P9.14/P9.16

```dts
&am33xx_pinmux {
    ehrpwm1_pins: ehrpwm1-pins {
        pinctrl-single,pins = <
            /* P9.14 EHRPWM1A: conf_gpmc_a2, mode6 */
            AM33XX_IOPAD(0x848, PIN_OUTPUT | MUX_MODE6)
            /* P9.16 EHRPWM1B: conf_gpmc_a3, mode6 */
            AM33XX_IOPAD(0x84c, PIN_OUTPUT | MUX_MODE6)
        >;
    };
};

&epwmss1 {
    /* EHRPWM1 subsystem base: 0x48302000 */
    status = "okay";
};

&ehrpwm1 {
    /* EHRPWM1 PWM base: 0x48302200 */
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&ehrpwm1_pins>;
};
```

### 14.6 TSC_ADC — AIN0

```dts
&tscadc {
    /* TSC_ADC base: 0x44E0D000, IRQ 16 */
    status = "okay";

    adc {
        ti,adc-channels = <0 1 2 3 4 5 6>;  /* AIN0-AIN6 */
    };
};
```

### 14.7 DMTIMER2 — Hardware Timer

```dts
&timer2 {
    /* DMTIMER2 base: 0x48040000, IRQ 68 */
    status = "okay";
    /* clock source: CLK_M_OSC = 24MHz or CLK_32KHZ */
    ti,timer-alwon;
};
```

---