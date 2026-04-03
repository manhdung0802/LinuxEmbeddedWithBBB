# Bài 12 - Từ Register đến Driver: Phương pháp luận

## Mục tiêu
- Nắm quy trình **có hệ thống** để chuyển đổi mô tả thanh ghi trong TRM thành driver Linux
- Biết cách đọc TRM, xác định register set, và ánh xạ từng register/bit vào đúng kernel API
- Áp dụng ngay vào ví dụ thực tế: GPIO register → gpio_chip ops trên BBB

---

## 1. Vấn đề: Khoảng cách giữa TRM và Kernel API

Khi đọc TRM (spruh73q.pdf), bạn thấy:
```
GPIO_OE register (offset 0x134):
  Bit 21: 0 = output, 1 = input
```

Khi đọc kernel source, bạn thấy:
```c
static int omap_gpio_direction_output(struct gpio_chip *gc, unsigned offset, int value)
```

**Câu hỏi**: Register `GPIO_OE` bit 21 → kernel API `direction_output()` — chuyển đổi như thế nào?

Bài này trả lời câu hỏi đó một cách có hệ thống.

---

## 2. Quy trình 7 bước: TRM → Driver

```
┌──────────────────────────────────────────────────────────────────┐
│  Bước 1: Xác định Peripheral Block trong TRM                    │
│  └── Chapter nào? Base Address? Clock Domain?                    │
│                                                                  │
│  Bước 2: Liệt kê Register Set                                   │
│  └── Tất cả registers, offsets, reset values, R/W access        │
│                                                                  │
│  Bước 3: Phân loại Register theo chức năng                       │
│  └── Config, Data, Status, Interrupt, Clock enable               │
│                                                                  │
│  Bước 4: Xác định Kernel Subsystem tương ứng                    │
│  └── gpio_chip? pwm_chip? uart_port? i2c_adapter? iio_dev?     │
│                                                                  │
│  Bước 5: Ánh xạ Register → Kernel ops                           │
│  └── Mỗi .ops function cần đọc/ghi register nào?               │
│                                                                  │
│  Bước 6: Xác định Init Sequence                                 │
│  └── Thứ tự: enable clock → soft reset → configure → enable     │
│                                                                  │
│  Bước 7: Viết Driver code                                       │
│  └── #define regs → struct → probe → ops → module_platform_drv  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 3. Ví dụ thực hành: GPIO1 trên BBB (USR LED0)

### Bước 1: Xác định Peripheral Block

Mở TRM spruh73q.pdf, Chapter 25 "GPIO":
- **Peripheral**: GPIO1
- **Base Address**: `0x4804C000`
- **Clock**: `CM_PER_GPIO1_CLKCTRL` @ `0x44E000AC`
- **IRQ**: 98
- **BBB Connection**: GPIO1_21 = USR LED0 (onboard, Active HIGH)

### Bước 2: Liệt kê Register Set

| Register | Offset | R/W | Reset Value | Mô tả TRM |
|----------|--------|-----|-------------|-----------|
| GPIO_REVISION | 0x000 | R | 0x50600801 | Module revision |
| GPIO_SYSCONFIG | 0x010 | R/W | 0x0 | System configuration |
| GPIO_IRQSTATUS_RAW_0 | 0x024 | R/W | 0x0 | Raw interrupt status |
| GPIO_IRQSTATUS_0 | 0x02C | R/W | 0x0 | Interrupt status (write-1-to-clear) |
| GPIO_IRQSTATUS_SET_0 | 0x034 | R/W | 0x0 | Enable interrupt (write-1-to-set) |
| GPIO_IRQSTATUS_CLR_0 | 0x03C | R/W | 0x0 | Disable interrupt (write-1-to-clear) |
| GPIO_SYSSTATUS | 0x114 | R | 0x0 | Reset done status |
| GPIO_CTRL | 0x130 | R/W | 0x0 | Module control (disable/enable) |
| **GPIO_OE** | **0x134** | R/W | 0xFFFFFFFF | Output Enable (0=output) |
| **GPIO_DATAIN** | **0x138** | R | — | Data input (read pin level) |
| GPIO_DATAOUT | 0x13C | R/W | 0x0 | Data output |
| GPIO_LEVELDETECT0 | 0x140 | R/W | 0x0 | Low-level detect enable |
| GPIO_LEVELDETECT1 | 0x144 | R/W | 0x0 | High-level detect enable |
| GPIO_RISINGDETECT | 0x148 | R/W | 0x0 | Rising edge detect |
| GPIO_FALLINGDETECT | 0x14C | R/W | 0x0 | Falling edge detect |
| GPIO_DEBOUNCENABLE | 0x150 | R/W | 0x0 | Debounce enable per bit |
| GPIO_DEBOUNCINGTIME | 0x154 | R/W | 0x0 | Debounce time (×31µs) |
| **GPIO_CLEARDATAOUT** | **0x190** | W | — | Clear output pin (write 1) |
| **GPIO_SETDATAOUT** | **0x194** | W | — | Set output pin (write 1) |

> Nguồn: TRM spruh73q.pdf, Chapter 25, Table 25-5

### Bước 3: Phân loại Register theo chức năng

```
┌─────────────────────────────────────────────────────────┐
│  CONFIG registers (setup trong probe/init)              │
│  ├── GPIO_SYSCONFIG : soft reset, idle mode             │
│  ├── GPIO_CTRL      : module enable/disable             │
│  ├── GPIO_OE        : direction (input/output per bit)  │
│  └── GPIO_DEBOUNCE* : debounce config                   │
│                                                         │
│  DATA registers (runtime read/write)                    │
│  ├── GPIO_DATAIN       : read pin level                 │
│  ├── GPIO_SETDATAOUT   : set output HIGH                │
│  └── GPIO_CLEARDATAOUT : set output LOW                 │
│                                                         │
│  INTERRUPT registers                                    │
│  ├── GPIO_IRQSTATUS_0     : pending interrupts (W1C)    │
│  ├── GPIO_IRQSTATUS_SET_0 : enable interrupt            │
│  ├── GPIO_IRQSTATUS_CLR_0 : disable interrupt           │
│  ├── GPIO_RISINGDETECT    : trigger on rising edge      │
│  ├── GPIO_FALLINGDETECT   : trigger on falling edge     │
│  └── GPIO_LEVELDETECT0/1  : trigger on level            │
│                                                         │
│  STATUS registers (read-only)                           │
│  ├── GPIO_REVISION  : hardware version                  │
│  └── GPIO_SYSSTATUS : reset completion                  │
└─────────────────────────────────────────────────────────┘
```

### Bước 4: Xác định Kernel Subsystem → `gpio_chip`

GPIO peripheral → kernel subsystem **gpiolib** → driver implement `struct gpio_chip`.

### Bước 5: Ánh xạ Register → `gpio_chip` ops

Đây là bước quan trọng nhất — **mỗi ops function ánh xạ tới register cụ thể**:

| `gpio_chip` ops | Thanh ghi AM335x | Bit field | Hoạt động |
|---|---|---|---|
| `.direction_input(chip, offset)` | `GPIO_OE` @ 0x134 | Set bit `offset` → 1 | `reg |= BIT(offset)` |
| `.direction_output(chip, offset, val)` | `GPIO_OE` @ 0x134 + `SETDATA/CLEARDATA` | Clear bit `offset` → 0 | `reg &= ~BIT(offset)` rồi set/clear data |
| `.get(chip, offset)` | `GPIO_DATAIN` @ 0x138 | Read bit `offset` | `return (readl(reg) >> offset) & 1` |
| `.set(chip, offset, val)` | `GPIO_SETDATAOUT` @ 0x194 hoặc `GPIO_CLEARDATAOUT` @ 0x190 | Write bit `offset` | `writel(BIT(offset), set_or_clear_reg)` |
| `.get_direction(chip, offset)` | `GPIO_OE` @ 0x134 | Read bit `offset` | 1=input, 0=output |
| `.irq_chip.irq_set_type(d, type)` | `GPIO_RISINGDETECT` @ 0x148 / `GPIO_FALLINGDETECT` @ 0x14C | Set bit | Chọn loại trigger |
| `.irq_chip.irq_mask(d)` | `GPIO_IRQSTATUS_CLR_0` @ 0x03C | Write bit | Mask interrupt |
| `.irq_chip.irq_unmask(d)` | `GPIO_IRQSTATUS_SET_0` @ 0x034 | Write bit | Unmask interrupt |
| `.irq_chip.irq_ack(d)` | `GPIO_IRQSTATUS_0` @ 0x02C | Write-1-to-clear | Acknowledge IRQ |

**Chú ý**: `GPIO_OE` dùng convention **ngược** — bit = 0 là OUTPUT, bit = 1 là INPUT.

### Bước 6: Init Sequence

```
1. Enable clock:
   CM_PER_GPIO1_CLKCTRL (0x44E000AC) = 0x02 (MODULEMODE = ENABLE)
   ↓ Kernel: pm_runtime_get_sync(&pdev->dev)

