# B�i 20 - AM335x EHRPWM Driver

> **Tham chi?u ph?n c?ng BBB:** Xem [pwm.md](../mapping/pwm.md)

## 0. BBB Connection � EHRPWM tr�n BeagleBone Black

> :warning: **AN TOAN DIEN:**
> - PWM output la **3.3V logic, max 6mA** - KHONG noi truc tiep motor/servo vao chan PWM
> - Dung **MOSFET driver** (VD: IRF520) hoac **H-bridge** (VD: L298N) cho motor DC/servo lon
> - LED: dung dien tro han dong 330-1k ohm
> - KHONG noi 5V/12V ve chan PWM BBB

### 0.1 So d? k?t n?i th?c t?

```
BeagleBone Black P9 header
                        +------+
  P9.14 (EHRPWM1A) ----?�  LED �--+
                         +------+  �
  P9.16 (EHRPWM1B) ----?� SERVO �  � 330O
                                   �
  P9.1/P9.2 (GND)  ----------------+

BeagleBone Black P8 header
  P8.19 (EHRPWM2A) ----?Motor/Buzzer
  P8.13 (EHRPWM2B) ----?Motor/Buzzer
```

### 0.2 B?ng PWM ? BBB Pin

| EHRPWM | Output | Pin | Pinmux Mode | Ghi ch� |
|--------|--------|-----|------------|---------|
| EHRPWM0 | A | P9.22 | mode 3 | Xung d?t SPI0_SCLK � d�ng c?n th?n |
| EHRPWM0 | B | P9.21 | mode 3 | Xung d?t SPI0_D0 |
| **EHRPWM1** | **A** | **P9.14** | **mode 6** | **KHUY�N D�NG � test LED dim** |
| **EHRPWM1** | **B** | **P9.16** | **mode 6** | **KHUY�N D�NG � test servo** |
| EHRPWM2 | A | P8.19 | mode 4 | Test motor speed |
| EHRPWM2 | B | P8.13 | mode 4 | Test motor speed |

### 0.3 Pinmux c?n thi?t cho P9.14 (EHRPWM1A)

```c
/* P9.14 = ball U14 = gpmc_a2 = EHRPWM1A khi mode 6 */
/* Control Module offset: 0x848 */
/* CONF_GPMC_A2: MODE=6, RXACTIVE=0, PU/PD=0 */
#define CONF_GPMC_A2_PWM  0x06  /* mode 6 = EHRPWM1A, output */
```

```dts
/* DT pinmux cho EHRPWM1A t?i P9.14 */
ehrpwm1_pins: ehrpwm1_pins {
    pinctrl-single,pins = <
        AM33XX_PADCONF(AM335X_PIN_GPMC_A2, PIN_OUTPUT_PULLDOWN, MUX_MODE6) /* P9.14 */
        AM33XX_PADCONF(AM335X_PIN_GPMC_A3, PIN_OUTPUT_PULLDOWN, MUX_MODE6) /* P9.16 */
    >;
};
```

> **Ngu?n**: BeagleboneBlackP9HeaderTable.pdf (P9.14 row); spruh73q.pdf Chapter 9 (Control Module pad conf)

---
- Hi?u PWM framework trong Linux kernel
- Ki?n tr�c `pwm_chip`, `pwm_ops`
- AM335x EHRPWM (Enhanced High Resolution PWM)
- Vi?t PWM driver v� s? d?ng PWM t? consumer driver

---

## 2. AM335x PWM

### 2.1 PWM modules

AM335x c� 3 EHRPWM module, m?i module 2 output:

| Module | Base Address | Output A | Output B | Ngu?n: spruh73q.pdf |
|--------|-------------|----------|----------|---------------------|
| EHRPWM0 | 0x48300200 | P9.22 | P9.21 | Chapter 15 |
| EHRPWM1 | 0x48302200 | P9.14 | P9.16 | Chapter 15 |
| EHRPWM2 | 0x48304200 | P8.19 | P8.13 | Chapter 15 |

### 2.2 EHRPWM Registers

| Register | Offset | M� t? |
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

### 2.3 PWM ho?t d?ng

```
        TBPRD
  --------+
          �     +---------
          �     �
  --------+-----+
        CMPA
       (duty)

  Period = TBPRD / TBCLK
  Duty   = CMPA / TBPRD � 100%
```

---

## 3. Linux PWM Framework

### 3.1 Ki?n tr�c

```
+-----------------------------------------+
� PWM Consumer (LED, motor, buzzer)       �
�  pwm_get() / devm_pwm_get()            �
�  pwm_apply_state()                      �
+-----------------------------------------�
� PWM Core (drivers/pwm/core.c)           �
�  pwmchip_add() / pwmchip_remove()       �
+-----------------------------------------�
� PWM Provider Driver (pwm_chip + ops)    �
�  .apply(), .get_state()                 �
+-----------------------------------------�
� Hardware (EHRPWM registers)             �
+-----------------------------------------+
```

### 3.2 C?u tr�c ch�nh

```c
/* PWM chip - provider driver dang k� */
struct pwm_chip {
    struct device      *dev;
    const struct pwm_ops *ops;
    int                 npwm;    /* S? channel */
};

/* PWM ops - callback t? provider */
struct pwm_ops {
    int (*apply)(struct pwm_chip *chip, struct pwm_device *pwm,
                  const struct pwm_state *state);
    int (*get_state)(struct pwm_chip *chip, struct pwm_device *pwm,
                      struct pwm_state *state);
};

/* PWM state - tr?ng th�i PWM */
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

	/* T�nh period (cycles) = period_ns * clk_rate / 1e9 */
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

### 5.2 T? sysfs

```bash
# Export PWM channel
echo 0 > /sys/class/pwm/pwmchip0/export

# C?u h�nh
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

## 6. Ki?n th?c c?t l�i

1. **`pwm_chip`** qu?n l� N channel, dang k� qua `devm_pwmchip_add()`
2. **`pwm_ops.apply()`** nh?n `pwm_state` (period, duty_cycle, enabled) t�nh b?ng nanosecond
3. **EHRPWM**: TBPRD = period, CMPA = duty, AQCTLA = action qualifier
4. **Consumer API**: `devm_pwm_get()`, `pwm_apply_state()`
5. **Sysfs interface**: `/sys/class/pwm/pwmchipX/`
6. **DT consumer**: `pwms = <phandle channel period flags>`

---

## 7. C�u h?i ki?m tra

1. `pwm_state.period` v� `duty_cycle` t�nh b?ng don v? g�?
2. EHRPWM register n�o quy?t d?nh duty cycle?
3. S? kh�c nhau gi?a `pwm_apply_state()` (consumer) v� `pwm_ops.apply()` (provider)?
4. T?i sao AQCTLA = 0x0012 s? t?o PWM output?
5. PWM consumer trong DT khai b�o nhu th? n�o?

---

## 8. Chu?n b? cho b�i sau

B�i ti?p theo: **B�i 25 - ADC Driver (IIO Subsystem)**

Ta s? h?c IIO framework chi ti?t: `iio_dev`, `iio_chan_spec`, v� AM335x TSC_ADC subsystem.
