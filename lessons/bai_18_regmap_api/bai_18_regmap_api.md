# Bài 18 - regmap API — Truy cập Register thông minh

## Mục tiêu
- Hiểu tại sao dùng `regmap` thay vì `readl/writel` trực tiếp
- Nắm kiến trúc regmap: `regmap_config`, `regmap_read/write`, `regmap_update_bits`
- Áp dụng regmap vào driver AM335x thực tế trên BBB
- Biết dùng `regmap_field` cho bit-field access an toàn

---

## 0. BBB Connection

Bài này dùng **GPIO1 (USR LED0-3)** và **DMTIMER2** làm ví dụ thực hành regmap.

| Hardware | Base Address | BBB Component |
|----------|-------------|---------------|
| GPIO1 | `0x4804C000` | USR LED0 (GPIO1_21, gpio53), USR LED1 (GPIO1_22, gpio54) |
| DMTIMER2 | `0x48040000` | Timer interrupt demo |

> Tham chiếu phần cứng BBB: Xem [gpio.md](../mapping/gpio.md), [timer.md](../mapping/timer.md)

---

## 1. Tại sao cần regmap?

### 1.1 Vấn đề với readl/writel thuần

```c
/* Driver GPIO cũ (bài 12) dùng readl/writel */
static int my_gpio_direction_output(struct gpio_chip *gc, unsigned offset, int val)
{
    struct my_gpio *gpio = gpiochip_get_data(gc);
    u32 reg = readl(gpio->base + GPIO_OE);   /* Đọc */
    reg &= ~BIT(offset);                      /* Sửa bit */
    writel(reg, gpio->base + GPIO_OE);        /* Ghi */
    return 0;
}
```

**Nhược điểm:**
1. **Không thread-safe**: nếu 2 thread cùng read-modify-write → race condition
2. **Không cache**: mỗi lần đọc đều qua bus → chậm
3. **Không validation**: ghi sai offset → crash, khó debug
4. **Code lặp**: pattern `readl → modify → writel` lặp đi lặp lại
5. **Không portable**: nếu muốn chuyển sang I2C/SPI regmap → phải viết lại

### 1.2 regmap giải quyết tất cả

| Tính năng | readl/writel | regmap |
|-----------|------------|--------|
| Thread-safe | Không (tự lock) | Có (internal lock) |
| Cache registers | Không | Có (configurable) |
| Validation | Không | Có (readable/writable/volatile tables) |
| Read-modify-write | Tự code | `regmap_update_bits()` atomic |
| Bit-field access | Tự shift/mask | `regmap_field_read/write()` |
| Debug (debugfs) | Không | Tự động tạo debugfs entries |
| Bus abstraction | Chỉ MMIO | MMIO, I2C, SPI cùng API |

---

## 2. Kiến trúc regmap

```
┌─────────────────────────────────────────────────┐
│                   Driver Code                    │
│                                                  │
│   regmap_read()  regmap_write()                  │
│   regmap_update_bits()                           │
│   regmap_field_read() regmap_field_write()        │
└──────────────────┬──────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────┐
│              regmap core                         │
│  ┌──────────┐ ┌──────────┐ ┌──────────────────┐│
│  │  Cache    │ │  Lock    │ │  Validation      ││
│  │  (rbtree/ │ │  (mutex/ │ │  (readable/      ││
│  │   flat)   │ │   spin)  │ │   writable/      ││
│  └──────────┘ └──────────┘ │   volatile table) ││
│                             └──────────────────┘│
└──────────────────┬──────────────────────────────┘
                   │
┌──────────────────▼──────────────────────────────┐
│            Bus-specific backend                  │
│  ┌──────┐  ┌──────┐  ┌──────┐  ┌─────────────┐│
│  │ MMIO │  │ I2C  │  │ SPI  │  │ Custom      ││
│  │readl │  │i2c_  │  │spi_  │  │(any bus)    ││
│  │writel│  │xfer  │  │sync  │  │             ││
│  └──────┘  └──────┘  └──────┘  └─────────────┘│
└─────────────────────────────────────────────────┘
```

---

## 3. regmap_config — Cấu hình cốt lõi

