# Bài 13 - GPIO Subsystem tổng quan

## 1. Mục tiêu bài học
- Hiểu gpiolib architecture trong Linux kernel
- Nắm vai trò gpio_chip: GPIO controller driver abstraction
- Phân biệt GPIO provider (controller driver) vs GPIO consumer (client driver)
- Hiểu GPIO numbering: legacy integer vs descriptor-based (gpiod)
- Xem GPIO từ sysfs và debugfs

---

## 2. GPIO Subsystem Architecture

### 2.1 Tổng quan:

```
┌────────────────────────────────────────────────┐
│              Userspace                          │
│   /sys/class/gpio/    libgpiod    /dev/gpiochip│
└───────────────────────┬────────────────────────┘
                        │
┌───────────────────────┴────────────────────────┐
│              GPIO Subsystem (gpiolib)           │
│                                                 │
│  ┌──────────────┐    ┌──────────────────────┐  │
│  │ GPIO Consumer│    │ GPIO Provider        │  │
│  │ (gpiod API)  │    │ (gpio_chip driver)   │  │
│  │              │    │                      │  │
│  │ LED driver   │    │ AM335x GPIO driver   │  │
│  │ Button driver│    │ (omap-gpio.c)        │  │
│  │ SPI CS pin   │    │                      │  │
│  └──────────────┘    └──────────────────────┘  │
└─────────────────────────────────────────────────┘
                        │
┌───────────────────────┴────────────────────────┐
│              Hardware                           │
│   GPIO0 (0x44E07000)  GPIO1 (0x4804C000)       │
│   GPIO2 (0x481AC000)  GPIO3 (0x481AE000)       │
└─────────────────────────────────────────────────┘
```

### 2.2 Hai layer:

1. **GPIO Provider** (gpio_chip driver): Viết bởi SoC vendor (TI). Biết cách đọc/ghi thanh ghi GPIO AM335x.
2. **GPIO Consumer** (gpiod API): Driver bạn viết. Dùng GPIO mà không cần biết register-level.

---

## 3. `struct gpio_chip`

### 3.1 Cấu trúc chính:

```c
#include <linux/gpio/driver.h>

struct gpio_chip {
    const char *label;          /* Tên controller */
    struct device *parent;
    int base;                   /* GPIO number bắt đầu (-1 = dynamic) */
    u16 ngpio;                  /* Số GPIO pin quản lý */

    /* Callback functions */
    int (*direction_input)(struct gpio_chip *gc, unsigned int offset);
    int (*direction_output)(struct gpio_chip *gc, unsigned int offset, int val);
    int (*get)(struct gpio_chip *gc, unsigned int offset);
    void (*set)(struct gpio_chip *gc, unsigned int offset, int val);
    int (*get_direction)(struct gpio_chip *gc, unsigned int offset);

    /* Interrupt support */
    int (*to_irq)(struct gpio_chip *gc, unsigned int offset);
};
```

### 3.2 Callback functions:

| Callback | Mô tả |
|----------|-------|
| `direction_input()` | Đặt pin thành input |
| `direction_output()` | Đặt pin thành output + ghi giá trị ban đầu |
| `get()` | Đọc giá trị pin (0/1) |
| `set()` | Ghi giá trị pin (0/1) |
| `get_direction()` | Xem pin đang input hay output |
| `to_irq()` | Map GPIO offset → Linux IRQ number |

### 3.3 Ví dụ: AM335x GPIO chip (đơn giản hóa):

```c
/* drivers/gpio/gpio-omap.c (simplified) */
struct omap_gpio {
    void __iomem *base;
    struct gpio_chip chip;
};

static int omap_gpio_get(struct gpio_chip *gc, unsigned int offset)
{
    struct omap_gpio *omap = gpiochip_get_data(gc);
    return (readl(omap->base + GPIO_DATAIN) >> offset) & 1;
}

static void omap_gpio_set(struct gpio_chip *gc, unsigned int offset, int val)
{
    struct omap_gpio *omap = gpiochip_get_data(gc);
    if (val)
        writel(1 << offset, omap->base + GPIO_SETDATAOUT);
    else
        writel(1 << offset, omap->base + GPIO_CLEARDATAOUT);
}

static int omap_gpio_direction_output(struct gpio_chip *gc,
                                       unsigned int offset, int val)
{
    struct omap_gpio *omap = gpiochip_get_data(gc);
    u32 oe = readl(omap->base + GPIO_OE);
    oe &= ~(1 << offset);
    writel(oe, omap->base + GPIO_OE);
    omap_gpio_set(gc, offset, val);
    return 0;
}
```

---

## 4. GPIO Numbering

### 4.1 Legacy integer-based (deprecated):