2. Soft reset (optional):
   GPIO_SYSCONFIG bit 1 = 1 (SOFTRESET)
   Wait GPIO_SYSSTATUS bit 0 = 1 (RESETDONE)

3. Enable module:
   GPIO_CTRL bit 0 = 0 (MODULE_ENABLED)

4. Configure direction:
   GPIO_OE bit[21] = 0 (output cho USR LED0)

5. Set initial value:
   GPIO_CLEARDATAOUT = BIT(21) (LED OFF)

6. (Optional) Configure interrupts:
   GPIO_RISINGDETECT bit[27] = 1 (rising edge cho S2 button)
   GPIO_IRQSTATUS_SET_0 bit[27] = 1 (enable interrupt)
```

### Bước 7: Viết Driver Code

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/gpio/driver.h>
#include <linux/pm_runtime.h>

/* Bước 2: Define register offsets từ TRM */
#define GPIO_OE              0x134
#define GPIO_DATAIN          0x138
#define GPIO_CLEARDATAOUT    0x190
#define GPIO_SETDATAOUT      0x194

struct am335x_gpio {
    void __iomem *base;        /* ioremap'd base address */
    struct gpio_chip gc;
};

/* Bước 5: Implement ops — mỗi function thao tác register cụ thể */

/* .direction_input → set GPIO_OE bit = 1 (input) */
static int am335x_gpio_direction_input(struct gpio_chip *gc, unsigned offset)
{
    struct am335x_gpio *gpio = gpiochip_get_data(gc);
    u32 val = readl(gpio->base + GPIO_OE);   /* Đọc GPIO_OE (0x134) */
    val |= BIT(offset);                       /* Set bit = 1 → input */
    writel(val, gpio->base + GPIO_OE);        /* Ghi lại GPIO_OE */
    return 0;
}

/* .direction_output → clear GPIO_OE bit = 0 (output), rồi set value */
static int am335x_gpio_direction_output(struct gpio_chip *gc, unsigned offset, int value)
{
    struct am335x_gpio *gpio = gpiochip_get_data(gc);
    u32 val;

    /* Set initial output value */
    if (value)
        writel(BIT(offset), gpio->base + GPIO_SETDATAOUT);   /* 0x194 */
    else
        writel(BIT(offset), gpio->base + GPIO_CLEARDATAOUT); /* 0x190 */

    /* Clear OE bit → output */
    val = readl(gpio->base + GPIO_OE);
    val &= ~BIT(offset);
    writel(val, gpio->base + GPIO_OE);
    return 0;
}

/* .get → đọc GPIO_DATAIN bit */
static int am335x_gpio_get(struct gpio_chip *gc, unsigned offset)
{
    struct am335x_gpio *gpio = gpiochip_get_data(gc);
    return !!(readl(gpio->base + GPIO_DATAIN) & BIT(offset)); /* 0x138 */
}

/* .set → ghi GPIO_SETDATAOUT hoặc GPIO_CLEARDATAOUT */
static void am335x_gpio_set(struct gpio_chip *gc, unsigned offset, int value)
{
    struct am335x_gpio *gpio = gpiochip_get_data(gc);
    if (value)
        writel(BIT(offset), gpio->base + GPIO_SETDATAOUT);   /* 0x194 */
    else
        writel(BIT(offset), gpio->base + GPIO_CLEARDATAOUT); /* 0x190 */
}

/* Bước 6: probe — init sequence: clock → ioremap → register gpio_chip */
static int am335x_gpio_probe(struct platform_device *pdev)
{
    struct am335x_gpio *gpio;
    struct resource *res;

    gpio = devm_kzalloc(&pdev->dev, sizeof(*gpio), GFP_KERNEL);
    if (!gpio)
        return -ENOMEM;

    /* Enable clock (Bước 6 init: CM_PER_GPIO1_CLKCTRL) */
    pm_runtime_enable(&pdev->dev);
    pm_runtime_get_sync(&pdev->dev);

    /* ioremap base address (0x4804C000 cho GPIO1) */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    gpio->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(gpio->base))
        return PTR_ERR(gpio->base);

    /* Register gpio_chip — ánh xạ ops names → functions */
    gpio->gc.label = "am335x-gpio";
    gpio->gc.parent = &pdev->dev;
    gpio->gc.owner = THIS_MODULE;
    gpio->gc.base = -1;                /* Auto-assign GPIO numbers */
    gpio->gc.ngpio = 32;               /* 32 GPIOs per bank */
    gpio->gc.direction_input = am335x_gpio_direction_input;
    gpio->gc.direction_output = am335x_gpio_direction_output;
    gpio->gc.get = am335x_gpio_get;
    gpio->gc.set = am335x_gpio_set;

    return devm_gpiochip_add_data(&pdev->dev, &gpio->gc, gpio);
}

/* Device Tree matching */
static const struct of_device_id am335x_gpio_of_match[] = {
    { .compatible = "my,am335x-gpio-learn" },
    { }
};
MODULE_DEVICE_TABLE(of, am335x_gpio_of_match);

static struct platform_driver am335x_gpio_driver = {
    .probe = am335x_gpio_probe,
    .driver = {
        .name = "am335x-gpio-learn",
        .of_match_table = am335x_gpio_of_match,
    },
};
module_platform_driver(am335x_gpio_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AM335x GPIO driver - Bài 12 Register→Driver example");
```

