## 2. Control Module — Pinmux

### 2.1 Control Module Base

| Module | Base Address | Mô tả |
|--------|-------------|-------|
| Control Module | `0x44E10000` | Pad mux, SoC identity, boot config |

### 2.2 Pad Configuration Register Format

Mỗi pad có 1 thanh ghi `conf_<padname>` 32 bit, offset từ `0x44E10000`:

```
Bit 31-8 : Reserved
Bit 6    : SLEWCtrl  - 0=fast, 1=slow
Bit 5    : RxActive  - 0=output only, 1=input enabled (BẮT BUỘC khi dùng làm input/I2C/SPI)
Bit 4    : PullUDen  - 0=pull enabled, 1=pull disabled
Bit 3    : PullTYPE  - 0=pull-down, 1=pull-up
Bit 2-0  : MMODE     - 0-7, mode/function selection (xem bảng pad)
```

### 2.3 Pad Offsets Thường Dùng

| Pad | Offset | P8/P9 Pin | Chức năng mode 7 |
|-----|--------|-----------|-----------------|
| `conf_gpmc_ad0` | `0x800` | P8.45 (via LCD) | GPIO2_6 |
| `conf_gpmc_a0` | `0x840` | P9.15 | GPIO1_16 |
| `conf_gpmc_a2` | `0x848` | P9.14 | **EHRPWM1A (mode6)** / GPIO1_18 |
| `conf_gpmc_a3` | `0x84C` | P9.16 | **EHRPWM1B (mode6)** / GPIO1_19 |
| `conf_gpmc_ben1` | `0x878` | P9.12 | GPIO1_28 |
| `conf_gpmc_clk` | `0x88C` | P8.18 | GPIO2_1 |
| `conf_gpmc_ad8` | `0x820` | P8.19 | **EHRPWM2A (mode4)** / GPIO0_22 |
| `conf_gpmc_ad9` | `0x824` | P8.13 | **EHRPWM2B (mode4)** / GPIO0_23 |
| `conf_gpmc_ad10` | `0x828` | P8.14 | GPIO0_26 |
| `conf_gpmc_ad11` | `0x82C` | P8.17 | GPIO0_27 |
| `conf_gpmc_ad12` | `0x830` | P8.12 | GPIO1_12 |
| `conf_gpmc_ad13` | `0x834` | P8.11 | GPIO1_13 |
| `conf_spi0_sclk` | `0x950` | P9.22 | **SPI0_CLK (mode0)** |
| `conf_spi0_d0` | `0x954` | P9.21 | **SPI0_MISO (mode0)** |
| `conf_spi0_d1` | `0x958` | P9.18 | **SPI0_MOSI (mode0)** |
| `conf_spi0_cs0` | `0x95C` | P9.17 | **SPI0_CS0 (mode0)** |
| `conf_uart1_rxd` | `0x970` | P9.26 | **UART1_RXD (mode0)** |
| `conf_uart1_txd` | `0x974` | P9.24 | **UART1_TXD (mode0)** |
| `conf_uart1_ctsn` | `0x978` | P9.20 | **I2C2_SDA (mode3)** |
| `conf_uart1_rtsn` | `0x97C` | P9.19 | **I2C2_SCL (mode3)** |
| `conf_i2c0_sda` | `0x988` | - | I2C0 SDA (onboard) |
| `conf_i2c0_scl` | `0x98C` | - | I2C0 SCL (onboard) |
| `conf_mcasp0_aclkx` | `0x990` | P9.31 | **SPI1_CLK (mode3)** |
| `conf_mcasp0_fsx` | `0x994` | P9.29 | **SPI1_MISO (mode3)** |
| `conf_mcasp0_axr0` | `0x998` | P9.30 | **SPI1_MOSI (mode3)** |
| `conf_mcasp0_axr2` | `0x99C` | P9.28 | **SPI1_CS0 (mode3)** |
| `conf_gpmc_a8` | `0x860` | P9.14 via | EHRPWM1A |
| `conf_gpmc_csn0` | `0x874` | P8.26 | GPIO1_29 |
| `conf_gpmc_csn1` | `0x878` | P8.21 | GPIO1_30 |
| `conf_lcd_data8` | `0x8E8` | P8.37 | UART5_TXD (mode4) |
| `conf_lcd_data9` | `0x8EC` | P8.38 | UART5_RXD (mode4) |
| `conf_uart0_rxd` | `0x970` | J1 | UART0 debug |
| `conf_uart0_txd` | `0x974` | J1 | UART0 debug |

> **Nguồn:** AM335x TRM `spruh73q.pdf`, Section 9.3 (CONTROL_MODULE registers), trang ~1275

### 2.4 Ví dụ Pad Config Values Thường Dùng

```c
/* GPIO input với pull-up, fast slew */
#define PIN_GPIO_INPUT_PULLUP   (0 << 6 | 1 << 5 | 0 << 4 | 1 << 3 | 7)
/* Value: 0b0_1_0_1_111 = 0x37 */

/* GPIO output, no pull, fast slew */
#define PIN_GPIO_OUTPUT_NOPULL  (0 << 6 | 0 << 5 | 1 << 4 | 0 << 3 | 7)
/* Value: 0b0_0_1_0_111 = 0x17 */

/* I2C (open-drain) pin: mode3, input, pull-up */
#define PIN_I2C                 (0 << 6 | 1 << 5 | 0 << 4 | 1 << 3 | 3)
/* Value: 0b0_1_0_1_011 = 0x33 */

/* SPI CLK: mode0, no pull, fast slew */
#define PIN_SPI_CLK             (0 << 6 | 0 << 5 | 1 << 4 | 0 << 3 | 0)
/* Value: 0x10 */

/* UART TX: mode0, no pull */
#define PIN_UART_TX             (0 << 6 | 0 << 5 | 1 << 4 | 0 << 3 | 0)
/* Value: 0x10 */

/* UART RX: mode0, input, pull-up */
#define PIN_UART_RX             (0 << 6 | 1 << 5 | 0 << 4 | 1 << 3 | 0)
/* Value: 0x30 */
```

---
---

> **Xem thêm:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)


---

> **Xem them:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)

