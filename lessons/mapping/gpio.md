## 1. GPIO

### 1.1 GPIO Banks — Base Address & Clock

| Bank | Base Address | GPIO Range | Linux GPIO# | Clock Register | Clock Domain |
|------|-------------|-----------|------------|----------------|-------------|
| GPIO0 | `0x44E07000` | GPIO0_0 – GPIO0_31 | 0 – 31 | `CM_WKUP_GPIO0_CLKCTRL` @ `0x44E00408` | CM_WKUP |
| GPIO1 | `0x4804C000` | GPIO1_0 – GPIO1_31 | 32 – 63 | `CM_PER_GPIO1_CLKCTRL` @ `0x44E000AC` | CM_PER |
| GPIO2 | `0x481AC000` | GPIO2_0 – GPIO2_31 | 64 – 95 | `CM_PER_GPIO2_CLKCTRL` @ `0x44E000B0` | CM_PER |
| GPIO3 | `0x481AE000` | GPIO3_0 – GPIO3_31 | 96 – 127 | `CM_PER_GPIO3_CLKCTRL` @ `0x44E000B4` | CM_PER |

**Công thức tính Linux GPIO number:**
```
linux_gpio = (bank_number × 32) + bit_number
Ví dụ: GPIO1_21 → 1×32 + 21 = 53
```

### 1.2 GPIO Registers (offset từ base address)

| Register | Offset | Mô tả | Bit quan trọng |
|----------|--------|-------|---------------|
| `GPIO_REVISION` | `0x000` | IP revision | [31:0] version |
| `GPIO_SYSCONFIG` | `0x010` | Module config | [1] SOFTRESET, [2] ENAWAKEUP |
| `GPIO_SYSSTATUS` | `0x114` | Reset status | [0] RESETDONE |
| `GPIO_IRQSTATUS_RAW_0` | `0x024` | IRQ raw line 1 | [n] GPIO_n raw status |
| `GPIO_IRQSTATUS_0` | `0x02C` | IRQ status line 1 | [n] GPIO_n IRQ status |
| `GPIO_IRQSTATUS_SET_0` | `0x034` | Enable IRQ line 1 | [n]=1 enable |
| `GPIO_IRQSTATUS_CLR_0` | `0x03C` | Disable IRQ line 1 | [n]=1 disable |
| `GPIO_IRQWAKEN_0` | `0x044` | Wakeup enable | [n]=1 enable wakeup |
| `GPIO_SYSSTATUS` | `0x114` | Reset done | [0] RESETDONE |
| `GPIO_CTRL` | `0x130` | Clock gating | [0] DISABLEMODULE |
| `GPIO_OE` | `0x134` | Output enable | [n]=0 output, [n]=1 input |
| `GPIO_DATAIN` | `0x138` | Read pin state | [n] current pin value |
| `GPIO_DATAOUT` | `0x13C` | Output data | [n] output value |
| `GPIO_LEVELDETECT0` | `0x140` | Low-level detect | [n]=1 detect low |
| `GPIO_LEVELDETECT1` | `0x144` | High-level detect | [n]=1 detect high |
| `GPIO_RISINGDETECT` | `0x148` | Rising edge detect | [n]=1 detect rising |
| `GPIO_FALLINGDETECT` | `0x14C` | Falling edge detect | [n]=1 detect falling |
| `GPIO_DEBOUNCENABLE` | `0x150` | Debounce enable | [n]=1 enable debounce |
| `GPIO_DEBOUNCINGTIME` | `0x154` | Debounce time | [7:0] time × 31μs |
| `GPIO_CLEARDATAOUT` | `0x190` | Clear output (atomic) | [n]=1 clears GPIO_n output |
| `GPIO_SETDATAOUT` | `0x194` | Set output (atomic) | [n]=1 sets GPIO_n output |

> **Nguồn:** AM335x TRM `spruh73q.pdf`, Section 25.4 (GPIO Registers), trang ~4877

### 1.3 Onboard Components (không cần dây ngoài)

| Component | GPIO | Linux GPIO# | Chân board | Active | TRM / Schematic |
|-----------|------|------------|-----------|--------|----------------|
| USR LED0 | GPIO1_21 | **53** | Onboard | HIGH → sáng | BBB_SCH.pdf trang 3 |
| USR LED1 | GPIO1_22 | **54** | Onboard | HIGH → sáng | BBB_SCH.pdf trang 3 |
| USR LED2 | GPIO1_23 | **55** | Onboard | HIGH → sáng | BBB_SCH.pdf trang 3 |
| USR LED3 | GPIO1_24 | **56** | Onboard | HIGH → sáng | BBB_SCH.pdf trang 3 |
| USER BTN S2 | GPIO0_27 | **27** | Onboard | LOW khi nhấn | BBB_SCH.pdf trang 4 |
| POWER BTN | GPIO0_29 | **29** | Onboard | - | BBB_SCH.pdf |
| eMMC BOOT | GPIO2_2 | **66** | Onboard | - | Xung đột nếu dùng GPIO2 |

### 1.4 GPIO trên P8 Header (chọn lọc thường dùng)

