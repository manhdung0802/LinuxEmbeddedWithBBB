## 4. I2C

### 4.1 I2C Buses — Base Address & Pins

| Bus | Base Address | IRQ | P9 SCL | P9 SDA | Pad SCL | Pad SDA | Dùng cho |
|-----|-------------|-----|--------|--------|---------|---------|---------|
| I2C0 | `0x44E0B000` | 70 | - | - | `conf_i2c0_scl` | `conf_i2c0_sda` | PMIC TPS65217 — **KHÔNG DÙNG** |
| I2C1 | `0x4802A000` | 71 | P9.17 | P9.18 | `conf_spi0_cs0` | `conf_spi0_d1` | Cape/HDMI — cẩn thận |
| **I2C2** | **`0x4819C000`** | **30** | **P9.19** | **P9.20** | `conf_uart1_rtsn` | `conf_uart1_ctsn` | **Dùng cho học** |

### 4.2 I2C Registers

| Register | Offset | Mô tả |
|----------|--------|-------|
| `I2C_REVNB_LO` | `0x00` | Revision low |
| `I2C_REVNB_HI` | `0x04` | Revision high |
| `I2C_SYSC` | `0x10` | System config (clocks, iDLE) |
| `I2C_IRQSTATUS_RAW` | `0x24` | Raw IRQ status |
| `I2C_IRQSTATUS` | `0x28` | Enabled IRQ status |
| `I2C_IRQENABLE_SET` | `0x2C` | Enable IRQs |
| `I2C_IRQENABLE_CLR` | `0x30` | Disable IRQs |
| `I2C_WE` | `0x34` | Wakeup enable |
| `I2C_SYSS` | `0x90` | System status (RDONE) |
| `I2C_BUF` | `0x94` | Buffer config (FIFO, DMA) |
| `I2C_CNT` | `0x98` | Data count |
| `I2C_DATA` | `0x9C` | TX/RX data byte |
| `I2C_CON` | `0xA4` | Control (master/slave, START, STOP) |
| `I2C_OA` | `0xA8` | Own address |
| `I2C_SA` | `0xAC` | Slave address target |
| `I2C_PSC` | `0xB0` | Clock prescaler |
| `I2C_SCLL` | `0xB4` | SCL low time |
| `I2C_SCLH` | `0xB8` | SCL high time |
| `I2C_SYSTEST` | `0xBC` | Test mode |
| `I2C_BUFSTAT` | `0xC0` | Buffer status |

**Tính toán tốc độ I2C (100kHz) với functional clock 48MHz:**
```
PSC  = 3  → internal clock = 48MHz / (PSC+1) = 12MHz
SCLL = 53 → SCL low  = (SCLL + 7) / 12MHz = 5μs
SCLH = 55 → SCL high = (SCLH + 5) / 12MHz = 5μs → 100kHz
```

> **Nguồn:** AM335x TRM `spruh73q.pdf`, Section 21 (I2C), trang ~4250

### 4.3 Thiết bị I2C thực hành trên BBB

| Cảm biến | I2C Address | Chức năng | Kết nối |
|----------|------------|----------|---------|
| TMP102 | `0x48` (ADDR=GND) | Temperature | I2C2 P9.19/P9.20 |
| MPU6050 | `0x68` (AD0=GND) | IMU (Accel+Gyro) | I2C2 P9.19/P9.20 |
| DS3231 | `0x68` | RTC | I2C2 |
| OLED SSD1306 | `0x3C` | 128×64 display | I2C2 |
| ADS1115 | `0x48` | 16-bit ADC | I2C2 |

---
---

> **Xem thêm:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)


---

> **Xem them:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)