```c
#include <linux/regmap.h>

/* Ví dụ: GPIO1 registers trên AM335x */
static const struct regmap_config am335x_gpio_regmap_config = {
    /* Kích thước mỗi register (AM335x: 32-bit = 4 bytes) */
    .reg_bits = 32,
    .val_bits = 32,
    .reg_stride = 4,         /* Khoảng cách giữa các register addresses */

    /* Kích thước register map (GPIO: 0x000 - 0x198) */
    .max_register = 0x198,

    /* Cache: KHÔNG cache cho GPIO vì DATAIN thay đổi liên tục */
    .cache_type = REGCACHE_NONE,

    /* Validation tables (optional nhưng recommended) */
    .readable_reg = am335x_gpio_readable,
    .writeable_reg = am335x_gpio_writeable,
    .volatile_reg = am335x_gpio_volatile,
};

/* Validation: register nào đọc được? */
static bool am335x_gpio_readable(struct device *dev, unsigned int reg)
{
    switch (reg) {
    case 0x000: /* GPIO_REVISION */
    case 0x010: /* GPIO_SYSCONFIG */
    case 0x114: /* GPIO_SYSSTATUS */
    case 0x130: /* GPIO_CTRL */
    case 0x134: /* GPIO_OE */
    case 0x138: /* GPIO_DATAIN */
    case 0x13C: /* GPIO_DATAOUT */
        return true;
    default:
        return (reg >= 0x024 && reg <= 0x03C) || /* IRQ regs */
               (reg >= 0x140 && reg <= 0x154) || /* detect/debounce */
               (reg == 0x190 || reg == 0x194);   /* set/clear dataout */
    }
}

/* Validation: register nào ghi được? */
static bool am335x_gpio_writeable(struct device *dev, unsigned int reg)
{
    switch (reg) {
    case 0x000: /* GPIO_REVISION — read-only */
    case 0x114: /* GPIO_SYSSTATUS — read-only */
    case 0x138: /* GPIO_DATAIN — read-only */
        return false;
    default:
        return am335x_gpio_readable(dev, reg);
    }
}

/* Volatile: register nào thay đổi mà không cần CPU ghi?
   (Quan trọng cho cache: volatile registers luôn đọc từ hardware) */
static bool am335x_gpio_volatile(struct device *dev, unsigned int reg)
{
    switch (reg) {
    case 0x138: /* GPIO_DATAIN — external pin changes */
    case 0x02C: /* GPIO_IRQSTATUS_0 — set by hardware */
    case 0x114: /* GPIO_SYSSTATUS */
        return true;
    default:
        return false;
    }
}
```

---

## 4. Tạo regmap và sử dụng trong driver

### 4.1 Tạo regmap MMIO trong probe()

```c
#include <linux/regmap.h>
#include <linux/platform_device.h>

struct am335x_gpio_dev {
    struct regmap *regmap;
    struct gpio_chip gc;
};

static int am335x_gpio_probe(struct platform_device *pdev)
{
    struct am335x_gpio_dev *gdev;
    void __iomem *base;
    struct resource *res;

    gdev = devm_kzalloc(&pdev->dev, sizeof(*gdev), GFP_KERNEL);
    if (!gdev)
        return -ENOMEM;

    /* Enable clock */
    pm_runtime_enable(&pdev->dev);
    pm_runtime_get_sync(&pdev->dev);

    /* ioremap */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(base))
        return PTR_ERR(base);

    /* Tạo regmap MMIO — thay thế readl/writel trực tiếp */
    gdev->regmap = devm_regmap_init_mmio(&pdev->dev, base,
                                          &am335x_gpio_regmap_config);
    if (IS_ERR(gdev->regmap))
        return PTR_ERR(gdev->regmap);

    /* Verify: đọc revision register */
    unsigned int rev;
    regmap_read(gdev->regmap, 0x000, &rev);  /* GPIO_REVISION */
    dev_info(&pdev->dev, "GPIO revision: 0x%08x\n", rev);

    /* ... register gpio_chip ... */
    return 0;
}
```

### 4.2 API cơ bản

