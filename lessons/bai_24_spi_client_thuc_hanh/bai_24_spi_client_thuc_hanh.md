# B�i 24 - SPI Client Driver th?c h�nh

> **Tham chiếu phần cứng BBB:** Xem [spi.md](../mapping/spi.md)

## 0. BBB Connection

> ⚠️ **AN TOÀN ĐIỆN:** SPI BBB = 3.3V logic. Dùng level-shifter cho slave 5V. SPI0 xung đột HDMI. � SPI0 + MCP3008

```
BeagleBone Black � SPI0 + MCP3008 ADC
-------------------------------------
  BBB P9.22 (SCLK) ----- MCP3008 pin 13 (CLK)
  BBB P9.18 (MOSI) ----- MCP3008 pin 11 (DIN)
  BBB P9.21 (MISO) ----- MCP3008 pin 12 (DOUT)
  BBB P9.17 (CS0)  ----- MCP3008 pin 10 (CS/SHDN)
  BBB 3.3V ------------- MCP3008 pin 15+16 (VREF+VDD)
  BBB GND -------------- MCP3008 pin 9+14 (DGND+AGND)
  Potentiometer -------- MCP3008 pin 1 (CH0)
```

### Ki?m tra SPI0 active
```bash
ls /dev/spidev0.*
# /dev/spidev0.0
```

---

## 1. M?c ti�u b�i h?c
- Vi?t driver ho�n ch?nh cho MCP3008 ADC qua SPI
- T�ch h?p IIO subsystem cho ADC readings
- Hi?u SPI protocol c?a MCP3008
- Device Tree + pinmux cho SPI0 tr�n BBB

---

## 2. MCP3008 ADC

### 2.1 Th�ng s?

- Chip: Microchip MCP3008
- Interface: SPI (mode 0), max 3.6 MHz @ 5V
- Resolution: 10-bit
- Channels: 8 single-ended / 4 differential
- Vref: external reference voltage

### 2.2 SPI Protocol

M?i l?n d?c ADC c?n 3 byte:

```
TX: [Start][SGL/DIFF D2 D1 D0 x x x x][x x x x x x x x]
RX: [x x x x x x x x][x x x x x x B9 B8][B7 B6 B5 B4 B3 B2 B1 B0]
```

- Byte 0 TX: 0x01 (start bit)
- Byte 1 TX: (1 << 7) | (channel << 4) - single-ended mode
- Byte 2 TX: don't care
- Result: ((rx[1] & 0x03) << 8) | rx[2]

---

## 3. Driver MCP3008 + IIO

