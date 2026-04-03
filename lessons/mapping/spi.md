## 5. SPI (McSPI)

### 5.1 SPI Buses — Base Address & Pins

| Bus | Base Address | IRQ | CLK | MISO | MOSI | CS0 | CS1 | Dùng cho |
|-----|-------------|-----|-----|------|------|-----|-----|---------|
| **SPI0** | **`0x48030000`** | **65** | **P9.22** | **P9.21** | **P9.18** | **P9.17** | - | **Dùng cho học** |
| SPI1 | `0x481A0000` | 125 | P9.31 | P9.29 | P9.30 | P9.28 | P9.42 | Cẩn thận: HDMI |

### 5.2 McSPI Registers (per-channel, ch=0)

| Register | Offset | Mô tả |
|----------|--------|-------|
| `MCSPI_REVISION` | `0x000` | IP revision |
| `MCSPI_SYSCONFIG` | `0x110` | System config |
| `MCSPI_SYSSTATUS` | `0x114` | Reset done |
| `MCSPI_IRQSTATUS` | `0x118` | IRQ status |
| `MCSPI_IRQENABLE` | `0x11C` | IRQ enable |
| `MCSPI_SYST` | `0x124` | System test |
| `MCSPI_MODULCTRL` | `0x128` | Master/slave mode |
| `MCSPI_CH0CONF` | `0x12C` | Channel 0 config |
| `MCSPI_CH0STAT` | `0x130` | Channel 0 status |
| `MCSPI_CH0CTRL` | `0x134` | Channel 0 enable |
| `MCSPI_TX0` | `0x138` | TX FIFO channel 0 |
| `MCSPI_RX0` | `0x13C` | RX FIFO channel 0 |
| `MCSPI_CH1CONF` | `0x140` | Channel 1 config |
| `MCSPI_XFERLEVEL` | `0x17C` | FIFO level config |

**CH0CONF bits quan trọng:**
```
Bit 0-1 (PHA/POL): SPI mode (CPOL, CPHA)
Bit 2-6 (CLKD)   : Clock divider
Bit 7   (EPOL)   : CS polarity
Bit 8-11 (WL)    : Word length - 1 (ví dụ 7 = 8-bit)
Bit 12  (TRM)    : 0=TX+RX, 1=RX only, 2=TX only
Bit 13  (DMAW)   : DMA write enable
Bit 14  (DMAR)   : DMA read enable
Bit 18  (IS)     : Input select (MISO pin)
Bit 19  (DPE0)   : TX on D0
Bit 20  (DPE1)   : TX on D1
```

> **Nguồn:** AM335x TRM `spruh73q.pdf`, Section 24 (McSPI), trang ~4700

### 5.3 Thiết bị SPI thực hành

| Thiết bị | Protocol | CS | Chức năng |
|----------|---------|-----|----------|
| MCP3008 | SPI mode 0 | P9.17 | 8-ch 10-bit ADC |
| W25Q32 | SPI mode 0 | P9.17 | 32Mbit SPI Flash |
| MAX7219 | SPI mode 0 | P9.17 | LED matrix controller |
| MCP4921 | SPI mode 0 | P9.17 | 12-bit DAC |

---
---

> **Xem thêm:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)


---

> **Xem them:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)

