# Bài 23 - UART/Serial Driver

## 1. Mục tiêu bài học
- Hiểu Serial Core framework trong Linux kernel
- Kiến trúc `uart_driver`, `uart_port`, `uart_ops`
- AM335x UART controller registers
- Viết UART platform driver cơ bản

---

## 2. AM335x UART

### 2.1 UART modules

AM335x có 6 UART module:

| Module | Base Address | BBB usage | Nguồn: spruh73q.pdf |
|--------|-------------|-----------|---------------------|
| UART0 | 0x44E09000 | Debug serial (J1 header) | Chapter 19 |
| UART1 | 0x48022000 | Available (P9.24/P9.26) | Chapter 19 |
| UART2 | 0x48024000 | Available (P9.21/P9.22) | Chapter 19 |
| UART3 | 0x481A6000 | Available | Chapter 19 |
| UART4 | 0x481A8000 | Available (P9.11/P9.13) | Chapter 19 |
| UART5 | 0x481AA000 | Available (P8.37/P8.38) | Chapter 19 |

### 2.2 Thanh ghi quan trọng (16550 compatible)

| Register | Offset | Mô tả |
|----------|--------|-------|
| THR | 0x00 | Transmit Holding Register (write) |
| RHR | 0x00 | Receive Holding Register (read) |
| IER | 0x04 | Interrupt Enable Register |
| IIR | 0x08 | Interrupt Identification Register (read) |
| FCR | 0x08 | FIFO Control Register (write) |
| LCR | 0x0C | Line Control Register |
| MCR | 0x10 | Modem Control Register |
| LSR | 0x14 | Line Status Register |
| MSR | 0x18 | Modem Status Register |
| DLL | 0x00 | Divisor Latch Low (LCR[7]=1) |
| DLH | 0x04 | Divisor Latch High (LCR[7]=1) |
| MDR1 | 0x20 | Mode Definition Register 1 |
| SYSC | 0x54 | System Configuration Register |
| SYSS | 0x58 | System Status Register |

### 2.3 LSR bits

| Bit | Tên | Mô tả |
|-----|-----|-------|
| 0 | DR | Data Ready (RX có data) |
| 1 | OE | Overrun Error |
| 5 | THRE | THR Empty (TX ready) |
| 6 | TEMT | Transmitter Empty |

---

## 3. Serial Core Framework

### 3.1 Kiến trúc

```
┌─────────────────────────────────────────┐
│ Userspace: /dev/ttyS0, /dev/ttyO1      │
├─────────────────────────────────────────┤
│ TTY Layer (tty_driver, tty_operations)  │
├─────────────────────────────────────────┤
│ Serial Core (uart_driver, uart_port)    │
│  - uart_register_driver()               │
│  - uart_add_one_port()                  │
├─────────────────────────────────────────┤
│ Low-level UART driver (uart_ops)        │
│  - tx_empty, start_tx, stop_tx          │
│  - startup, shutdown, set_termios       │
├─────────────────────────────────────────┤
│ Hardware (AM335x UART registers)        │
└─────────────────────────────────────────┘
```

### 3.2 struct uart_driver

```c
struct uart_driver {
    struct module  *owner;
    const char     *driver_name;    /* "my_uart" */
    const char     *dev_name;       /* "ttyMY" → /dev/ttyMY0 */
    int             major;          /* 0 = auto */
    int             minor;
    int             nr;             /* Số port tối đa */
    struct console *cons;
};
```

### 3.3 struct uart_port

```c
struct uart_port {
    void __iomem   *membase;       /* ioremap address */
    resource_size_t mapbase;       /* Physical address */
    unsigned int    irq;
    unsigned int    uartclk;       /* Input clock (48MHz cho AM335x) */
    unsigned int    fifosize;
    unsigned int    type;          /* PORT_16550A, etc */
    unsigned int    line;          /* Port number */
    const struct uart_ops *ops;
    struct device  *dev;
};
```

### 3.4 struct uart_ops

```c
struct uart_ops {
    /* TX */
    unsigned int (*tx_empty)(struct uart_port *);
    void (*start_tx)(struct uart_port *);
    void (*stop_tx)(struct uart_port *);

    /* RX */
    void (*stop_rx)(struct uart_port *);

    /* Control */
    void (*startup)(struct uart_port *);  /* Open port */
    void (*shutdown)(struct uart_port *); /* Close port */
    void (*set_termios)(struct uart_port *, struct ktermios *new,
                         const struct ktermios *old);

    /* Status */
    unsigned int (*get_mctrl)(struct uart_port *);
    void (*set_mctrl)(struct uart_port *, unsigned int);

    /* Config */
    const char *(*type)(struct uart_port *);
    void (*config_port)(struct uart_port *, int);
    void (*release_port)(struct uart_port *);
    int (*request_port)(struct uart_port *);
};
```

---

## 4. Mini UART Driver

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/clk.h>

#define DRIVER_NAME "learn-uart"
#define DEV_NAME    "ttyLRN"
#define MAX_PORTS   1