---

## 4. Bảng tổng hợp: Register → Kernel ops cho tất cả peripheral

### 4.1 GPIO

| TRM Register | Kernel ops / API |
|---|---|
| GPIO_OE | `gpio_chip.direction_input()` / `.direction_output()` |
| GPIO_DATAIN | `gpio_chip.get()` |
| GPIO_SETDATAOUT / GPIO_CLEARDATAOUT | `gpio_chip.set()` |
| GPIO_IRQSTATUS_0 | `irq_chip.irq_ack()` |
| GPIO_IRQSTATUS_SET_0 / CLR_0 | `irq_chip.irq_unmask()` / `.irq_mask()` |
| GPIO_RISINGDETECT / FALLINGDETECT | `irq_chip.irq_set_type()` |
| GPIO_DEBOUNCE* | `gpio_chip.set_config()` (debounce) |

### 4.2 I2C

| TRM Register | Kernel ops / API |
|---|---|
| I2C_CON | `i2c_algorithm.master_xfer()` — start/stop/direction |
| I2C_SA | `i2c_algorithm.master_xfer()` — set slave address |
| I2C_CNT | `i2c_algorithm.master_xfer()` — byte count |
| I2C_DATA | `i2c_algorithm.master_xfer()` — read/write data |
| I2C_IRQSTATUS | ISR handler — check XRDY/RRDY/ARDY/NACK |
| I2C_PSC / I2C_SCLL / I2C_SCLH | `probe()` — clock configuration |

