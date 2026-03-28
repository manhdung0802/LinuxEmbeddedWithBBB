# Bài 13 - Interrupt Controller (INTC)

## Tóm tắt nhanh

- **INTC base**: `0x48200000`, 128 interrupt channels
- **ARM CPU**: IRQ (vector `0x18`), FIQ (vector `0x1C`)
- **GPIO0 IRQ**: line 32/33; **UART0**: 72; **DMTIMER2**: 45

## Linux kernel workflow

```
gpio_to_irq(gpio_num)        → lấy Linux IRQ number
request_irq(irq, handler, flags, name, dev)   → đăng ký ISR
free_irq(irq, dev)           → hủy đăng ký
```

## Top Half / Bottom Half

| | Top Half (ISR) | Bottom Half |
|-|----------------|-------------|
| Sleep | ❌ Không | ✅ Có (workqueue) |
| Thời gian | Cực ngắn | Tùy ý |
| Dùng cho | Clear flag, schedule | Xử lý data nặng |

## File trong bài

- `bai_13_interrupt_controller.md` — Bài học chi tiết
- `examples/` — Kernel module ISR + workqueue
- `exercises/` — Bài tập
- `solutions/` — Lời giải