```c
/* Đọc register */
unsigned int val;
regmap_read(regmap, GPIO_OE, &val);           /* offset 0x134 */

/* Ghi register */
regmap_write(regmap, GPIO_CTRL, 0x00);        /* Module enable */

/* Read-Modify-Write ATOMIC (thay vì readl + mask + writel) */
/* Set bit 21 = 0 trong GPIO_OE (output cho LED0) */
regmap_update_bits(regmap, GPIO_OE,
                   BIT(21),    /* mask: bit cần thay đổi */
                   0);         /* value: giá trị mới cho bit đó */

/* Set bit 21 = 1 trong GPIO_OE (input) */
regmap_update_bits(regmap, GPIO_OE,
                   BIT(21),    /* mask */
                   BIT(21));   /* value */

/* Ghi 1 vào bit cụ thể (write-1-to-set semantics) */
regmap_write(regmap, GPIO_SETDATAOUT, BIT(21));    /* LED0 ON */
regmap_write(regmap, GPIO_CLEARDATAOUT, BIT(21));  /* LED0 OFF */
```

### 4.3 So sánh trước/sau regmap

```c
/* === TRƯỚC (readl/writel) — KHÔNG thread-safe === */
static int old_direction_output(struct gpio_chip *gc, unsigned off, int val)
{
    struct my_gpio *g = gpiochip_get_data(gc);
    u32 oe = readl(g->base + GPIO_OE);   /* Race condition! */
    oe &= ~BIT(off);
    writel(oe, g->base + GPIO_OE);       /* Có thể overwrite bit khác */
    return 0;
}

/* === SAU (regmap) — Thread-safe, validated === */
static int new_direction_output(struct gpio_chip *gc, unsigned off, int val)
{
    struct my_gpio *g = gpiochip_get_data(gc);
    /* Atomic: lock + read + modify + write + unlock */
    regmap_update_bits(g->regmap, GPIO_OE, BIT(off), 0);
    return 0;
}
```

---

## 5. regmap_field — Truy cập Bit-Field an toàn

Khi register có nhiều bit-fields (như EHRPWM TBCTL), dùng `regmap_field` để truy cập từng field mà không cần tự shift/mask.

### 5.1 Ví dụ: EHRPWM TBCTL register

```
TBCTL register (offset 0x00) — Time-Base Control:
┌────┬────────┬──────┬───────┬────────┬──────┬────────┐
│ 15 │ 14     │ 13   │ 12:10 │ 9:7    │ 6:4  │ 1:0    │
│FREE│SOFT    │PHSDIR│CLKDIV │HSPCLKDIV│SWFSYNC│CTRMODE│
└────┴────────┴──────┴───────┴────────┴──────┴────────┘
```

```c
#include <linux/regmap.h>

/* Define fields theo vị trí bit trong TRM */
#define EHRPWM_TBCTL         0x00

/* regmap_field definitions */
static const struct reg_field tbctl_ctrmode = REG_FIELD(EHRPWM_TBCTL, 0, 1);
static const struct reg_field tbctl_hspclkdiv = REG_FIELD(EHRPWM_TBCTL, 7, 9);
static const struct reg_field tbctl_clkdiv = REG_FIELD(EHRPWM_TBCTL, 10, 12);
static const struct reg_field tbctl_free_soft = REG_FIELD(EHRPWM_TBCTL, 14, 15);

struct am335x_pwm {
    struct regmap *regmap;
    struct regmap_field *f_ctrmode;
    struct regmap_field *f_hspclkdiv;
    struct regmap_field *f_clkdiv;
    struct regmap_field *f_free_soft;
    struct pwm_chip chip;
};

/* Trong probe(): allocate fields */
static int am335x_pwm_probe(struct platform_device *pdev)
{
    struct am335x_pwm *pwm;
    /* ... alloc, ioremap, create regmap ... */

    /* Allocate regmap_field cho mỗi bit-field */
    pwm->f_ctrmode = devm_regmap_field_alloc(&pdev->dev, pwm->regmap,
                                              tbctl_ctrmode);
    pwm->f_hspclkdiv = devm_regmap_field_alloc(&pdev->dev, pwm->regmap,
                                                tbctl_hspclkdiv);
    pwm->f_clkdiv = devm_regmap_field_alloc(&pdev->dev, pwm->regmap,
                                             tbctl_clkdiv);
    /* ... */
    return 0;
}

/* Trong ops: đọc/ghi field riêng biệt — không cần shift/mask thủ công */
static int am335x_pwm_apply(struct pwm_chip *chip, struct pwm_device *p,
                             const struct pwm_state *state)
{
    struct am335x_pwm *pwm = container_of(chip, struct am335x_pwm, chip);

    /* Set counter mode = Up-count (giá trị 0) */
    regmap_field_write(pwm->f_ctrmode, 0);

    /* Set prescaler: HSPCLKDIV = 1 (/2), CLKDIV = 0 (/1) */
    regmap_field_write(pwm->f_hspclkdiv, 1);
    regmap_field_write(pwm->f_clkdiv, 0);

    /* Đọc counter mode hiện tại */
    unsigned int mode;
    regmap_field_read(pwm->f_ctrmode, &mode);
    dev_dbg(&chip->dev, "Counter mode: %u\n", mode);

    return 0;
}
```