### 4.3 SPI (McSPI)

| TRM Register | Kernel ops / API |
|---|---|
| MCSPI_MODULCTRL | `spi_controller.setup()` — master/slave mode |
| MCSPI_CH0CONF | `spi_controller.setup()` — CPOL/CPHA/word length |
| MCSPI_CH0CTRL | `spi_controller.transfer_one()` — enable channel |
| MCSPI_TX0 | `spi_controller.transfer_one()` — write TX data |
| MCSPI_RX0 | `spi_controller.transfer_one()` — read RX data |
| MCSPI_CH0STAT | ISR / polling — check TXS/RXS/EOT |
| MCSPI_IRQSTATUS | ISR handler |

### 4.4 UART

| TRM Register | Kernel ops / API |
|---|---|
| UART_DLL + UART_DLH | `uart_ops.set_termios()` — baud rate |
| UART_LCR | `uart_ops.set_termios()` — word length, parity, stop |
| UART_FCR | `uart_ops.startup()` — FIFO enable/reset |
| UART_IER | `uart_ops.startup()` / `.shutdown()` — interrupt enable |
| UART_THR | `uart_ops.start_tx()` — transmit byte |
| UART_RHR | ISR → `uart_insert_char()` — receive byte |
| UART_LSR | ISR — check TX empty, RX ready, errors |

### 4.5 PWM (EHRPWM)

| TRM Register | Kernel ops / API |
|---|---|
| TBCTL | `pwm_ops.apply()` — time-base control, prescaler |
| TBPRD | `pwm_ops.apply()` — period |
| CMPA / CMPB | `pwm_ops.apply()` — duty cycle |
| AQCTLA / AQCTLB | `pwm_ops.apply()` — action on compare match |
| TBCNT | `pwm_ops.get_state()` — current counter |