/* AM335x UART register offsets */
#define UART_THR    0x00
#define UART_RHR    0x00
#define UART_IER    0x04
#define UART_LCR    0x0C
#define UART_MCR    0x10
#define UART_LSR    0x14
#define UART_MDR1   0x20
#define UART_DLL    0x00
#define UART_DLH    0x04

/* LSR bits */
#define LSR_DR      BIT(0)
#define LSR_THRE    BIT(5)
#define LSR_TEMT    BIT(6)

struct learn_uart {
	struct uart_port port;
	struct clk *clk;
};

static inline u32 learn_uart_read(struct uart_port *port, u32 off)
{
	return readl(port->membase + off);
}

static inline void learn_uart_write(struct uart_port *port, u32 off, u32 val)
{
	writel(val, port->membase + off);
}

static unsigned int learn_tx_empty(struct uart_port *port)
{
	return (learn_uart_read(port, UART_LSR) & LSR_TEMT) ? TIOCSER_TEMT : 0;
}

static void learn_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	/* Không dùng modem control */
}

static unsigned int learn_get_mctrl(struct uart_port *port)
{
	return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}

static void learn_stop_tx(struct uart_port *port)
{
	u32 ier = learn_uart_read(port, UART_IER);
	ier &= ~BIT(1);  /* Disable THR interrupt */
	learn_uart_write(port, UART_IER, ier);
}

static void learn_start_tx(struct uart_port *port)
{
	u32 lsr;
	struct circ_buf *xmit = &port->state->xmit;

	/* Gửi data từ circular buffer */
	while (!uart_circ_empty(xmit)) {
		lsr = learn_uart_read(port, UART_LSR);
		if (!(lsr & LSR_THRE))
			break;
		learn_uart_write(port, UART_THR, xmit->buf[xmit->tail]);
		uart_xmit_advance(port, 1);
	}

	if (uart_circ_empty(xmit))
		learn_stop_tx(port);
}

static void learn_stop_rx(struct uart_port *port)
{
	u32 ier = learn_uart_read(port, UART_IER);
	ier &= ~BIT(0);  /* Disable RHR interrupt */
	learn_uart_write(port, UART_IER, ier);
}

static irqreturn_t learn_uart_irq(int irq, void *dev_id)
{
	struct uart_port *port = dev_id;
	u32 lsr;

	lsr = learn_uart_read(port, UART_LSR);

	/* RX */
	while (lsr & LSR_DR) {
		u8 ch = learn_uart_read(port, UART_RHR);
		uart_insert_char(port, lsr, 0, ch, TTY_NORMAL);
		lsr = learn_uart_read(port, UART_LSR);
	}
	tty_flip_buffer_push(&port->state->port);

	/* TX */
	if (lsr & LSR_THRE)
		learn_start_tx(port);

	return IRQ_HANDLED;
}

static int learn_startup(struct uart_port *port)
{
	int ret;

	ret = request_irq(port->irq, learn_uart_irq, 0, DRIVER_NAME, port);
	if (ret)
		return ret;

	/* Enable RX interrupt */
	learn_uart_write(port, UART_IER, BIT(0));

	return 0;
}

static void learn_shutdown(struct uart_port *port)
{
	learn_uart_write(port, UART_IER, 0);
	free_irq(port->irq, port);
}

static void learn_set_termios(struct uart_port *port,
                               struct ktermios *termios,
                               const struct ktermios *old)
{
	unsigned int baud;
	u32 lcr = 0;
	u16 divisor;

	/* Tính baud rate */
	baud = uart_get_baud_rate(port, termios, old, 300, 115200);
	divisor = port->uartclk / (16 * baud);

	/* Word length */
	switch (termios->c_cflag & CSIZE) {
	case CS5: lcr = 0x00; break;
	case CS6: lcr = 0x01; break;
	case CS7: lcr = 0x02; break;
	case CS8:
	default:  lcr = 0x03; break;
	}

	/* Stop bits */
	if (termios->c_cflag & CSTOPB)
		lcr |= BIT(2);

	/* Parity */
	if (termios->c_cflag & PARENB) {
		lcr |= BIT(3);
		if (!(termios->c_cflag & PARODD))
			lcr |= BIT(4);
	}

	uart_port_lock_irq(port);

	/* Set divisor (LCR[7] = 1 to access DLL/DLH) */
	learn_uart_write(port, UART_LCR, lcr | BIT(7));
	learn_uart_write(port, UART_DLL, divisor & 0xFF);
	learn_uart_write(port, UART_DLH, (divisor >> 8) & 0xFF);
	learn_uart_write(port, UART_LCR, lcr);

	/* UART 16x mode */
	learn_uart_write(port, UART_MDR1, 0x00);

	uart_update_timeout(port, termios->c_cflag, baud);

	uart_port_unlock_irq(port);
}

static const char *learn_type(struct uart_port *port)
{
	return "LEARN-UART";
}

static void learn_config_port(struct uart_port *port, int flags)
{
	port->type = PORT_16550A;
}

static void learn_release_port(struct uart_port *port) { }
static int learn_request_port(struct uart_port *port) { return 0; }

