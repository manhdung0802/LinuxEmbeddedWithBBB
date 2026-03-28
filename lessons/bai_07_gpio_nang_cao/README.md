# Bài 7 - GPIO Nâng Cao: Interrupt và Debounce

## Tóm tắt nhanh

- **Edge detect**: `GPIO_RISINGDETECT` (0x148), `GPIO_FALLINGDETECT` (0x14C)
- **Interrupt enable**: `GPIO_IRQENABLE_0` (0x034)
- **Interrupt status**: `GPIO_IRQSTATUS_0` (0x02C) — ghi 1 để clear
- **Debounce HW**: `GPIO_DEBOUNCENABLE` (0x150), `GPIO_DEBOUNCINGTIME` (0x154)
- **Debounce time**: T = (value + 1) × 31µs

## Linux userspace

```bash
echo falling > /sys/class/gpio/gpioN/edge
```
Rồi dùng `poll(fd, POLLPRI)` để chờ interrupt.

## File trong bài

- `bai_07_gpio_nang_cao.md` — Bài học chi tiết
- `examples/` — Code poll() interrupt
- `exercises/` — Bài tập
- `solutions/` — Lời giải
