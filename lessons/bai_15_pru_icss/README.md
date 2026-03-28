# Bài 15 - PRU-ICSS: Real-time Programming

## Tóm tắt nhanh

- **2 PRU cores**: 200MHz, 5ns/instruction, deterministic
- **IRAM**: 8KB/PRU; **DRAM**: 8KB/PRU; **Shared**: 12KB
- **R30** = output GPIO register; **R31** = input GPIO register
- **PRU0 base**: `0x4A334000` (IRAM), `0x4A300000` (DRAM)

## Tại sao dùng PRU

| | ARM + Linux | PRU |
|-|-------------|-----|
| Toggle GPIO 1µs | ❌ OS jitter | ✅ Exact |
| WS2812/1-wire | ❌ | ✅ |
| Real-time loop | ❌ | ✅ |

## Linux workflow

```bash
cp firmware.out /lib/firmware/am335x-pru0-fw
echo start > /sys/class/remoteproc/remoteproc1/state
# Giao tiếp: /dev/rpmsg0
```

## File trong bài

- `bai_15_pru_icss.md` — Bài học chi tiết
- `examples/` — PRU blink + RPMsg
- `exercises/` — Bài tập
- `solutions/` — Lời giải