---

## 6. regmap cho I2C / SPI devices

Cùng API, khác bus — đây là sức mạnh lớn nhất của regmap.

### 6.1 I2C regmap (ví dụ TMP102 sensor trên BBB I2C2)

```c
/* TMP102: 16-bit registers, 8-bit addresses */
static const struct regmap_config tmp102_regmap_config = {
    .reg_bits = 8,        /* Register address = 8-bit */
    .val_bits = 16,       /* Register value = 16-bit (big-endian) */
    .val_format_endian = REGMAP_ENDIAN_BIG,
    .max_register = 0x03, /* 4 registers: TEMP, CONF, TLOW, THIGH */
    .cache_type = REGCACHE_RBTREE,  /* Cache CONF, TLOW, THIGH */
    .volatile_reg = tmp102_volatile, /* TEMP register is volatile */
};

static bool tmp102_volatile(struct device *dev, unsigned int reg)
{
    return (reg == 0x00); /* Temperature register changes with hardware */
}

/* Trong i2c_driver probe */
static int tmp102_probe(struct i2c_client *client)
{
    struct regmap *regmap;

    /* Tạo regmap cho I2C — CÙNG API với MMIO! */
    regmap = devm_regmap_init_i2c(client, &tmp102_regmap_config);
    if (IS_ERR(regmap))
        return PTR_ERR(regmap);

    /* Đọc temperature — cùng regmap_read(), bên dưới là i2c_transfer */
    unsigned int raw_temp;
    regmap_read(regmap, 0x00, &raw_temp);

    int temp_mc = ((int16_t)raw_temp >> 4) * 625 / 10; /* milli-Celsius */
    dev_info(&client->dev, "Temperature: %d.%03d°C\n",
             temp_mc / 1000, temp_mc % 1000);

    return 0;
}
```

### 6.2 SPI regmap (ví dụ SPI register device)

```c
static const struct regmap_config my_spi_regmap_config = {
    .reg_bits = 8,
    .val_bits = 8,
    .max_register = 0xFF,
    .cache_type = REGCACHE_FLAT,
    .read_flag_mask = 0x80,    /* SPI read: set MSB of address byte */
};

static int my_spi_probe(struct spi_device *spi)
{
    struct regmap *regmap;

    /* Tạo regmap cho SPI — CÙNG API! */
    regmap = devm_regmap_init_spi(spi, &my_spi_regmap_config);
    /* Bên dưới: spi_write_then_read() */

    unsigned int id;
    regmap_read(regmap, 0x00, &id);  /* WHO_AM_I register */
    return 0;
}
```

---

## 7. regmap Cache — Tối ưu performance

```c
/* Cache types */
.cache_type = REGCACHE_NONE,    /* Luôn đọc từ hardware (default) */
.cache_type = REGCACHE_RBTREE,  /* Cache bằng red-black tree (nhiều register) */
.cache_type = REGCACHE_FLAT,    /* Cache bằng array (ít register, nhanh) */
.cache_type = REGCACHE_MAPLE,   /* Maple tree (kernel 6.1+) */

/* regmap_cache_sync: ghi tất cả cached values ra hardware */
/* Hữu ích khi resume từ suspend */
static int my_driver_resume(struct device *dev)
{
    struct my_dev *mydev = dev_get_drvdata(dev);
    /* Restore tất cả register từ cache */
    regmap_cache_sync(mydev->regmap);
    return 0;
}
```

