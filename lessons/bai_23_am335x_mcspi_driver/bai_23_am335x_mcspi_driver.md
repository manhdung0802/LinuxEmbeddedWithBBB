# B�i 23 - AM335x McSPI & SPI Subsystem

> **Tham chi?u ph?n c?ng BBB:** Xem [spi.md](../mapping/spi.md)

## 0. BBB Connection � SPI tr�n BBB
> ⚠️ **AN TOÀN ĐIỆN:**
> - SPI trên BBB là **3.3V logic** — KHÔNG nối slave 5V trực tiếp
> - Dùng bi-directional level-shifter nếu slave là 5V (VD: TXB0104)
> - SPI0 shares pins với HDMI — cần disable HDMI trong DT nếu dùng SPI0
```
BeagleBone Black � SPI0 wiring
------------------------------
 AM335x McSPI0 @ 0x48030000, IRQ 65

  BBB Pin   Signal       Pad Offset   Mode 0
  ---------------------------------------------
  P9.17     SPI0_CS0     0x95C        spi0_cs0
  P9.18     SPI0_D1      0x958        spi0_d1 (MOSI)
  P9.21     SPI0_D0      0x954        spi0_d0 (MISO)
  P9.22     SPI0_SCLK    0x950        spi0_sclk

  K?t n?i MCP3008 (v� d?):
  +---------+         +------------+
  �   BBB   �         �  MCP3008   �
  �  P9.22  +-- CLK --� 13 CLK     �
  �  P9.18  +-- MOSI -� 11 DIN     �
  �  P9.21  +-- MISO -� 12 DOUT    �
  �  P9.17  +-- CS ---� 10 CS/SHDN �
  �  3.3V   +---------� 16 VDD     �
  �  GND    +---------�  9 DGND    �
  +---------+         +------------+

  Clock enable: CM_PER_SPI0_CLKCTRL @ 0x44E0004C
```

> **Luu �:** SPI1 chia ch�n v?i HDMI (P9.28-P9.31). C?n disable HDMI overlay tru?c khi d�ng SPI1.

---

## 1. M?c ti�u b�i h?c
- Hi?u ki?n tr�c SPI subsystem trong Linux kernel
- Vi?t SPI client driver d�ng `spi_driver`
- Hi?u co ch? `spi_transfer` v� `spi_message`
- C?u h�nh SPI qua Device Tree
- AM335x McSPI controller registers

---

## 2. SPI Protocol co b?n

### 2.1 T�n hi?u SPI

```
Master (AM335x)                 Slave (Device)
   SCLK ---------------------? SCLK
   MOSI (D1) ----------------? MOSI (Data In)
   MISO (D0) ?----------------MISO (Data Out)
   CS0  ---------------------? CS (active low)
```

### 2.2 4 mode SPI

| Mode | CPOL | CPHA | M� t? |
|------|------|------|-------|
| 0    | 0    | 0    | Clock idle low, sample on rising edge |
| 1    | 0    | 1    | Clock idle low, sample on falling edge |
| 2    | 1    | 0    | Clock idle high, sample on falling edge |
| 3    | 1    | 1    | Clock idle high, sample on rising edge |

### 2.3 AM335x McSPI

AM335x c� 2 module McSPI:

| Module | Base Address | Channels | Ngu?n: spruh73q.pdf |
|--------|-------------|----------|---------------------|
| McSPI0 | 0x48030000 | 2 (ch0, ch1) | Chapter 24 |
| McSPI1 | 0x481A0000 | 2 (ch0, ch1) | Chapter 24 |

Tr�n BBB:
- **SPI0**: P9.17 (CS0), P9.18 (D1/MOSI), P9.21 (D0/MISO), P9.22 (SCLK)
- **SPI1**: P9.28 (CS0), P9.29 (D0), P9.30 (D1), P9.31 (SCLK)

---

## 3. Linux SPI Framework

### 3.1 Ki?n tr�c