### 4.6 Timer (DMTIMER)

| TRM Register | Kernel ops / API |
|---|---|
| TCLR | `clockevent_device.set_state_oneshot/periodic()` — mode |
| TCRR | Timer counter — read for clocksource |
| TLDR | `clockevent_device.set_next_event()` — reload value |
| TISR / TIER | ISR + enable — `request_irq()` handler |
| TWPS | Polling — write-posted status |

### 4.7 ADC (TSC_ADC)

| TRM Register | Kernel ops / API |
|---|---|
| CTRL | `iio_dev` init — ADC enable, step config write protect |
| STEPCONFIG[n] | `probe()` — channel config (input, averaging, mode) |
| STEPDELAY[n] | `probe()` — sample delay |
| FIFO0DATA | `iio_info.read_raw()` — read conversion result |
| IRQSTATUS / IRQENABLE | ISR — step completion notification |

---

## 5. Pattern chung: Cấu trúc Driver từ TRM

Tất cả driver AM335x đều tuân theo pattern này:

```c
/* ======== PHẦN 1: TRM REGISTER DEFINITIONS ======== */
/* Lấy từ TRM Table register offsets */
#define REG_REVISION    0x000
#define REG_SYSCONFIG   0x010
#define REG_CONFIG_A     0x0XX    /* Peripheral-specific */
#define REG_DATA         0x0XX
#define REG_IRQSTATUS    0x0XX
#define REG_IRQENABLE    0x0XX

/* Lấy từ TRM bit field descriptions */
#define CONFIG_A_MODE_MASK    GENMASK(3, 0)
#define CONFIG_A_ENABLE       BIT(15)
#define IRQSTATUS_DONE        BIT(0)

/* ======== PHẦN 2: DEVICE STRUCT ======== */
struct am335x_myperiph {
    void __iomem *base;         /* ioremap'd MMIO base */
    struct clk *fclk;           /* Functional clock */
    int irq;                    /* IRQ number */
    /* Subsystem-specific struct (gpio_chip, pwm_chip, etc.) */
};

/* ======== PHẦN 3: OPS FUNCTIONS ======== */
/* Mỗi function = 1 hoặc vài register operations */
/* Comment: register name + offset + bit field */

/* ======== PHẦN 4: IRQ HANDLER ======== */
/* Đọc IRQSTATUS, xử lý, clear bằng write-1-to-clear */

/* ======== PHẦN 5: PROBE ======== */
/* Clock enable → ioremap → soft reset → configure → register subsystem */

/* ======== PHẦN 6: DT MATCH + MODULE ======== */
/* of_match_table, platform_driver, module_platform_driver() */
```

---

## 6. Checklist khi viết driver mới cho AM335x peripheral

- [ ] Tìm đúng chapter trong TRM cho peripheral
- [ ] Ghi lại base address, clock register, IRQ number
- [ ] Liệt kê tất cả register offsets + bit fields cần dùng
- [ ] Phân loại: Config / Data / Status / Interrupt
- [ ] Xác định kernel subsystem (`gpio_chip`? `pwm_chip`? `uart_port`?)
- [ ] Ánh xạ từng ops function → register(s) cụ thể
- [ ] Xác định init sequence (clock → reset → config → enable)
- [ ] Xác định cleanup sequence (disable → reset → clock off)
- [ ] Viết DT node target BBB (address, IRQ, pinmux, clock)
- [ ] Build, load, test trên BBB hardware thực
- [ ] Verify register values với `devmem2` hoặc `/dev/mem` (debug)

---

## Câu hỏi kiểm tra

1. **GPIO_OE**: Tại sao bit = 0 nghĩa là output chứ không phải input? (Hint: convention của TI)
2. **Register phân loại**: `GPIO_IRQSTATUS_0` thuộc loại Config, Data, Status hay Interrupt?
3. **Ánh xạ**: `pwm_ops.apply()` cần ghi những thanh ghi nào của EHRPWM?
4. **Init sequence**: Nếu quên enable clock trước khi readl() register, chuyện gì xảy ra?
5. **regmap vs readl/writel**: Khi nào nên dùng regmap thay vì trực tiếp readl/writel? (Gợi ý: bài tiếp theo sẽ dạy chi tiết)
