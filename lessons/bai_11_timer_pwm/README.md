# Bài 11 - Timer/PWM: DMTIMER, eCAP, eHRPWM

## Tóm tắt nhanh

| Module | Dùng cho |
|--------|---------|
| DMTIMER (8 module) | Delay, autoreload, event counting |
| eHRPWM (3 module) | PWM cho motor, LED dimming, servo |
| eCAP (3 module) | Đo tần số, duty cycle tín hiệu ngoài |

## Công thức DMTIMER

```
TLDR = 0xFFFFFFFF - (F_timer × T_ms / 1000) + 1
```
- DMTIMER2 clock = 24MHz
- 1ms → TLDR = 0xFFFFA1C1

## Công thức eHRPWM

```
F_PWM = F_TBCLK / (TBPRD + 1)   [up-count mode]
Duty  = CMPA / TBPRD × 100%
```

## File trong bài

- `bai_11_timer_pwm.md` — Bài học chi tiết
- `examples/` — Code DMTIMER + eHRPWM
- `exercises/` — Bài tập
- `solutions/` — Lời giải