---

## 8. regmap debugfs — Debug register dễ dàng

```bash
# Khi dùng regmap, kernel tự tạo debugfs entries:
mount -t debugfs none /sys/kernel/debug

# Xem tất cả regmap devices
ls /sys/kernel/debug/regmap/

# Xem register dump
cat /sys/kernel/debug/regmap/<device>/registers

# Ví dụ output:
# 000: 50600801    <- GPIO_REVISION
# 010: 00000000    <- GPIO_SYSCONFIG
# 134: ffe1ffff    <- GPIO_OE (bit 21-24 = 0 → USR LEDs output)
# 138: 00600000    <- GPIO_DATAIN (current pin levels)

# Ghi register (debug only!)
echo "134 ffe1ffff" > /sys/kernel/debug/regmap/<device>/registers
```

---

## 9. Ví dụ hoàn chỉnh: GPIO driver với regmap trên BBB

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/gpio/driver.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>

/* AM335x GPIO registers — TRM Chapter 25 */
#define GPIO_REVISION       0x000
#define GPIO_SYSCONFIG      0x010
#define GPIO_CTRL           0x130
#define GPIO_OE             0x134
#define GPIO_DATAIN         0x138
#define GPIO_DATAOUT        0x13C
#define GPIO_CLEARDATAOUT   0x190
#define GPIO_SETDATAOUT     0x194

struct am335x_gpio_rm {
    struct regmap *regmap;
    struct gpio_chip gc;
};

/* regmap config */
static bool am335x_gpio_volatile_reg(struct device *dev, unsigned int reg)
{
    return (reg == GPIO_DATAIN) || (reg == 0x02C); /* DATAIN + IRQSTATUS */
}

static const struct regmap_config am335x_gpio_regmap_cfg = {
    .reg_bits = 32,
    .val_bits = 32,
    .reg_stride = 4,
    .max_register = GPIO_SETDATAOUT,
    .cache_type = REGCACHE_FLAT,
    .volatile_reg = am335x_gpio_volatile_reg,
};

/* gpio_chip ops sử dụng regmap */
static int rm_gpio_direction_input(struct gpio_chip *gc, unsigned off)
{
    struct am335x_gpio_rm *g = gpiochip_get_data(gc);
    /* GPIO_OE bit[off] = 1 → input (atomic update) */
    return regmap_update_bits(g->regmap, GPIO_OE, BIT(off), BIT(off));
}

static int rm_gpio_direction_output(struct gpio_chip *gc, unsigned off, int val)
{
    struct am335x_gpio_rm *g = gpiochip_get_data(gc);
    /* Set value first */
    regmap_write(g->regmap,
                 val ? GPIO_SETDATAOUT : GPIO_CLEARDATAOUT,
                 BIT(off));
    /* GPIO_OE bit[off] = 0 → output */
    return regmap_update_bits(g->regmap, GPIO_OE, BIT(off), 0);
}

static int rm_gpio_get(struct gpio_chip *gc, unsigned off)
{
    struct am335x_gpio_rm *g = gpiochip_get_data(gc);
    unsigned int val;
    regmap_read(g->regmap, GPIO_DATAIN, &val);
    return !!(val & BIT(off));
}

static void rm_gpio_set(struct gpio_chip *gc, unsigned off, int val)
{
    struct am335x_gpio_rm *g = gpiochip_get_data(gc);
    regmap_write(g->regmap,
                 val ? GPIO_SETDATAOUT : GPIO_CLEARDATAOUT,
                 BIT(off));
}

