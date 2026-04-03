## 6. UART

### 6.1 UART Modules — Base Address & Pins

| Module | Base Address | IRQ | TX Pin | RX Pin | `/dev` | Dùng cho |
|--------|-------------|-----|--------|--------|--------|---------|
| UART0 | `0x44E09000` | 72 | J1 header | J1 header | `/dev/ttyO0` | **Debug console — không override** |
| **UART1** | **`0x48022000`** | **73** | **P9.24** | **P9.26** | **`/dev/ttyO1`** | **Dùng cho học** |
| UART2 | `0x48024000` | 74 | P9.21 | P9.22 | `/dev/ttyO2` | Xung đột SPI0 |
| UART3 | `0x481A6000` | 44 | P9.42 | - | `/dev/ttyO3` | Chỉ TX |
| UART4 | `0x481A8000` | 45 | P9.13 | P9.11 | `/dev/ttyO4` | Dùng được |
| UART5 | `0x481AA000` | 46 | P8.37 | P8.38 | `/dev/ttyO5` | Dùng được (HDMI) |

### 6.2 UART Registers

| Register | Offset | Mô tả |
|----------|--------|-------|
| `UART_DLL` | `0x00` | Divisor latch low (baud rate) |
| `UART_RHR/THR` | `0x00` | RX hold / TX hold (data byte) |
| `UART_DLH` | `0x04` | Divisor latch high |
| `UART_IER` | `0x04` | Interrupt enable |
| `UART_FCR/IIR` | `0x08` | FIFO control / IRQ ID |
| `UART_LCR` | `0x0C` | Line control (data bits, stop, parity) |
| `UART_MCR` | `0x10` | Modem control (RTS/CTS) |
| `UART_LSR` | `0x14` | Line status |
| `UART_MSR` | `0x18` | Modem status |
| `UART_SCR` | `0x1C` | Scratch pad |
| `UART_MDR1` | `0x20` | Mode (0=16x UART, 1=SIR) |
| `UART_SSR` | `0x44` | Supplementary status |
| `UART_SYSC` | `0x54` | System config |
| `UART_SYSS` | `0x58` | System status |
| `UART_WER` | `0x5C` | Wakeup enable |
| `UART_CFPS` | `0x60` | Carrier freq prescaler |

**Tính baud rate với functional clock 48MHz:**
```
divisor = 48000000 / (16 × baud_rate)
115200 baud → divisor = 48000000 / (16 × 115200) = 26
DLL = 26 & 0xFF, DLH = (26 >> 8) & 0xFF
```

> **Nguồn:** AM335x TRM `spruh73q.pdf`, Section 19 (UART), trang ~4100

---
---

> **Xem thêm:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)


---

> **Xem them:** [device_tree_snippets.md](device_tree_snippets.md) | [kernel_api.md](kernel_api.md) | [pin_conflicts.md](pin_conflicts.md)

