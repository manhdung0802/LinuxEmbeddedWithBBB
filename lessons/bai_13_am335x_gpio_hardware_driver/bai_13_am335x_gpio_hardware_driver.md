# B�i 13 - AM335x GPIO Hardware & GPIO Subsystem

> **Tham chi?u ph?n c?ng BBB:** Xem [gpio.md](../mapping/gpio.md) vu{00E0} [pinmux.md](../mapping/pinmux.md)

## 0. BBB Connection � GPIO tr�n BeagleBone Black


> :warning: **AN TOAN DIEN:**
> - AM335x GPIO la **3.3V logic** - KHONG NOI 5V vao bat ky chan P8/P9 nao
> - Moi chan GPIO source/sink toi da **6mA** - dung MOSFET/transistor de dieu khien tai lon (relay, motor)
> - KHONG noi nguoc chieu - co the hong vinh vien SoC
> - Luon kiem tra pinmux truoc khi dung (tranh xung dot eMMC/HDMI)

Tru?c khi d?c b?t k? thanh ghi n�o, h�y nh�n v�o board BBB th?c t?:

### 0.1 Onboard hardware g?n v?i GPIO

```
BeagleBone Black (nh�n t? tr�n xu?ng)
                         +-------------+
   P9 -----------------? �  AM335x     � ?---- P8
                         �  SoC        �
   USR LED0 ?-- GPIO1_21-�  GPIO1      �
   USR LED1 ?-- GPIO1_22-�  0x4804C000 �
   USR LED2 ?-- GPIO1_23-�             �
   USR LED3 ?-- GPIO1_24-�             �
                         �             �
   BTN S2   --? GPIO0_27-�  GPIO0      �
   (Boot/User)           �  0x44E07000 �
                         +-------------+
```

### 0.2 B?ng mapping GPIO ? BBB Component

| Signal | GPIO Module | Bit | Linux GPIO# | V? tr� | Ghi ch� |
|--------|------------|-----|------------|--------|---------|
| USR LED0 | GPIO1 | 21 | **53** | Onboard, g?n Ethernet | Active HIGH |
| USR LED1 | GPIO1 | 22 | **54** | Onboard | Active HIGH |
| USR LED2 | GPIO1 | 23 | **55** | Onboard | Active HIGH |
| USR LED3 | GPIO1 | 24 | **56** | Onboard | Active HIGH |
| User Button S2 | GPIO0 | 27 | **27** | Onboard, g?n P9 | Active LOW, c� pull-up |
| P9.12 | GPIO1 | 28 | **60** | Header P9 pin 12 | D�ng cho test external |
| P9.15 | GPIO1 | 16 | **48** | Header P9 pin 15 | D�ng cho test external |
| P8.3 | GPIO1 | 6 | **38** | Header P8 pin 3 | D�ng cho test external |

> **C�ng th?c**: Linux GPIO# = (module_number � 32) + bit_number  
> V� d?: GPIO1_21 ? 1�32 + 21 = **53**

### 0.3 GPIO clock ph?i du?c enable tru?c khi d�ng

```c
/* GPIO1 clock enable (CM_PER) � TRM trang 179 */
#define CM_PER_BASE       0x44E00000
#define CM_PER_GPIO1_CLK  0xAC   /* offset trong CM_PER */
/* Bit[1:0] = MODULEMODE = 0x2 (ENABLE) */
```

> **Ngu?n**: BBB_SRM_C.pdf (b?ng USR LED mapping); spruh73q.pdf Chapter 9 (GPIO registers)

---

## 1. M?c ti�u b�i h?c
- Hi?u gpiolib architecture trong Linux kernel
- N?m vai tr� gpio_chip: GPIO controller driver abstraction
- Ph�n bi?t GPIO provider (controller driver) vs GPIO consumer (client driver)
- Hi?u GPIO numbering: legacy integer vs descriptor-based (gpiod)
- Xem GPIO t? sysfs v� debugfs

---

## 2. GPIO Subsystem Architecture

### 2.1 T?ng quan:

```
+------------------------------------------------+
�              Userspace                          �
�   /sys/class/gpio/    libgpiod    /dev/gpiochip�
+------------------------------------------------+
                        �
+------------------------------------------------+
�              GPIO Subsystem (gpiolib)           �
�                                                 �
�  +--------------+    +----------------------+  �
�  � GPIO Consumer�    � GPIO Provider        �  �
�  � (gpiod API)  �    � (gpio_chip driver)   �  �
�  �              �    �                      �  �
�  � LED driver   �    � AM335x GPIO driver   �  �
�  � Button driver�    � (omap-gpio.c)        �  �
�  � SPI CS pin   �    �                      �  �
�  +--------------+    +----------------------+  �
+-------------------------------------------------+
                        �
+------------------------------------------------+
�              Hardware                           �
�   GPIO0 (0x44E07000)  GPIO1 (0x4804C000)       �
�   GPIO2 (0x481AC000)  GPIO3 (0x481AE000)       �
+-------------------------------------------------+
```

### 2.2 Hai layer:

1. **GPIO Provider** (gpio_chip driver): Vi?t b?i SoC vendor (TI). Bi?t c�ch d?c/ghi thanh ghi GPIO AM335x.
2. **GPIO Consumer** (gpiod API): Driver b?n vi?t. D�ng GPIO m� kh�ng c?n bi?t register-level.

---

## 3. `struct gpio_chip`

