# Bài 8 - UART: Thanh Ghi, Baud Rate và Giao Tiếp Serial

## Tóm tắt nhanh

- **6 UART**: UART0..5, UART0 (`0x44E09000`) là console Linux
- **Clock**: 48MHz từ DPLL_PER
- **Baud rate**: Divisor = 48,000,000 / (16 × baud)
  - 115200 → Divisor = 26 (DLL=0x1A, DLH=0)

## Trình tự khởi tạo

```
1. Bật clock → 2. Pinmux mode 0 → 3. MDR1=0x7 (disable)
→ 4. LCR[7]=1 → 5. DLL/DLH → 6. LCR=8N1 → 7. FCR → 8. MDR1=0x0
```

## Thanh ghi cần nhớ

| Tên | Offset | Mục đích |
|-----|--------|---------|
| THR/RHR | 0x00 | Ghi/đọc dữ liệu |
| LCR | 0x0C | Frame format + DLAB mode |
| LSR | 0x14 | TX rỗng (bit5), RX ready (bit0) |
| DLL/DLH | 0x00/0x04 | Baud divisor (khi LCR[7]=1) |
| MDR1 | 0x20 | 0x0=enable, 0x7=disable |

## File trong bài

- `bai_08_uart.md` — Bài học chi tiết
- `examples/` — Code uart_init example
- `exercises/` — Bài tập
- `solutions/` — Lời giải
