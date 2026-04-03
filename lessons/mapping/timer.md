## 8. Timer (DMTIMER)

### 8.1 DMTIMER Modules

| Timer | Base Address | IRQ | Clock | Dùng cho |
|-------|-------------|-----|-------|---------|
| DMTIMER0 | `0x44E05000` | 66 | CLKDIV32K/32KHz | udelay — không override |
| DMTIMER1ms | `0x44E31000` | 67 | 24MHz | OS tick (jiffies) — không override |
| **DMTIMER2** | **`0x48040000`** | **68** | **24MHz/32KHz** | **Dùng cho học** |
| DMTIMER3 | `0x48042000` | 69 | 24MHz/32KHz | Dùng được |
| DMTIMER4 | `0x48044000` | 92 | 24MHz/32KHz | |
| DMTIMER5 | `0x48046000` | 93 | 24MHz/32KHz | |
| DMTIMER6 | `0x48048000` | 94 | 24MHz/32KHz | |
| DMTIMER7 | `0x4804A000` | 95 | 24MHz/32KHz | |

### 8.2 DMTIMER Registers

| Register | Offset | Mô tả |
|----------|--------|-------|
| `TIDR` | `0x000` | IP revision |
| `TIOCP_CFG` | `0x010` | OCP interface config |
| `TISTAT` | `0x014` | Status (RESETDONE) |
| `TISR` | `0x018` | IRQ status (OVF, TCAR, MAT) |
| `TIER` | `0x01C` | IRQ enable |
| `TWER` | `0x020` | Wakeup enable |
| `TCLR` | `0x024` | Control (start/stop, mode, compare) |
| `TCRR` | `0x028` | Counter register (current value) |
| `TLDR` | `0x02C` | Load/reload value |
| `TTGR` | `0x030` | Trigger (force reload) |
| `TWPS` | `0x034` | Write-posted pending status |
| `TMAR` | `0x038` | Compare value |
| `TCAR1` | `0x03C` | Capture value 1 |
| `TSICR` | `0x040` | Interface control (posted mode) |
| `TCAR2` | `0x044` | Capture value 2 |

**TCLR bits:**
```
Bit 0 (ST)   : 1=start counting
Bit 1 (AR)   : 0=one-shot, 1=auto-reload
Bit 2 (PTC)  : prescaler flag
Bit 5-7(PT)  : prescaler output
Bit 6 (CE)   : compare enable
Bit 12(GPO)  : PORGPOCFG output
```

> **Nguồn:** AM335x TRM `spruh73q.pdf`, Section 20 (DMTIMER), trang ~4220

---
---

> **Xem thêm:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)


---

> **Xem them:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)