### 3.1 C?u tr�c ch�nh:

```c
#include <linux/gpio/driver.h>

struct gpio_chip {
    const char *label;          /* T�n controller */
    struct device *parent;
    int base;                   /* GPIO number b?t d?u (-1 = dynamic) */
    u16 ngpio;                  /* S? GPIO pin qu?n l� */

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

| Callback | M� t? |
|----------|-------|
| `direction_input()` | �?t pin th�nh input |
| `direction_output()` | �?t pin th�nh output + ghi gi� tr? ban d?u |
| `get()` | �?c gi� tr? pin (0/1) |
| `set()` | Ghi gi� tr? pin (0/1) |
| `get_direction()` | Xem pin dang input hay output |
| `to_irq()` | Map GPIO offset ? Linux IRQ number |

### 3.3 V� d?: AM335x GPIO chip (don gi?n h�a):

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
/*
 * V� d? th?c h�nh tr�n BBB:
 * GPIO1_21 = USR LED0 onboard BeagleBone Black
 * Linux GPIO# = 1 * 32 + 21 = 53
 */
int gpio_num = 1 * 32 + 21;  /* GPIO1_21 = 53 ? USR LED0 tr�n BBB */
gpio_request(gpio_num, "bbb_usr_led0");
gpio_direction_output(gpio_num, 0);  /* output, ban d?u t?t */
gpio_set_value(gpio_num, 1);         /* b?t LED0 */
msleep(500);
gpio_set_value(gpio_num, 0);         /* t?t LED0 */
gpio_free(gpio_num);
```

**C�ng th?c**: `linux_gpio_number = module * 32 + bit`

### 4.2 Descriptor-based (khuy�n d�ng):

```c
/* C�ch m?i: d�ng GPIO descriptor */
struct gpio_desc *led_gpio;
led_gpio = devm_gpiod_get(&pdev->dev, "led", GPIOD_OUT_LOW);
gpiod_set_value(led_gpio, 1);
```

? B�i 15 s? d?y chi ti?t gpiod API.

---

## 5. �ang k� GPIO Chip

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

`devm_gpiochip_add_data()` dang k� gpio_chip v�o gpiolib. T? d�y, kernel bi?t c� th�m 32 GPIO pin.

---

## 6. Xem GPIO t? sysfs / debugfs

### 6.1 Sysfs (legacy) � Test USR LED0 tr�n BBB:

```bash
# === B?t USR LED0 (GPIO1_21 = gpio53) tr�n BBB ===

# Export GPIO1_21
echo 53 > /sys/class/gpio/export

# �?t direction output
echo out > /sys/class/gpio/gpio53/direction

# B?t LED0
echo 1 > /sys/class/gpio/gpio53/value

# T?t LED0
echo 0 > /sys/class/gpio/gpio53/value

# Unexport
echo 53 > /sys/class/gpio/unexport

# === �?c User Button S2 (GPIO0_27 = gpio27) ===
echo 27 > /sys/class/gpio/export
echo in > /sys/class/gpio/gpio27/direction
cat /sys/class/gpio/gpio27/value  # 0 khi nh?n (Active LOW), 1 khi th?
echo 27 > /sys/class/gpio/unexport
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

### 6.3 libgpiod (userspace, khuy�n d�ng):

```bash
# C�i d?t
apt install gpiod

# Li?t k� GPIO chips
gpiodetect

# Xem GPIO lines
gpioinfo gpiochip1

# �?c GPIO
gpioget gpiochip1 21

# Ghi GPIO
gpioset gpiochip1 21=1
```

---

## 7. AM335x GPIO trong kernel

AM335x c� 4 GPIO module, m?i module 32 pin, driver: `drivers/gpio/gpio-omap.c`

```
gpiochip0: GPIO0 (0x44E07000) � pin 0-31
gpiochip1: GPIO1 (0x4804C000) � pin 32-63
gpiochip2: GPIO2 (0x481AC000) � pin 64-95
gpiochip3: GPIO3 (0x481AE000) � pin 96-127
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

Ngu?n: `BBB_docs/datasheets/spruh73q.pdf`, Chapter 25 - GPIO

---

## 8. Ki?n th?c c?t l�i sau b�i n�y

1. **gpiolib** = kernel subsystem qu?n l� t?t c? GPIO
2. **gpio_chip** = abstraction cho GPIO controller, ch?a callback functions
3. **Provider** (gpio_chip driver) vs **Consumer** (gpiod user)
4. **Descriptor-based** (gpiod) thay th? legacy integer-based GPIO
5. **`devm_gpiochip_add_data()`** dang k� controller
6. AM335x c� 4 GPIO module � 32 pin = 128 GPIO

---

## 9. C�u h?i ki?m tra

1. `gpio_chip` d?i di?n cho c�i g�? Ai implement n�?
2. Provider v� consumer trong GPIO subsystem kh�c nhau th? n�o?
3. GPIO1_21 tuong ?ng Linux GPIO number bao nhi�u?
4. T?i sao `base = -1` khi g?i `gpiochip_add()`?
5. Li?t k� 3 c�ch xem tr?ng th�i GPIO tr�n BBB.

---

## 10. Chu?n b? cho b�i sau

B�i ti?p theo: **B�i 14 - GPIO Platform Driver cho AM335x**

Ta s? t? vi?t gpio_chip driver cho AM335x, implement direction/get/set ops b?ng ioremap GPIO registers.