static int am335x_gpio_rm_probe(struct platform_device *pdev)
{
    struct am335x_gpio_rm *g;
    void __iomem *base;
    unsigned int rev;

    g = devm_kzalloc(&pdev->dev, sizeof(*g), GFP_KERNEL);
    if (!g)
        return -ENOMEM;

    pm_runtime_enable(&pdev->dev);
    pm_runtime_get_sync(&pdev->dev);

    base = devm_platform_ioremap_resource(pdev, 0);
    if (IS_ERR(base))
        return PTR_ERR(base);

    /* Tạo regmap MMIO */
    g->regmap = devm_regmap_init_mmio(&pdev->dev, base,
                                       &am335x_gpio_regmap_cfg);
    if (IS_ERR(g->regmap))
        return PTR_ERR(g->regmap);

    /* Đọc revision */
    regmap_read(g->regmap, GPIO_REVISION, &rev);
    dev_info(&pdev->dev, "GPIO revision: 0x%08x\n", rev);

    /* Enable module: CTRL bit 0 = 0 */
    regmap_update_bits(g->regmap, GPIO_CTRL, BIT(0), 0);

    /* Register gpio_chip */
    g->gc.label = "am335x-gpio-regmap";
    g->gc.parent = &pdev->dev;
    g->gc.owner = THIS_MODULE;
    g->gc.base = -1;
    g->gc.ngpio = 32;
    g->gc.direction_input = rm_gpio_direction_input;
    g->gc.direction_output = rm_gpio_direction_output;
    g->gc.get = rm_gpio_get;
    g->gc.set = rm_gpio_set;

    return devm_gpiochip_add_data(&pdev->dev, &g->gc, g);
}

static const struct of_device_id am335x_gpio_rm_of_match[] = {
    { .compatible = "my,am335x-gpio-regmap-learn" },
    { }
};
MODULE_DEVICE_TABLE(of, am335x_gpio_rm_of_match);

static struct platform_driver am335x_gpio_rm_driver = {
    .probe = am335x_gpio_rm_probe,
    .driver = {
        .name = "am335x-gpio-regmap-learn",
        .of_match_table = am335x_gpio_rm_of_match,
    },
};
module_platform_driver(am335x_gpio_rm_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AM335x GPIO driver with regmap - Bài 18");
```

### Device Tree cho ví dụ trên

```dts
/* Test: bind vào GPIO1 (USR LEDs) */
&am33xx_pinmux {
    gpio_regmap_pins: gpio-regmap-pins {
        pinctrl-single,pins = <
            0x054 0x07  /* P9.12 conf_gpmc_a0 → GPIO1_28 MODE7 */
        >;
    };
};

/ {
    my_gpio_regmap: my-gpio-regmap@4804c000 {
        compatible = "my,am335x-gpio-regmap-learn";
        reg = <0x4804c000 0x1000>;
        pinctrl-names = "default";
        pinctrl-0 = <&gpio_regmap_pins>;
    };
};
```

---

## 10. Khi nào dùng regmap vs readl/writel?

| Tình huống | Chọn | Lý do |
|---|---|---|
| Driver đơn giản, ít register | readl/writel | Overhead regmap không cần thiết |
| Nhiều register, cần thread-safety | **regmap** | `regmap_update_bits()` atomic |
| I2C/SPI device registers | **regmap** | `devm_regmap_init_i2c/spi` |
| Cần cache (suspend/resume) | **regmap** | `regmap_cache_sync()` |
| Bit-field phức tạp | **regmap_field** | Không cần shift/mask thủ công |
| Debug register values | **regmap** | Tự tạo debugfs |
| Performance-critical ISR | readl/writel | regmap lock overhead |
| TI upstream drivers | **regmap** | Convention trong kernel tree |

**Quy tắc chung**: Dùng regmap cho driver mới. Chỉ dùng readl/writel trong ISR handler hoặc boot-critical path.

---

## Câu hỏi kiểm tra

1. **regmap_update_bits** nhận 3 tham số (reg, mask, val). Để set GPIO_OE bit 21 = 0, mask và val là gì?
2. Tại sao `GPIO_DATAIN` phải được đánh dấu `volatile` trong regmap config?
3. `regmap_field_write(f_ctrmode, 2)` — regmap tự làm gì bên trong? (shift, mask, read-modify-write?)
4. Khi driver dùng `REGCACHE_FLAT`, hàm `regmap_read()` đọc từ đâu: cache hay hardware?
5. `devm_regmap_init_i2c()` và `devm_regmap_init_mmio()` dùng cùng `regmap_read()` — kernel phân biệt bus bằng cách nào?
