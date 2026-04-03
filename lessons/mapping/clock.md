## 12. Clock Domains

### 12.1 CM_PER (Peripheral Clocks)

| Register | Address | Điều khiển |
|----------|---------|-----------|
| `CM_PER_CPGMAC0_CLKCTRL` | `0x44E00014` | Ethernet |
| `CM_PER_UART1_CLKCTRL` | `0x44E0006C` | UART1 |
| `CM_PER_UART2_CLKCTRL` | `0x44E00070` | UART2 |
| `CM_PER_UART3_CLKCTRL` | `0x44E00074` | UART3 |
| `CM_PER_UART4_CLKCTRL` | `0x44E00078` | UART4 |
| `CM_PER_UART5_CLKCTRL` | `0x44E0007C` | UART5 |
| `CM_PER_I2C1_CLKCTRL` | `0x44E00048` | I2C1 |
| `CM_PER_I2C2_CLKCTRL` | `0x44E00044` | I2C2 |
| `CM_PER_SPI0_CLKCTRL` | `0x44E0004C` | SPI0 |
| `CM_PER_SPI1_CLKCTRL` | `0x44E00050` | SPI1 |
| `CM_PER_GPIO1_CLKCTRL` | `0x44E000AC` | GPIO1 |
| `CM_PER_GPIO2_CLKCTRL` | `0x44E000B0` | GPIO2 |
| `CM_PER_GPIO3_CLKCTRL` | `0x44E000B4` | GPIO3 |
| `CM_PER_TIMER2_CLKCTRL` | `0x44E00080` | DMTIMER2 |
| `CM_PER_TIMER3_CLKCTRL` | `0x44E00084` | DMTIMER3 |
| `CM_PER_TIMER4_CLKCTRL` | `0x44E00088` | DMTIMER4 |
| `CM_PER_TIMER5_CLKCTRL` | `0x44E000EC` | DMTIMER5 |
| `CM_PER_TIMER6_CLKCTRL` | `0x44E000F0` | DMTIMER6 |
| `CM_PER_TIMER7_CLKCTRL` | `0x44E000F4` | DMTIMER7 |
| `CM_PER_EPWMSS1_CLKCTRL` | `0x44E000CC` | EHRPWM1 subsystem |
| `CM_PER_EPWMSS2_CLKCTRL` | `0x44E000D4` | EHRPWM2 subsystem |
| `CM_PER_TPCC_CLKCTRL` | `0x44E000BC` | EDMA3 CC |
| `CM_PER_TPTC0_CLKCTRL` | `0x44E000C0` | EDMA3 TC0 |

### 12.2 CM_WKUP (Wakeup Clocks)

| Register | Address | Điều khiển |
|----------|---------|-----------|
| `CM_WKUP_GPIO0_CLKCTRL` | `0x44E00408` | GPIO0 |
| `CM_WKUP_TIMER0_CLKCTRL` | `0x44E00410` | DMTIMER0 |
| `CM_WKUP_TIMER1_CLKCTRL` | `0x44E00418` | DMTIMER1ms |
| `CM_WKUP_I2C0_CLKCTRL` | `0x44E004B8` | I2C0 (PMIC) |
| `CM_WKUP_ADC_TSC_CLKCTRL` | `0x44E004BC` | TSC_ADC |
| `CM_WKUP_WDT1_CLKCTRL` | `0x44E004D4` | WDT1 |
| `CM_WKUP_UART0_CLKCTRL` | `0x44E004B0` | UART0 (debug) |

**CLKCtrl register format:**
```
Bit 1-0 (MODULEMODE) : 0=disabled, 1=SW-explicit (không dùng), 2=enabled
Bit 17-16 (IDLEST)   : 0=functional, 1=transitioning, 2=idle, 3=disabled
Bit 18    (STBYST)   : 0=functional, 1=standby (có trên một số modules)
```

**Để enable clock trong code:**
```c
/* Đọc–sửa–ghi (không xóa các bit reserved) */
val = readl(cm_base + CM_PER_I2C2_CLKCTRL);
val = (val & ~0x3) | 2;      /* MODULEMODE=2 (enable) */
writel(val, cm_base + CM_PER_I2C2_CLKCTRL);
/* Đợi IDLEST=0 */
while ((readl(cm_base + CM_PER_I2C2_CLKCTRL) >> 16) & 0x3);
```

> **Nguồn:** AM335x TRM `spruh73q.pdf`, Section 8 (Clock Management), trang ~1100

---
---

> **Xem thêm:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)


---

> **Xem them:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)

