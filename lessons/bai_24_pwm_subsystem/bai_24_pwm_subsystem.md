# Bài 24 - PWM Subsystem

## 1. Mục tiêu bài học
- Hiểu PWM framework trong Linux kernel
- Kiến trúc `pwm_chip`, `pwm_ops`
- AM335x EHRPWM (Enhanced High Resolution PWM)
- Viết PWM driver và sử dụng PWM từ consumer driver

---

## 2. AM335x PWM

### 2.1 PWM modules

AM335x có 3 EHRPWM module, mỗi module 2 output:

| Module | Base Address | Output A | Output B | Nguồn: spruh73q.pdf |
|--------|-------------|----------|----------|---------------------|
| EHRPWM0 | 0x48300200 | P9.22 | P9.21 | Chapter 15 |
| EHRPWM1 | 0x48302200 | P9.14 | P9.16 | Chapter 15 |
| EHRPWM2 | 0x48304200 | P8.19 | P8.13 | Chapter 15 |

### 2.2 EHRPWM Registers

| Register | Offset | Mô tả |
|----------|--------|-------|
| TBCTL | 0x00 | Time-Base Control |
| TBSTS | 0x02 | Time-Base Status |
| TBPHS | 0x06 | Time-Base Phase |
| TBCNT | 0x08 | Time-Base Counter |
| TBPRD | 0x0A | Time-Base Period |
| CMPAHR | 0x10 | Counter-Compare A HR |
| CMPA | 0x12 | Counter-Compare A |
| CMPB | 0x14 | Counter-Compare B |
| AQCTLA | 0x16 | Action-Qualifier Output A |
| AQCTLB | 0x18 | Action-Qualifier Output B |

### 2.3 PWM hoạt động

```
        TBPRD
  ────────┐
          │     ┌─────────
          │     │
  ────────┼─────┘
        CMPA
       (duty)

  Period = TBPRD / TBCLK
  Duty   = CMPA / TBPRD × 100%
```

---

## 3. Linux PWM Framework

### 3.1 Kiến trúc

```
┌─────────────────────────────────────────┐
│ PWM Consumer (LED, motor, buzzer)       │
│  pwm_get() / devm_pwm_get()            │
│  pwm_apply_state()                      │
├─────────────────────────────────────────┤
│ PWM Core (drivers/pwm/core.c)           │
│  pwmchip_add() / pwmchip_remove()       │
├─────────────────────────────────────────┤
│ PWM Provider Driver (pwm_chip + ops)    │
│  .apply(), .get_state()                 │
├─────────────────────────────────────────┤
│ Hardware (EHRPWM registers)             │
└─────────────────────────────────────────┘
```

### 3.2 Cấu trúc chính

```c
/* PWM chip - provider driver đăng ký */
struct pwm_chip {
    struct device      *dev;
    const struct pwm_ops *ops;
    int                 npwm;    /* Số channel */
};

/* PWM ops - callback từ provider */
struct pwm_ops {
    int (*apply)(struct pwm_chip *chip, struct pwm_device *pwm,
                  const struct pwm_state *state);
    int (*get_state)(struct pwm_chip *chip, struct pwm_device *pwm,
                      struct pwm_state *state);
};

/* PWM state - trạng thái PWM */
struct pwm_state {
    u64 period;             /* Period in nanoseconds */
    u64 duty_cycle;         /* Duty cycle in nanoseconds */
    enum pwm_polarity polarity;
    bool enabled;
};
```

---

## 4. PWM Provider Driver (EHRPWM simplified)

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/clk.h>

#define TBCTL    0x00
#define TBCNT    0x08
#define TBPRD    0x0A
#define CMPA     0x12
#define AQCTLA   0x16

struct learn_pwm {
	void __iomem *base;
	struct clk *clk;
	unsigned long clk_rate;
};

static int learn_pwm_apply(struct pwm_chip *chip,
                            struct pwm_device *pwm,
                            const struct pwm_state *state)
{
	struct learn_pwm *lpwm = pwmchip_get_drvdata(chip);
	u64 period_cycles, duty_cycles;

	/* Tính period (cycles) = period_ns * clk_rate / 1e9 */
	period_cycles = mul_u64_u64_div_u64(state->period,
	                                     lpwm->clk_rate, NSEC_PER_SEC);
	duty_cycles = mul_u64_u64_div_u64(state->duty_cycle,
	                                   lpwm->clk_rate, NSEC_PER_SEC);

	if (period_cycles > 0xFFFF)
		period_cycles = 0xFFFF;
	if (duty_cycles > period_cycles)
		duty_cycles = period_cycles;

	/* Ghi registers */
	writew(0, lpwm->base + TBCNT);                  /* Reset counter */
	writew(period_cycles - 1, lpwm->base + TBPRD);  /* Period */
	writew(duty_cycles, lpwm->base + CMPA);          /* Duty */

	/* Action: set high at zero, set low at CMPA */
	writew(0x0012, lpwm->base + AQCTLA);

	if (state->enabled) {
		/* TBCTL: count up mode, no prescaler */
		writew(0x0000, lpwm->base + TBCTL);
	} else {
		/* TBCTL: stop-freeze */
		writew(0x0003, lpwm->base + TBCTL);
	}

	return 0;
}