| P8 Pin | GPIO | Linux GPIO# | Pad Name | Mức logic | Chú ý |
|--------|------|------------|---------|----------|-------|
| P8.3 | GPIO1_6 | 38 | gpmc_ad6 | 3.3V | eMMC nếu bật |
| P8.4 | GPIO1_7 | 39 | gpmc_ad7 | 3.3V | eMMC nếu bật |
| P8.7 | GPIO2_2 | 66 | gpmc_advn_ale | 3.3V | Tự do nếu không dùng eMMC |
| P8.8 | GPIO2_3 | 67 | gpmc_oen_ren | 3.3V | |
| P8.9 | GPIO2_5 | 69 | gpmc_be0n_cle | 3.3V | |
| P8.10 | GPIO2_4 | 68 | gpmc_wen | 3.3V | |
| P8.11 | GPIO1_13 | 45 | gpmc_ad13 | 3.3V | |
| P8.12 | GPIO1_12 | 44 | gpmc_ad12 | 3.3V | |
| P8.13 | GPIO0_23 | 23 | gpmc_ad9 | 3.3V | EHRPWM2B |
| P8.14 | GPIO0_26 | 26 | gpmc_ad10 | 3.3V | |
| P8.15 | GPIO1_15 | 47 | gpmc_ad15 | 3.3V | |
| P8.16 | GPIO1_14 | 46 | gpmc_ad14 | 3.3V | |
| P8.17 | GPIO0_27 | 27 | gpmc_ad11 | 3.3V | User Button onboard |
| P8.18 | GPIO2_1 | 65 | gpmc_clk | 3.3V | |
| P8.19 | GPIO0_22 | 22 | gpmc_ad8 | 3.3V | **EHRPWM2A** |
| P8.20 | GPIO1_31 | 63 | gpmc_csn2 | 3.3V | |
| P8.26 | GPIO1_29 | 61 | gpmc_csn0 | 3.3V | |
| P8.37 | GPIO2_14 | 78 | lcd_data8 | 3.3V | UART5_TXD, HDMI |
| P8.38 | GPIO2_15 | 79 | lcd_data9 | 3.3V | UART5_RXD, HDMI |
| P8.45 | GPIO2_6 | 70 | lcd_data0 | 3.3V | **EHRPWM2A alt** |
| P8.46 | GPIO2_7 | 71 | lcd_data1 | 3.3V | **EHRPWM2B alt** |

> **Nguồn:** `BeagleboneBlackP8HeaderTable.pdf`

### 1.5 GPIO trên P9 Header (chọn lọc thường dùng)

| P9 Pin | GPIO | Linux GPIO# | Pad Name | Mức logic | Chú ý |
|--------|------|------------|---------|----------|-------|
| P9.11 | GPIO0_30 | 30 | gpmc_wait0 | 3.3V | UART4_RXD |
| P9.12 | GPIO1_28 | **60** | gpmc_ben1 | 3.3V | **Test pin phổ biến** |
| P9.13 | GPIO0_31 | 31 | gpmc_wpn | 3.3V | UART4_TXD |
| P9.14 | GPIO1_18 | 50 | gpmc_a2 | 3.3V | **EHRPWM1A** |
| P9.15 | GPIO1_16 | **48** | gpmc_a0 | 3.3V | **Test pin phổ biến** |
| P9.16 | GPIO1_19 | 51 | gpmc_a3 | 3.3V | **EHRPWM1B** |
| P9.17 | GPIO0_5 | 5 | spi0_cs0 | 3.3V | SPI0_CS0, I2C1_SCL |
| P9.18 | GPIO0_4 | 4 | spi0_d1 | 3.3V | SPI0_MOSI, I2C1_SDA |
| P9.19 | GPIO0_13 | 13 | uart1_rtsn | 3.3V | **I2C2_SCL** |
| P9.20 | GPIO0_12 | 12 | uart1_ctsn | 3.3V | **I2C2_SDA** |
| P9.21 | GPIO0_3 | 3 | spi0_d0 | 3.3V | **SPI0_MISO**, UART2_TXD |
| P9.22 | GPIO0_2 | 2 | spi0_sclk | 3.3V | **SPI0_CLK**, UART2_RXD |
| P9.23 | GPIO1_17 | 49 | gpmc_a1 | 3.3V | |
| P9.24 | GPIO0_15 | 15 | uart1_txd | 3.3V | **UART1_TXD** |
| P9.25 | GPIO3_21 | 117 | mcasp0_ahclkx | 3.3V | MCASP0 / PRU |
| P9.26 | GPIO0_14 | 14 | uart1_rxd | 3.3V | **UART1_RXD** |
| P9.27 | GPIO3_19 | 115 | mcasp0_fsr | 3.3V | |
| P9.28 | GPIO3_17 | 113 | mcasp0_axr2 | 3.3V | SPI1_CS0 |
| P9.29 | GPIO3_15 | 111 | mcasp0_fsx | 3.3V | **SPI1_MISO** |
| P9.30 | GPIO3_16 | 112 | mcasp0_axr0 | 3.3V | **SPI1_MOSI** |
| P9.31 | GPIO3_14 | 110 | mcasp0_aclkx | 3.3V | **SPI1_CLK** |
| P9.33 | AIN4 | - | - | **1.8V MAX** | **ADC channel 4** |
| P9.35 | AIN6 | - | - | **1.8V MAX** | **ADC channel 6** |
| P9.36 | AIN5 | - | - | **1.8V MAX** | **ADC channel 5** |
| P9.37 | AIN2 | - | - | **1.8V MAX** | **ADC channel 2** |
| P9.38 | AIN3 | - | - | **1.8V MAX** | **ADC channel 3** |
| P9.39 | AIN0 | - | - | **1.8V MAX** | **ADC channel 0** |
| P9.40 | AIN1 | - | - | **1.8V MAX** | **ADC channel 1** |

> **Nguồn:** `BeagleboneBlackP9HeaderTable.pdf`

---
---

> **Xem thêm:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)


---

> **Xem them:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)

