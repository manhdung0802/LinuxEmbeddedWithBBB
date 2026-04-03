## 7. PWM (EHRPWM)

### 7.1 EHRPWM Modules — Base & Pins

| Module | Base Address | IRQ | Output A | Output B | Dùng cho |
|--------|-------------|-----|---------|---------|---------|
| EHRPWM0 | `0x48300200` | 86 | P9.22 (xung đột SPI0) | P9.21 | Cẩn thận |
| **EHRPWM1** | **`0x48302200`** | **87** | **P9.14** | **P9.16** | **Dùng cho học** |
| **EHRPWM2** | **`0x48304200`** | **39** | **P8.19** | **P8.13** | **Dùng cho học** |

### 7.2 EHRPWM Registers

| Register | Offset | Mô tả |
|----------|--------|-------|
| `TBCTL` | `0x00` | Time-Base Control |
| `TBSTS` | `0x02` | Time-Base Status |
| `TBPHSHR` | `0x04` | High-res phase |
| `TBPHS` | `0x06` | Phase offset |
| `TBCNT` | `0x08` | Counter (current value) |
| `TBPRD` | `0x0A` | Period (top count) |
| `CMPCTL` | `0x0E` | Compare Control |
| `CMPAHR` | `0x10` | Compare A high-res |
| `CMPA` | `0x12` | Compare A value |
| `CMPB` | `0x14` | Compare B value |
| `AQCTLA` | `0x16` | Action-Qualifier Control A |
| `AQCTLB` | `0x18` | Action-Qualifier Control B |
| `AQSFRC` | `0x1A` | AQ Software Force |
| `AQCSFRC` | `0x1C` | AQ Continuous SW Force |
| `DBCTL` | `0x1E` | Dead-Band Control |
| `DBRED` | `0x20` | Dead-Band Rising edge delay |
| `DBFED` | `0x22` | Dead-Band Falling edge delay |
| `PCCTL` | `0x3C` | PWM-Chopper Control |
| `TZSEL` | `0x40` | Trip-Zone Select |
| `TZEINT` | `0x44` | TZ Interrupt |
| `ETSEL` | `0x48` | Event-Trigger Select |
| `ETPS` | `0x4A` | Event-Trigger Prescale |
| `ETFLG` | `0x4C` | Event-Trigger Flag |
| `ETFRC` | `0x50` | Event-Trigger Force |

**Ví dụ cấu hình PWM 50Hz (servo), TBCLK = SYSCLK/1 = 100MHz:**
```
TBPRD = 2000000 - 1  (20ms period)
CMPA  = 150000       (1.5ms = 90° position)
TBCTL: CLKDIV=0, HSPCLKDIV=0, CTRMODE=0 (up-count)
AQCTLA: CAU=2 (set high on TBCNT==CMPA up), PRD=1 (clear on period)
```

> **Nguồn:** AM335x TRM `spruh73q.pdf`, Section 15.3 (EPWM registers), trang ~2700

---
---

> **Xem thêm:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)


---

> **Xem them:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)