```c
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/iio/iio.h>
#include <linux/of.h>

#define MCP3008_NUM_CHANNELS 8

struct mcp3008_data {
	struct spi_device *spi;
	struct mutex lock;
	u8 tx_buf[3] __aligned(IIO_DMA_MINALIGN);
	u8 rx_buf[3];
};

/* �?c m?t channel ADC */
static int mcp3008_read_channel(struct mcp3008_data *adc, int channel)
{
	struct spi_transfer t = {
		.tx_buf = adc->tx_buf,
		.rx_buf = adc->rx_buf,
		.len = 3,
	};
	struct spi_message m;
	int ret;

	/* T?o command: Start bit + single-ended + channel */
	adc->tx_buf[0] = 0x01;                       /* Start bit */
	adc->tx_buf[1] = (0x08 | channel) << 4;     /* SGL + channel */
	adc->tx_buf[2] = 0x00;                       /* Don't care */

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	mutex_lock(&adc->lock);
	ret = spi_sync(adc->spi, &m);
	if (ret == 0)
		ret = ((adc->rx_buf[1] & 0x03) << 8) | adc->rx_buf[2];
	mutex_unlock(&adc->lock);

	return ret;
}

/* IIO read callback */
static int mcp3008_read_raw(struct iio_dev *indio_dev,
                             struct iio_chan_spec const *chan,
                             int *val, int *val2, long mask)
{
	struct mcp3008_data *adc = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = mcp3008_read_channel(adc, chan->channel);
		if (ret < 0)
			return ret;
		*val = ret;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		/* Vref / 2^10 = 1800 mV / 1024 = 1.7578125 */
		*val = 1800;       /* mV */
		*val2 = 10;        /* bits */
		return IIO_VAL_FRACTIONAL_LOG2;

	default:
		return -EINVAL;
	}
}

/* �?nh nghia IIO channels */
#define MCP3008_CHANNEL(num)                            \
	{                                                   \
		.type = IIO_VOLTAGE,                            \
		.indexed = 1,                                   \
		.channel = (num),                               \
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),    \
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
	}

static const struct iio_chan_spec mcp3008_channels[] = {
	MCP3008_CHANNEL(0),
	MCP3008_CHANNEL(1),
	MCP3008_CHANNEL(2),
	MCP3008_CHANNEL(3),
	MCP3008_CHANNEL(4),
	MCP3008_CHANNEL(5),
	MCP3008_CHANNEL(6),
	MCP3008_CHANNEL(7),
};

static const struct iio_info mcp3008_info = {
	.read_raw = mcp3008_read_raw,
};

static int mcp3008_probe(struct spi_device *spi)
{
	struct iio_dev *indio_dev;
	struct mcp3008_data *adc;

	indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*adc));
	if (!indio_dev)
		return -ENOMEM;

	adc = iio_priv(indio_dev);
	adc->spi = spi;
	mutex_init(&adc->lock);
	spi_set_drvdata(spi, indio_dev);

	/* C?u h�nh SPI */
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	if (!spi->max_speed_hz)
		spi->max_speed_hz = 1000000;
	spi_setup(spi);

	/* C?u h�nh IIO device */
	indio_dev->name = "mcp3008";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &mcp3008_info;
	indio_dev->channels = mcp3008_channels;
	indio_dev->num_channels = ARRAY_SIZE(mcp3008_channels);

	/* Test d?c channel 0 */
	int raw = mcp3008_read_channel(adc, 0);
	if (raw >= 0)
		dev_info(&spi->dev, "CH0 raw: %d\n", raw);

	return devm_iio_device_register(&spi->dev, indio_dev);
}

static void mcp3008_remove(struct spi_device *spi)
{
	dev_info(&spi->dev, "MCP3008 removed\n");
}

static const struct of_device_id mcp3008_of_match[] = {
	{ .compatible = "learn,mcp3008" },
	{ },
};
MODULE_DEVICE_TABLE(of, mcp3008_of_match);

static struct spi_driver mcp3008_driver = {
	.driver = {
		.name = "learn-mcp3008",
		.of_match_table = mcp3008_of_match,
	},
	.probe = mcp3008_probe,
	.remove = mcp3008_remove,
};

module_spi_driver(mcp3008_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MCP3008 SPI ADC driver with IIO (learning)");
```

---

## 4. Device Tree

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

    mcp3008@0 {
        compatible = "learn,mcp3008";
        reg = <0>;
        spi-max-frequency = <1000000>;
    };
};
```

---

## 5. Makefile

```makefile
obj-m += mcp3008_drv.o

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
```

---

## 6. Test

```bash
# Load driver
sudo insmod mcp3008_drv.ko

# T�m IIO device
ls /sys/bus/iio/devices/
# iio:device0

# �?c channel 0
cat /sys/bus/iio/devices/iio:device0/in_voltage0_raw
# 512

# �?c scale
cat /sys/bus/iio/devices/iio:device0/in_voltage_scale
# 1.757812500

# Voltage (mV) = raw * scale
# 512 * 1.758 = 899.8 mV
```

---

## 7. Ki?n th?c c?t l�i

1. **SPI protocol** c?a sensor: d?c datasheet d? bi?t format TX/RX
2. **`spi_message` + `spi_transfer`** cho full-duplex SPI
3. **IIO subsystem**: `iio_chan_spec`, `read_raw`, `IIO_VAL_INT`, `IIO_VAL_FRACTIONAL_LOG2`
4. **`devm_iio_device_register()`** t? cleanup khi remove
5. **DMA alignment**: `__aligned(IIO_DMA_MINALIGN)` cho SPI buffer

---

## 8. C�u h?i ki?m tra

1. MCP3008 c?n 3 byte SPI transaction. Gi?i th�ch format t?ng byte TX/RX.
2. T?i sao d�ng 1 `spi_transfer` v?i len=3 thay v� 3 transfer ri�ng c?a 1 byte?
3. `IIO_VAL_FRACTIONAL_LOG2` tr? v? gi� tr? scale ra sao?
4. T?i sao c?n `__aligned(IIO_DMA_MINALIGN)` cho SPI buffer?
5. L�m sao th�m differential channel cho MCP3008?

---

## 9. Chu?n b? cho b�i sau

B�i ti?p theo: **B�i 23 - UART/Serial Driver**

Ta s? h?c serial core framework: `uart_driver`, `uart_port`, `uart_ops` v� AM335x UART registers.