static const struct uart_ops learn_uart_ops = {
	.tx_empty    = learn_tx_empty,
	.set_mctrl   = learn_set_mctrl,
	.get_mctrl   = learn_get_mctrl,
	.stop_tx     = learn_stop_tx,
	.start_tx    = learn_start_tx,
	.stop_rx     = learn_stop_rx,
	.startup     = learn_startup,
	.shutdown    = learn_shutdown,
	.set_termios = learn_set_termios,
	.type        = learn_type,
	.config_port = learn_config_port,
	.release_port = learn_release_port,
	.request_port = learn_request_port,
};

static struct uart_driver learn_uart_driver = {
	.owner       = THIS_MODULE,
	.driver_name = DRIVER_NAME,
	.dev_name    = DEV_NAME,
	.major       = 0,
	.minor       = 0,
	.nr          = MAX_PORTS,
};

static int learn_uart_probe(struct platform_device *pdev)
{
	struct learn_uart *luart;
	struct resource *res;
	int ret;

	luart = devm_kzalloc(&pdev->dev, sizeof(*luart), GFP_KERNEL);
	if (!luart)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	luart->port.membase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(luart->port.membase))
		return PTR_ERR(luart->port.membase);

	luart->port.mapbase = res->start;
	luart->port.irq = platform_get_irq(pdev, 0);
	if (luart->port.irq < 0)
		return luart->port.irq;

	luart->port.uartclk = 48000000;  /* 48 MHz */
	luart->port.fifosize = 64;
	luart->port.ops = &learn_uart_ops;
	luart->port.dev = &pdev->dev;
	luart->port.line = 0;

	platform_set_drvdata(pdev, luart);

	ret = uart_add_one_port(&learn_uart_driver, &luart->port);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add port: %d\n", ret);
		return ret;
	}

	dev_info(&pdev->dev, "UART port added: %s%d\n", DEV_NAME, luart->port.line);
	return 0;
}

static int learn_uart_remove(struct platform_device *pdev)
{
	struct learn_uart *luart = platform_get_drvdata(pdev);

	uart_remove_one_port(&learn_uart_driver, &luart->port);
	return 0;
}

static const struct of_device_id learn_uart_of_match[] = {
	{ .compatible = "learn,uart" },
	{ },
};
MODULE_DEVICE_TABLE(of, learn_uart_of_match);

static struct platform_driver learn_uart_platform_driver = {
	.probe = learn_uart_probe,
	.remove = learn_uart_remove,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = learn_uart_of_match,
	},
};

static int __init learn_uart_init(void)
{
	int ret;

	ret = uart_register_driver(&learn_uart_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&learn_uart_platform_driver);
	if (ret)
		uart_unregister_driver(&learn_uart_driver);

	return ret;
}

static void __exit learn_uart_exit(void)
{
	platform_driver_unregister(&learn_uart_platform_driver);
	uart_unregister_driver(&learn_uart_driver);
}

module_init(learn_uart_init);
module_exit(learn_uart_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Learning UART driver for AM335x");
```

---

## 5. Device Tree

```dts
&am33xx_pinmux {
    uart1_pins: uart1_pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_UART1_TXD, PIN_OUTPUT, MUX_MODE0)
            AM33XX_PADCONF(AM335X_PIN_UART1_RXD, PIN_INPUT_PULLUP, MUX_MODE0)
        >;
    };
};
```

---

## 6. Data Flow

```
Userspace write("Hello")
       │
       ▼
  TTY Layer → circ_buf
       │
       ▼
  uart_ops->start_tx()
       │
       ▼
  Đọc từ circ_buf → ghi vào THR register
       │
       ▼
  TX shift register → TX pin

RX pin → RX shift register
       │
       ▼
  IRQ: LSR[DR]=1
       │
       ▼
  Đọc RHR register
       │
       ▼
  uart_insert_char() → tty_flip_buffer_push()
       │
       ▼
  Userspace read()
```

---

## 7. Kiến thức cốt lõi

1. **Serial Core** 3 tầng: `uart_driver` → `uart_port` → `uart_ops`
2. **`uart_register_driver()`** phải gọi trước `uart_add_one_port()`
3. **Baud rate divisor**: `divisor = uartclk / (16 × baud)`
4. **LCR[7]=1** để access DLL/DLH (divisor latch)
5. **IRQ handler**: đọc LSR kiểm DR (RX) và THRE (TX)
6. **AM335x UART** là 16550-compatible với FIFO 64 byte
7. **`uart_insert_char()` + `tty_flip_buffer_push()`** đẩy RX data lên TTY layer

---

## 8. Câu hỏi kiểm tra

1. Tại sao phải gọi `uart_register_driver()` trước `uart_add_one_port()`?
2. Giải thích cách tính baud rate divisor cho 115200 baud với clock 48MHz.
3. LSR register bit nào cho biết có thể ghi THR? Bit nào cho biết có data RX?
4. `tty_flip_buffer_push()` làm gì?
5. Tại sao `learn_uart_init()` không dùng `module_platform_driver()` macro?

---

## 9. Chuẩn bị cho bài sau

Bài tiếp theo: **Bài 24 - PWM Subsystem**

Ta sẽ học PWM framework: `pwm_chip`, `pwm_ops`, và EHRPWM trên AM335x.
