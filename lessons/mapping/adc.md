## 9. ADC (TSC_ADC)

### 9.1 TSC_ADC Module

| Module | Base Address | IRQ | Voltage range | Chú ý |
|--------|-------------|-----|--------------|-------|
| TSC_ADC | `0x44E0D000` | 16 | **0V – 1.8V** | **TUYỆT ĐỐI không vượt 1.8V** |

### 9.2 ADC Channels — P9 Pins

| Channel | P9 Pin | Mô tả | Chú ý |
|---------|--------|-------|-------|
| AIN0 | **P9.39** | ADC channel 0 | VREF P9.32 (1.8V), AGND P9.34 |
| AIN1 | P9.40 | ADC channel 1 | |
| AIN2 | P9.37 | ADC channel 2 | |
| AIN3 | P9.38 | ADC channel 3 | |
| AIN4 | P9.33 | ADC channel 4 | |
| AIN5 | P9.36 | ADC channel 5 | |
| AIN6 | P9.35 | ADC channel 6 | |
| VREFP | P9.32 | 1.8V reference | |
| AGND | P9.34 | Analog GND | |

### 9.3 TSC_ADC Registers

| Register | Offset | Mô tả |
|----------|--------|-------|
| `TSCADC_REVISION` | `0x000` | IP version |
| `TSCADC_SYSCONFIG` | `0x010` | System config |
| `TSCADC_SYSSTATUS` | `0x014` | System status |
| `TSCADC_IRQSTATUS_RAW` | `0x024` | Raw IRQ |
| `TSCADC_IRQSTATUS` | `0x028` | Enabled IRQ |
| `TSCADC_IRQENABLE_SET` | `0x02C` | IRQ enable |
| `TSCADC_IRQENABLE_CLR` | `0x030` | IRQ clear |
| `TSCADC_DMAENABLE` | `0x034` | DMA enable |
| `TSCADC_CTRL` | `0x040` | Main control |
| `TSCADC_ADCRANGE` | `0x048` | ADC range |
| `TSCADC_CLKDIV` | `0x04C` | Clock divider |
| `TSCADC_MISC` | `0x050` | Misc config |
| `TSCADC_STEPCONFIG0` | `0x064` | Step 0 config |
| `TSCADC_STEPDELAY0` | `0x068` | Step 0 delay |
| `TSCADC_STEPCONFIG1` | `0x06C` | Step 1 config |
| `TSCADC_STEPDELAY1` | `0x070` | Step 1 delay |
| `TSCADC_FIFO0COUNT` | `0x0E4` | FIFO0 count |
| `TSCADC_FIFO0THRESHOLD` | `0x0E8` | FIFO0 threshold |
| `TSCADC_FIFO1COUNT` | `0x0F0` | FIFO1 count |
| `TSCADC_FIFO1THRESHOLD` | `0x0F4` | FIFO1 threshold |
| `TSCADC_FIFODATA` | `0x100` | FIFO data read-out |

**STEPCONFIG bits (chọn channel):**
```
Bit 19-16 (SEL_INP_SWC) : chọn AIN0-AIN7
Bit 26-20 (RFP_SWC)     : reference positive
Bit 3-1   (MODE)        : 0=SW-one-shot, 1=SW-cont, 4=HW-sync
```

> **Nguồn:** AM335x TRM `spruh73q.pdf`, Section 12 (TSC_ADC), trang ~1777

---
---

> **Xem thêm:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)


---

> **Xem them:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)