```
+---------------------------------------------+
�           Userspace (/dev/spidevX.Y)        �
+---------------------------------------------�
�        SPI Core (drivers/spi/spi.c)         �
�   - Qu?n l� bus, device matching            �
�   - spi_sync(), spi_async()                 �
+---------------------------------------------�
�  SPI Master      �     SPI Client Driver    �
�  (Controller)    �     (Protocol Driver)    �
�  omap2_mcspi.c   �     spi_driver           �
+---------------------------------------------�
�              Hardware (McSPI)                �
+---------------------------------------------+
```

### 3.2 C?u tr�c quan tr?ng

```c
/* SPI device - d?i di?n m?t slave device */
struct spi_device {
    struct device       dev;
    struct spi_controller *controller;
    u32                 max_speed_hz;
    u8                  chip_select;
    u8                  bits_per_word;
    u32                 mode;       /* SPI_MODE_0..3, SPI_CS_HIGH, etc */
    int                 irq;
};

/* SPI driver - driver cho slave device */
struct spi_driver {
    int  (*probe)(struct spi_device *spi);
    void (*remove)(struct spi_device *spi);
    struct device_driver driver;
    const struct spi_device_id *id_table;
};

/* SPI transfer - m?t giao d?ch truy?n/nh?n */
struct spi_transfer {
    const void *tx_buf;     /* Buffer g?i */
    void       *rx_buf;     /* Buffer nh?n */
    unsigned    len;        /* S? byte */
    unsigned    speed_hz;   /* Override speed */
    u16         delay_usecs;
    u8          bits_per_word;
    u8          cs_change;  /* Deassert CS sau transfer n�y */
};

/* SPI message - nh�m nhi?u transfer */
struct spi_message {
    struct list_head  transfers;  /* Danh s�ch spi_transfer */
    struct spi_device *spi;
    unsigned          is_dma_mapped:1;
    void (*complete)(void *context);
    void              *context;
    int               status;
};
```

---

## 4. SPI API

### 4.1 G?i/nh?n co b?n

```c
/* G?i data */
int spi_write(struct spi_device *spi, const void *buf, size_t len);

/* Nh?n data */
int spi_read(struct spi_device *spi, void *buf, size_t len);

/* G?i r?i nh?n (2 transfer) */
int spi_write_then_read(struct spi_device *spi,
    const void *txbuf, unsigned n_tx,
    void *rxbuf, unsigned n_rx);
```

### 4.2 Transfer ph?c t?p

```c
struct spi_transfer t[2];
struct spi_message m;

memset(t, 0, sizeof(t));

/* Transfer 1: g?i command */
t[0].tx_buf = &cmd;
t[0].len = 1;

/* Transfer 2: nh?n data */
t[1].rx_buf = &data;
t[1].len = 2;

spi_message_init(&m);
spi_message_add_tail(&t[0], &m);
spi_message_add_tail(&t[1], &m);

ret = spi_sync(spi, &m);
```

### 4.3 �?ng b? vs B?t d?ng b?

```c
/* Synchronous - block cho t?i khi xong */
int spi_sync(struct spi_device *spi, struct spi_message *msg);

/* Asynchronous - return ngay, callback khi xong */
int spi_async(struct spi_device *spi, struct spi_message *msg);
```

---

## 5. SPI Driver template

