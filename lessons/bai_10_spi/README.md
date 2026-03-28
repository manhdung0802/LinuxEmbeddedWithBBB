# Bài 10 - SPI: Giao Tiếp với ADC/DAC Bên Ngoài

## Tóm tắt nhanh

- **2 McSPI module**: SPI0 (`0x48030000`), SPI1 (`0x481A0000`)
- **SPI0 trên P9**: SCLK=P9.22, MISO=P9.21, MOSI=P9.18, CS0=P9.17
- **Clock**: F_SPI = 48MHz / 2^(CLKD+1)

## 4 Mode SPI

| Mode | CPOL | CPHA |
|------|------|------|
| 0    | 0    | 0    | ← phổ biến nhất
| 1    | 0    | 1    |
| 2    | 1    | 0    |
| 3    | 1    | 1    |

## Thanh ghi cần nhớ

| Tên | Offset | Mục đích |
|-----|--------|---------|
| MCSPI_CH0CONF | 0x12C | WL, CLKD, Mode, CS polarity |
| MCSPI_CH0STAT | 0x130 | TXS(1), RXS(0) |
| MCSPI_CH0CTRL | 0x134 | Enable channel |
| MCSPI_TX0 | 0x138 | Ghi data TX |
| MCSPI_RX0 | 0x13C | Đọc data RX |

## File trong bài

- `bai_10_spi.md` — Bài học chi tiết
- `examples/` — Code đọc ADC MCP3204
- `exercises/` — Bài tập
- `solutions/` — Lời giải
