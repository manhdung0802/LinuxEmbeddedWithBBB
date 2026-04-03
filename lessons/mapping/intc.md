## 3. Interrupt Controller (INTC)

### 3.1 INTC Base

| Module | Base Address |
|--------|-------------|
| AM335x INTC | `0x48200000` |

### 3.2 INTC Registers

| Register | Offset | Mô tả |
|----------|--------|-------|
| `INTC_REVISION` | `0x000` | IP revision |
| `INTC_SYSCONFIG` | `0x010` | Software reset |
| `INTC_SYSSTATUS` | `0x014` | Reset done |
| `INTC_SIR_IRQ` | `0x040` | Active IRQ number |
| `INTC_SIR_FIQ` | `0x044` | Active FIQ number |
| `INTC_CONTROL` | `0x048` | New IRQ/FIQ agreement |
| `INTC_MIR(n)` | `0x084 + 0x20*n` | Mask IRQ group n (32 IRQs each) |
| `INTC_MIR_SET(n)` | `0x08C + 0x20*n` | Mask (disable) bits |
| `INTC_MIR_CLEAR(n)` | `0x088 + 0x20*n` | Unmask (enable) bits |
| `INTC_ITR(n)` | `0x080 + 0x20*n` | Raw interrupt status |
| `INTC_PENDING_IRQ(n)` | `0x098 + 0x20*n` | Pending (after mask) |
| `INTC_ILR(m)` | `0x100 + 4*m` | Priority + IRQ/FIQ routing |

### 3.3 IRQ Numbers Thường Dùng trên BBB

| Peripheral | IRQ | Linux IRQ (thông thường) | Mô tả |
|-----------|-----|--------------------------|-------|
| DMTIMER0 | 66 | - | Phan cho udelay |
| DMTIMER1ms | 67 | - | OS tick |
| **DMTIMER2** | **68** | - | Dùng cho học |
| DMTIMER3 | 69 | - | |
| DMTIMER4 | 92 | - | |
| DMTIMER5 | 93 | - | |
| DMTIMER6 | 94 | - | |
| DMTIMER7 | 95 | - | |
| UART0 | 72 | - | Debug console |
| **UART1** | **73** | - | `/dev/ttyO1` học |
| UART2 | 74 | - | |
| UART3 | 44 | - | |
| UART4 | 45 | - | |
| UART5 | 46 | - | |
| **I2C0** | **70** | - | PMIC (không dùng) |
| **I2C1** | **71** | - | HDMI cape |
| **I2C2** | **30** | - | **Dùng cho học** |
| **SPI0** | **65** | - | **Dùng cho học** |
| **SPI1** | **125** | - | |
| **GPIO0** | **96** | - | GPIO0 interrupt |
| **GPIO1** | **98** | - | **GPIO1 interrupt** |
| **GPIO2** | **32** | - | GPIO2 interrupt |
| **GPIO3** | **62** | - | GPIO3 interrupt |
| TSCADC | 16 | - | ADC/touchscreen |
| EDMA3CC0 | 12 | - | EDMA completion |
| EDMA3CC0_ERR | 14 | - | EDMA error |
| USB0 | 18 | - | |
| MMCSD0 | 64 | - | eMMC |
| EHRPWM0 | 86 | - | |
| EHRPWM1 | 87 | - | P9.14/P9.16 |
| EHRPWM2 | 39 | - | P8.19/P8.13 |
| WDT1 | 91 | - | Watchdog |

> **Nguồn:** AM335x TRM `spruh73q.pdf`, Section 6.3 (Interrupt Mapping Table), trang ~303

---
---

> **Xem thêm:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)


---

> **Xem them:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)