```c
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/of.h>

struct my_spi_dev {
	struct spi_device *spi;
	struct mutex lock;
	u8 tx_buf[32];
	u8 rx_buf[32];
};

/* �?c register c?a SPI device */
static int my_spi_read_reg(struct my_spi_dev *dev, u8 reg, u8 *val)
{
	/* Nhi?u SPI device: bit 7 = 1 = read */
	u8 cmd = reg | 0x80;
	struct spi_transfer t[2] = {
		{
			.tx_buf = &cmd,
			.len = 1,
		},
		{
			.rx_buf = val,
			.len = 1,
		},
	};
	struct spi_message m;

	spi_message_init(&m);
	spi_message_add_tail(&t[0], &m);
	spi_message_add_tail(&t[1], &m);

	return spi_sync(dev->spi, &m);
}

/* Ghi register */
static int my_spi_write_reg(struct my_spi_dev *dev, u8 reg, u8 val)
{
	u8 buf[2] = { reg & 0x7F, val };

	return spi_write(dev->spi, buf, 2);
}

static int my_spi_probe(struct spi_device *spi)
{
	struct my_spi_dev *dev;
	u8 id;

	dev = devm_kzalloc(&spi->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->spi = spi;
	mutex_init(&dev->lock);
	spi_set_drvdata(spi, dev);

	/* C?u h�nh SPI */
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	spi->max_speed_hz = 1000000;  /* 1 MHz */
	spi_setup(spi);

	/* �?c WHO_AM_I register d? verify */
	my_spi_read_reg(dev, 0x0F, &id);
	dev_info(&spi->dev, "Device ID: 0x%02x\n", id);

	return 0;
}

static void my_spi_remove(struct spi_device *spi)
{
	dev_info(&spi->dev, "SPI device removed\n");
}

static const struct of_device_id my_spi_of_match[] = {
	{ .compatible = "learn,my-spi-dev" },
	{ },
};
MODULE_DEVICE_TABLE(of, my_spi_of_match);

static struct spi_driver my_spi_driver = {
	.driver = {
		.name = "my-spi-dev",
		.of_match_table = my_spi_of_match,
	},
	.probe = my_spi_probe,
	.remove = my_spi_remove,
};

module_spi_driver(my_spi_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SPI driver template");
```

---

## 6. Device Tree cho SPI

```dts
&am33xx_pinmux {
    spi0_pins: spi0_pins {
        pinctrl-single,pins = <
            AM33XX_PADCONF(AM335X_PIN_SPI0_SCLK, PIN_INPUT_PULLUP, MUX_MODE0)
            AM33XX_PADCONF(AM335X_PIN_SPI0_D0, PIN_INPUT_PULLUP, MUX_MODE0)
            AM33XX_PADCONF(AM335X_PIN_SPI0_D1, PIN_OUTPUT_PULLUP, MUX_MODE0)
            AM33XX_PADCONF(AM335X_PIN_SPI0_CS0, PIN_OUTPUT_PULLUP, MUX_MODE0)
        >;
    };
};

&spi0 {
    status = "okay";
    pinctrl-names = "default";
    pinctrl-0 = <&spi0_pins>;

    my_device@0 {
        compatible = "learn,my-spi-dev";
        reg = <0>;                  /* chip_select = 0 */
        spi-max-frequency = <1000000>;
        spi-cpol;                   /* CPOL = 1 (optional) */
        spi-cpha;                   /* CPHA = 1 (optional) */
    };
};
```

### 6.1 DT properties cho SPI device

| Property | M� t? |
|----------|-------|
| `reg` | Chip select number (0, 1) |
| `spi-max-frequency` | Max clock frequency |
| `spi-cpol` | Set CPOL = 1 |
| `spi-cpha` | Set CPHA = 1 |
| `spi-cs-high` | CS active high |
| `spi-3wire` | 3-wire SPI mode |

---

## 7. Ki?n th?c c?t l�i

1. **`spi_driver`** register qua `module_spi_driver()` macro
2. **`spi_transfer` + `spi_message`** cho giao d?ch ph?c t?p
3. **`spi_sync()`** block, **`spi_async()`** non-block
4. **`spi_write_then_read()`** helper cho command-response pattern
5. **DT binding**: `reg` = chip_select, `spi-max-frequency` = clock speed
6. **AM335x McSPI**: 2 module, m?i module 2 channel, full-duplex

---

## 8. C�u h?i ki?m tra

1. SPI mode 0, 1, 2, 3 kh�c nhau ? d�u?
2. S? kh�c nhau gi?a `spi_sync()` v� `spi_async()`?
3. T?i sao `spi_transfer` c?n c? `tx_buf` v� `rx_buf`?
4. Device Tree property `reg` trong SPI context nghia g�?
5. `spi_write_then_read()` d�ng m?y `spi_transfer` b�n trong?

---

## 9. Chu?n b? cho b�i sau

B�i ti?p theo: **B�i 22 - SPI Driver th?c h�nh**

Ta s? vi?t driver ho�n ch?nh cho MCP3008 ADC (8-channel, 10-bit) qua SPI.
