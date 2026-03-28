# Bài 6 - Clock Module (CM): PRCM, Clock Gating, Enabling Modules

## Tóm tắt nhanh

- **PRCM** = Power, Reset, Clock Management
- **CM_PER** (`0x44E00000`): clock cho GPIO1..3, UART1..5, SPI, I2C, Timer
- **CM_WKUP** (`0x44E00400`): clock cho GPIO0, UART0, I2C0, ADC

## Cấu trúc CLKCTRL

```
Bit [1:0] = MODULEMODE: 0x2 = ENABLE
Bit [17:16] = IDLEST (read-only): 0x0 = Functional
```

## Thanh ghi hay dùng

| Module | Địa chỉ |
|--------|---------|
| GPIO1 | `0x44E000AC` (+ bit 18) |
| GPIO2 | `0x44E000B0` |
| UART0 | `0x44E004B4` |
| I2C1  | `0x44E004A0` |

## File trong bài

- `bai_06_clock_module.md` — Bài học chi tiết
- `examples/` — Code hàm enable_module
- `exercises/` — Bài tập
- `solutions/` — Lời giải
