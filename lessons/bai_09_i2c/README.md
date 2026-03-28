# Bài 9 - I2C: Master/Slave, Đọc EEPROM và Cảm Biến

## Tóm tắt nhanh

- **3 I2C module**: I2C0 (`0x44E0B000`), I2C1 (`0x4802A000`), I2C2 (`0x4819C000`)
- **I2C1 trên P9**: SCL = P9.17, SDA = P9.18
- **Clock**: PSC=3, SCLL=53, SCLH=55 → 100kHz

## Thanh ghi cần nhớ

| Tên | Offset | Mục đích |
|-----|--------|---------|
| I2C_CON | 0xA4 | Enable, master/slave, TX/RX, START/STOP |
| I2C_SA  | 0xAC | Slave address |
| I2C_DCOUNT | 0x98 | Số byte giao dịch |
| I2C_DATA | 0x9C | Đọc/ghi dữ liệu |
| I2C_IRQSTATUS | 0x28 | RRDY(4), XRDY(3), ARDY(2), NACK(1) |

## Linux userspace

```bash
modprobe i2c-dev
i2cdetect -y -r 1   # scan bus
```
Rồi dùng `ioctl(fd, I2C_SLAVE, addr)`.

## File trong bài

- `bai_09_i2c.md` — Bài học chi tiết
- `examples/` — Code đọc TMP102
- `exercises/` — Bài tập
- `solutions/` — Lời giải
