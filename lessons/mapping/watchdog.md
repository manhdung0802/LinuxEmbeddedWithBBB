## 11. Watchdog (WDT1)

### 11.1 WDT Module

| Module | Base Address | IRQ | Clock | Mô tả |
|--------|-------------|-----|-------|-------|
| WDT1 | `0x44E35000` | 91 | 32KHz | Watchdog timer (reset nếu không ping) |

### 11.2 WDT Registers

| Register | Offset | Mô tả |
|----------|--------|-------|
| `WDT_WIDR` | `0x000` | IP revision |
| `WDT_WDSC` | `0x010` | System config |
| `WDT_WDST` | `0x014` | System status (RESETDONE) |
| `WDT_WISR` | `0x018` | IRQ status |
| `WDT_WIER` | `0x01C` | IRQ enable |
| `WDT_WCLR` | `0x024` | Control (prescaler) |
| `WDT_WCRR` | `0x028` | Counter register |
| `WDT_WLDR` | `0x02C` | Load register |
| `WDT_WTGR` | `0x030` | Trigger (ping) |
| `WDT_WWPS` | `0x034` | Write-pending status |
| `WDT_WDLY` | `0x044` | Delay (pre-reset IRQ) |
| `WDT_WSPR` | `0x048` | Start/stop protection |

**Sequence để start WDT (write-protected):**
```c
writel(0xBBBB, base + WDT_WSPR); /* step 1 */
while(readl(base + WDT_WWPS) & 0x10); /* wait W_PEND_WSPR */
writel(0x4444, base + WDT_WSPR); /* step 2 */
```

**Sequence để stop WDT:**
```c
writel(0xAAAA, base + WDT_WSPR);
while(readl(base + WDT_WWPS) & 0x10);
writel(0x5555, base + WDT_WSPR);
```

> **Nguồn:** AM335x TRM `spruh73q.pdf`, Section 20.4 (WDT), trang ~4327

---
---

> **Xem thêm:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)


---

> **Xem them:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)