```c
/* Cách cũ: dùng global GPIO number */
int gpio_num = 1 * 32 + 21;  /* GPIO1_21 = 53 */
gpio_request(gpio_num, "my_led");
gpio_direction_output(gpio_num, 1);
gpio_set_value(gpio_num, 1);
gpio_free(gpio_num);
```

**Công thức**: `linux_gpio_number = module * 32 + bit`

### 4.2 Descriptor-based (khuyên dùng):

```c
/* Cách mới: dùng GPIO descriptor */
struct gpio_desc *led_gpio;
led_gpio = devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_LOW);
gpiod_set_value(led_gpio, 1);
```

→ Bài 15 sẽ dạy chi tiết gpiod API.

---

## 5. Đăng ký GPIO Chip

```c
static int my_gpio_probe(struct platform_device *pdev)
{
    struct my_gpio *mgpio;

    mgpio = devm_kzalloc(&pdev->dev, sizeof(*mgpio), GFP_KERNEL);

    mgpio->chip.label = "my-gpio";
    mgpio->chip.parent = &pdev->dev;
    mgpio->chip.base = -1;              /* Dynamic numbering */
    mgpio->chip.ngpio = 32;
    mgpio->chip.direction_input = my_direction_input;
    mgpio->chip.direction_output = my_direction_output;
    mgpio->chip.get = my_get;
    mgpio->chip.set = my_set;

    return devm_gpiochip_add_data(&pdev->dev, &mgpio->chip, mgpio);
}
```

`devm_gpiochip_add_data()` đăng ký gpio_chip vào gpiolib. Từ đây, kernel biết có thêm 32 GPIO pin.

---

## 6. Xem GPIO từ sysfs / debugfs

### 6.1 Sysfs (legacy):

```bash
# Export GPIO1_21 (53)
echo 53 > /sys/class/gpio/export

# Đặt direction
echo out > /sys/class/gpio/gpio53/direction

# Ghi giá trị
echo 1 > /sys/class/gpio/gpio53/value

# Đọc giá trị
cat /sys/class/gpio/gpio53/value

# Unexport
echo 53 > /sys/class/gpio/unexport
```

### 6.2 Debugfs:

```bash
cat /sys/kernel/debug/gpio
# gpiochip0: GPIOs 0-31, parent: 44e07000.gpio, 44e07000.gpio:
#  gpio-6   (                    |led0             ) out hi
#
# gpiochip1: GPIOs 32-63, parent: 4804c000.gpio, 4804c000.gpio:
#  gpio-53  (                    |usr0             ) out lo
```

### 6.3 libgpiod (userspace, khuyên dùng):

```bash
# Cài đặt
apt install gpiod

# Liệt kê GPIO chips
gpiodetect

# Xem GPIO lines
gpioinfo gpiochip1

# Đọc GPIO
gpioget gpiochip1 21

# Ghi GPIO
gpioset gpiochip1 21=1
```

---

## 7. AM335x GPIO trong kernel

AM335x có 4 GPIO module, mỗi module 32 pin, driver: `drivers/gpio/gpio-omap.c`

```
gpiochip0: GPIO0 (0x44E07000) — pin 0-31
gpiochip1: GPIO1 (0x4804C000) — pin 32-63
gpiochip2: GPIO2 (0x481AC000) — pin 64-95
gpiochip3: GPIO3 (0x481AE000) — pin 96-127
```

Device Tree cho GPIO1:
```dts
gpio1: gpio@4804c000 {
    compatible = "ti,omap4-gpio";
    reg = <0x4804c000 0x1000>;
    interrupts = <98>;
    gpio-controller;
    #gpio-cells = <2>;
    interrupt-controller;
    #interrupt-cells = <2>;
};
```

Nguồn: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 25 - GPIO

---

## 8. Kiến thức cốt lõi sau bài này

1. **gpiolib** = kernel subsystem quản lý tất cả GPIO
2. **gpio_chip** = abstraction cho GPIO controller, chứa callback functions
3. **Provider** (gpio_chip driver) vs **Consumer** (gpiod user)
4. **Descriptor-based** (gpiod) thay thế legacy integer-based GPIO
5. **`devm_gpiochip_add_data()`** đăng ký controller
6. AM335x có 4 GPIO module × 32 pin = 128 GPIO

---

## 9. Câu hỏi kiểm tra

1. `gpio_chip` đại diện cho cái gì? Ai implement nó?
2. Provider và consumer trong GPIO subsystem khác nhau thế nào?
3. GPIO1_21 tương ứng Linux GPIO number bao nhiêu?
4. Tại sao `base = -1` khi gọi `gpiochip_add()`?
5. Liệt kê 3 cách xem trạng thái GPIO trên BBB.

---

## 10. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 14 - GPIO Platform Driver cho AM335x**

Ta sẽ tự viết gpio_chip driver cho AM335x, implement direction/get/set ops bằng ioremap GPIO registers.
