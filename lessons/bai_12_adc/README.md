# Bài 12 - ADC (TSC_ADC): Đọc Analog

## ⚠️ Cảnh báo quan trọng

**Input tối đa: 1.8V** trên AIN pins. Điện áp cao hơn sẽ **hỏng SoC**.

## Tóm tắt nhanh

- **TSC_ADC base**: `0x44E0D000`
- **12-bit ADC**: 0..4095, Vref = 1.8V
- **8 kênh**: AIN0..AIN6 trên P9 (xem bảng P9)
- `V = (raw / 4095) × 1.8`

## Trình tự đọc

```
Bật clock → Disable module → Config STEPCONFIGx
→ Enable module → Set STEPENABLE → Đọc FIFO0DATA
```

## Linux IIO

```bash
cat /sys/bus/iio/devices/iio:device0/in_voltage0_raw
```

## File trong bài

- `bai_12_adc.md` — Bài học chi tiết
- `examples/` — Code đọc AIN0
- `exercises/` — Bài tập
- `solutions/` — Lời giải