static int learn_pwm_get_state(struct pwm_chip *chip,
                                struct pwm_device *pwm,
                                struct pwm_state *state)
{
	struct learn_pwm *lpwm = pwmchip_get_drvdata(chip);
	u16 tbprd, cmpa, tbctl;

	tbprd = readw(lpwm->base + TBPRD);
	cmpa = readw(lpwm->base + CMPA);
	tbctl = readw(lpwm->base + TBCTL);

	state->period = DIV_ROUND_UP_ULL((u64)(tbprd + 1) * NSEC_PER_SEC,
	                                  lpwm->clk_rate);
	state->duty_cycle = DIV_ROUND_UP_ULL((u64)cmpa * NSEC_PER_SEC,
	                                      lpwm->clk_rate);
	state->polarity = PWM_POLARITY_NORMAL;
	state->enabled = ((tbctl & 0x03) == 0);

	return 0;
}

static const struct pwm_ops learn_pwm_ops = {
	.apply = learn_pwm_apply,
	.get_state = learn_pwm_get_state,
};

static int learn_pwm_probe(struct platform_device *pdev)
{
	struct learn_pwm *lpwm;
	struct pwm_chip *chip;

	chip = devm_pwmchip_alloc(&pdev->dev, 2, sizeof(*lpwm));
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	lpwm = pwmchip_get_drvdata(chip);

	lpwm->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(lpwm->base))
		return PTR_ERR(lpwm->base);

	lpwm->clk = devm_clk_get_enabled(&pdev->dev, "fck");
	if (IS_ERR(lpwm->clk))
		return PTR_ERR(lpwm->clk);

	lpwm->clk_rate = clk_get_rate(lpwm->clk);

	chip->ops = &learn_pwm_ops;

	return devm_pwmchip_add(&pdev->dev, chip);
}

static const struct of_device_id learn_pwm_of_match[] = {
	{ .compatible = "learn,ehrpwm" },
	{ },
};
MODULE_DEVICE_TABLE(of, learn_pwm_of_match);

static struct platform_driver learn_pwm_driver = {
	.probe = learn_pwm_probe,
	.driver = {
		.name = "learn-pwm",
		.of_match_table = learn_pwm_of_match,
	},
};

module_platform_driver(learn_pwm_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Learning EHRPWM driver for AM335x");
```

---

## 5. PWM Consumer

### 5.1 Trong driver

```c
struct pwm_device *pwm;
struct pwm_state state;

pwm = devm_pwm_get(&pdev->dev, NULL);

pwm_init_state(pwm, &state);
state.period = 1000000;       /* 1ms = 1kHz */
state.duty_cycle = 500000;    /* 50% */
state.enabled = true;
pwm_apply_state(pwm, &state);
```

### 5.2 Từ sysfs

```bash
# Export PWM channel
echo 0 > /sys/class/pwm/pwmchip0/export

# Cấu hình
echo 1000000 > /sys/class/pwm/pwmchip0/pwm0/period
echo 500000 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle
echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable
```

### 5.3 Device Tree consumer

```dts
buzzer {
    compatible = "pwm-beeper";
    pwms = <&ehrpwm1 0 1000000 0>;
    /* phandle channel period_ns flags */
};

backlight {
    compatible = "pwm-backlight";
    pwms = <&ehrpwm1 1 5000000 0>;
    brightness-levels = <0 10 20 50 100 200 255>;
    default-brightness-level = <3>;
};
```

---

## 6. Kiến thức cốt lõi

1. **`pwm_chip`** quản lý N channel, đăng ký qua `devm_pwmchip_add()`
2. **`pwm_ops.apply()`** nhận `pwm_state` (period, duty_cycle, enabled) tính bằng nanosecond
3. **EHRPWM**: TBPRD = period, CMPA = duty, AQCTLA = action qualifier
4. **Consumer API**: `devm_pwm_get()`, `pwm_apply_state()`
5. **Sysfs interface**: `/sys/class/pwm/pwmchipX/`
6. **DT consumer**: `pwms = <phandle channel period flags>`

---

## 7. Câu hỏi kiểm tra

1. `pwm_state.period` và `duty_cycle` tính bằng đơn vị gì?
2. EHRPWM register nào quyết định duty cycle?
3. Sự khác nhau giữa `pwm_apply_state()` (consumer) và `pwm_ops.apply()` (provider)?
4. Tại sao AQCTLA = 0x0012 sẽ tạo PWM output?
5. PWM consumer trong DT khai báo như thế nào?

---

## 8. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 25 - ADC Driver (IIO Subsystem)**

Ta sẽ học IIO framework chi tiết: `iio_dev`, `iio_chan_spec`, và AM335x TSC_ADC subsystem.
