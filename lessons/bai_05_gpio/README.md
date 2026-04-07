# Bài 5 - GPIO: Control Module, Pad Config và Toggle LED

## Tóm tắt nhanh

- **4 GPIO module**: GPIO0..3, base `0x44E07000` / `0x4804C000` / `0x481AC000` / `0x481AE000`
- **Control Module** (`0x44E10000`): pinmux, pull-up/down, input enable
- **Công thức**: Linux GPIO = `module * 32 + bit`

## Thanh ghi cần nhớ

| Thanh ghi | Offset | Dùng để |
|-----------|--------|---------|
| `GPIO_OE` | `0x134` | 0=output, 1=input |
| `GPIO_DATAIN` | `0x138` | Đọc trạng thái chân |
| `GPIO_SETDATAOUT` | `0x194` | Set bit lên 1 (atomic) |
| `GPIO_CLEARDATAOUT` | `0x190` | Clear bit về 0 (atomic) |

## Trình tự GPIO

```
1. Bật clock (CM_PER_GPIO1_CLKCTRL)
2. Pinmux (conf_xxx, mode 7 = GPIO)
3. Direction (GPIO_OE)
4. Đọc/ghi data
```

## File trong bài

- `bai_05_gpio.md` — Bài học chi tiết
- `examples/` — Code ví dụ blink LED
- `exercises/` — Bài tập thực hành
- `solutions/` — Lời giải

## Trạng thái

- **Hoàn thành:** 2026-04-07 — Quiz mục 8 đã trả lời và đánh giá nhanh.
- **Tiếp theo:** Chuẩn bị Bài 06 - Clock Module
