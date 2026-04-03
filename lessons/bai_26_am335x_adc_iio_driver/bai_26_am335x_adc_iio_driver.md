# B�i 26 - AM335x ADC & IIO Subsystem

> **Tham chiếu phần cứng BBB:** Xem [adc.md](../mapping/adc.md)

## 0. BBB Connection � ADC tr�n BeagleBone Black

### 0.1 So d? k?t n?i th?c t?

```
BeagleBone Black P9 header

  P9.32 (VREFP = 1.8V) -+
                         �  Potentiometer 10kO
  P9.39 (AIN0)      ----?�  +---------+
                         +--�  Wiper  �---- P9.32 (1.8V)
  P9.34 (AGND)      --------�  Bottom �---- P9.34 (AGND)
                             +---------+

  ??  C?NH B�O QUAN TR?NG:
  AIN0-AIN6 ch?u t?i da 1.8V !
  TUY?T �?I KH�NG n?i 3.3V v�o AIN pin ? burn ADC!
  D�ng voltage divider: 3.3V ? R1/R2 ? AIN (max 1.8V)
```

### 0.2 B?ng ADC Channel ? BBB Pin

| Channel | P9 Pin | T�n | �i?n �p MAX | Ghi ch� |
|---------|--------|-----|------------|--------|
| AIN0 | P9.39 | Analog Input 0 | **1.8V** | **D�ng cho v� d? ch�nh** |
| AIN1 | P9.40 | Analog Input 1 | **1.8V** | |
| AIN2 | P9.37 | Analog Input 2 | **1.8V** | |
| AIN3 | P9.38 | Analog Input 3 | **1.8V** | |
| AIN4 | P9.33 | Analog Input 4 | **1.8V** | |
| AIN5 | P9.36 | Analog Input 5 | **1.8V** | |
| AIN6 | P9.35 | Analog Input 6 | **1.8V** | |
| VREFP | P9.32 | 1.8V Reference | 1.8V | N?i v�o d?u cao potentiometer |
| AGND  | P9.34 | Analog GND | 0V | N?i GND analog ri�ng |

### 0.3 Test ADC tr�n BBB (qua IIO sysfs � kernel d� c� driver)

```bash
# Ki?m tra IIO device d� c� s?n
ls /sys/bus/iio/devices/
# ? iio:device0

# �?c AIN0 (P9.39)
cat /sys/bus/iio/devices/iio:device0/in_voltage0_raw
# ? v� d?: 2048  (range 0-4095 cho 12-bit ADC)

# �?c t?t c? channels
for i in 0 1 2 3 4 5 6; do
    echo -n "AIN$i: "
    cat /sys/bus/iio/devices/iio:device0/in_voltage${i}_raw
done

# Chuy?n sang di?n �p th?c:
# V = raw * 1800mV / 4095
```

> **Ngu?n**: BeagleboneBlackP9HeaderTable.pdf (P9.33-P9.40); spruh73q.pdf Chapter 12 (TSC_ADC)

---

## 1. M?c ti�u b�i h?c
- Hi?u IIO (Industrial I/O) subsystem chi ti?t
- AM335x TSC_ADC subsystem (Touchscreen ADC) � base 0x44E0D000
- `iio_dev`, `iio_chan_spec`, `iio_info`
- Vi?t ADC driver d?c AIN0 (P9.39) t�ch h?p IIO framework

---

## 2. AM335x ADC

### 2.1 TSC_ADC Subsystem

AM335x c� module TSC_ADC k?t h?p Touchscreen controller v� ADC:

| Th�ng s? | Gi� tr? | Ngu?n: spruh73q.pdf |
|----------|---------|---------------------|
| Base Address | 0x44E0D000 | Chapter 12 |
| Resolution | 12-bit | Chapter 12 |
| Channels | 8 (AIN0-AIN7) | Chapter 12 |
| Input range | 0 - 1.8V | Chapter 12 |
| Sample rate | Max 200 ksps | Chapter 12 |

### 2.2 Ch�n ADC tr�n BBB

| Channel | Pin Header | T�n |
|---------|-----------|------|
| AIN0 | P9.39 | Analog Input 0 |
| AIN1 | P9.40 | Analog Input 1 |
| AIN2 | P9.37 | Analog Input 2 |
| AIN3 | P9.38 | Analog Input 3 |
| AIN4 | P9.33 | Analog Input 4 |
| AIN5 | P9.36 | Analog Input 5 |
| AIN6 | P9.35 | Analog Input 6 |
| VREF_AN | P9.32 | 1.8V reference |
| AGND | P9.34 | Analog ground |

### 2.3 Thanh ghi ADC quan tr?ng

| Register | Offset | M� t? |
|----------|--------|-------|
| CTRL | 0x40 | Control register |
| ADCSTAT | 0x44 | ADC status |
| ADCRANGE | 0x48 | ADC range |
| ADC_CLKDIV | 0x4C | Clock divider |
| STEPENABLE | 0x54 | Step enable |
| STEPCONFIG1 | 0x64 | Step 1 config |
| STEPDELAY1 | 0x68 | Step 1 delay |
| FIFO0DATA | 0x100 | FIFO 0 data |
| FIFO1DATA | 0x200 | FIFO 1 data |

### 2.4 Ho?t d?ng ADC

```
1. C?u h�nh step (channel, mode, averaging)
2. Enable step ? hardware t? chuy?n d?i
3. K?t qu? v�o FIFO
4. Software d?c t? FIFO

+----------+    +----------+    +------+
� AINx pin �---?�Step Logic�---?� FIFO �---? Software d?c
+----------+    +----------+    +------+
                     ?
              STEPCONFIG[n]
              (channel, avg)
```

---

## 3. IIO Framework

### 3.1 Ki?n tr�c

```
+------------------------------------------+
� Userspace                                �
�  /sys/bus/iio/devices/iio:deviceX/       �
�    in_voltage0_raw, in_voltage_scale     �
�  /dev/iio:deviceX (buffered mode)        �
+------------------------------------------�
� IIO Core                                 �
�  iio_device_register()                   �
�  iio_chan_spec, iio_info                 �
+------------------------------------------�
� IIO Driver (ADC, DAC, accelerometer...)  �
�  read_raw() callback                     �
+------------------------------------------�
� Hardware                                 �
+------------------------------------------+
```

### 3.2 iio_chan_spec

```c
struct iio_chan_spec {
    enum iio_chan_type type;     /* IIO_VOLTAGE, IIO_TEMP, ... */
    int channel;                /* Channel index */
    int channel2;               /* Differential pair */
    unsigned long info_mask_separate;    /* Per-channel attrs */
    unsigned long info_mask_shared_by_type;  /* Shared attrs */
    int indexed;                /* 1 = use channel number */
    struct {
        char sign;              /* 's' or 'u' */
        u8 realbits;
        u8 storagebits;
        u8 shift;
        enum iio_endian endianness;
    } scan_type;
    int scan_index;             /* For buffered mode */
};
```

### 3.3 iio_info

```c
struct iio_info {
    int (*read_raw)(struct iio_dev *indio_dev,
                     struct iio_chan_spec const *chan,
                     int *val, int *val2, long mask);
    int (*write_raw)(struct iio_dev *indio_dev,
                      struct iio_chan_spec const *chan,
                      int val, int val2, long mask);
    int (*read_event_config)(/* ... */);
    int (*write_event_config)(/* ... */);
};
```

### 3.4 Return values

| Return | � nghia | Sysfs gi� tr? |
|--------|---------|----------------|
| IIO_VAL_INT | S? nguy�n | val |
| IIO_VAL_INT_PLUS_MICRO | val + val2/1000000 | val.val2 |
| IIO_VAL_INT_PLUS_NANO | val + val2/1e9 | val.val2 |
| IIO_VAL_FRACTIONAL | val/val2 | val/val2 |
| IIO_VAL_FRACTIONAL_LOG2 | val/(1<<val2) | val/(1<<val2) |

---

## 4. AM335x ADC Driver

```c
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/delay.h>

/* TSC_ADC registers */
#define REG_CTRL         0x40
#define REG_ADCSTAT      0x44
#define REG_CLKDIV       0x4C
#define REG_STEPENABLE   0x54
#define REG_IDLECONFIG   0x58
#define REG_STEPCONFIG(n) (0x64 + (n) * 8)
#define REG_STEPDELAY(n)  (0x68 + (n) * 8)
#define REG_FIFO0CNT     0xE4
#define REG_FIFO0DATA    0x100

/* Bits */
#define CTRL_ENABLE        BIT(0)
#define CTRL_STEP_ID_TAG   BIT(1)
#define STEP_AVG_16        (0x4 << 2)
#define STEP_MODE_SW_ONE   0x00

#define AM335X_ADC_CHANNELS  8
#define AM335X_ADC_VREF_MV   1800

struct am335x_adc {
	void __iomem *base;
	struct clk *clk;
	struct mutex lock;
};

static u32 adc_read(struct am335x_adc *adc, u32 reg)
{
	return readl(adc->base + reg);
}

static void adc_write(struct am335x_adc *adc, u32 reg, u32 val)
{
	writel(val, adc->base + reg);
}

/* �?c m?t channel ADC */
static int am335x_adc_read_channel(struct am335x_adc *adc, int channel)
{
	u32 stepconfig, data;
	int timeout = 1000;

	mutex_lock(&adc->lock);

	/* Disable ADC tru?c khi config */
	adc_write(adc, REG_CTRL, adc_read(adc, REG_CTRL) & ~CTRL_ENABLE);

	/* Config step 0 cho channel */
	stepconfig = STEP_MODE_SW_ONE | STEP_AVG_16;
	stepconfig |= (channel << 19);  /* SEL_INP_SWC: channel select */
	stepconfig |= (channel << 15);  /* SEL_INM_SWC (single-ended) */
	adc_write(adc, REG_STEPCONFIG(0), stepconfig);
	adc_write(adc, REG_STEPDELAY(0), 0);

	/* Enable ADC + step 1 (step 0 trong hardware = step 1 bit) */
	adc_write(adc, REG_CTRL, CTRL_ENABLE | CTRL_STEP_ID_TAG);
	adc_write(adc, REG_STEPENABLE, BIT(1));

	/* Ch? FIFO c� data */
	while (timeout--) {
		if (adc_read(adc, REG_FIFO0CNT) > 0)
			break;
		udelay(1);
	}

	if (timeout <= 0) {
		mutex_unlock(&adc->lock);
		return -ETIMEDOUT;
	}

	data = adc_read(adc, REG_FIFO0DATA);

	/* Disable step */
	adc_write(adc, REG_STEPENABLE, 0);

	mutex_unlock(&adc->lock);

	/* FIFO data: bit[11:0] = ADC value, bit[19:16] = step/channel ID */
	return data & 0xFFF;
}

/* IIO read callback */
static int am335x_adc_read_raw(struct iio_dev *indio_dev,
                                struct iio_chan_spec const *chan,
                                int *val, int *val2, long mask)
{
	struct am335x_adc *adc = iio_priv(indio_dev);
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = am335x_adc_read_channel(adc, chan->channel);
		if (ret < 0)
			return ret;
		*val = ret;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		/* 1800 mV / 4096 = 0.439453125 mV/LSB */
		*val = AM335X_ADC_VREF_MV;
		*val2 = 12;  /* 2^12 = 4096 */
		return IIO_VAL_FRACTIONAL_LOG2;

	default:
		return -EINVAL;
	}
}

#define AM335X_ADC_CHANNEL(num)                                \
	{                                                          \
		.type = IIO_VOLTAGE,                                   \
		.indexed = 1,                                          \
		.channel = (num),                                      \
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),           \
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),  \
		.scan_index = (num),                                   \
		.scan_type = {                                         \
			.sign = 'u',                                       \
			.realbits = 12,                                    \
			.storagebits = 16,                                 \
			.endianness = IIO_CPU,                             \
		},                                                     \
	}

static const struct iio_chan_spec am335x_adc_channels[] = {
	AM335X_ADC_CHANNEL(0),
	AM335X_ADC_CHANNEL(1),
	AM335X_ADC_CHANNEL(2),
	AM335X_ADC_CHANNEL(3),
	AM335X_ADC_CHANNEL(4),
	AM335X_ADC_CHANNEL(5),
	AM335X_ADC_CHANNEL(6),
	AM335X_ADC_CHANNEL(7),
};

static const struct iio_info am335x_adc_info = {
	.read_raw = am335x_adc_read_raw,
};

static int am335x_adc_probe(struct platform_device *pdev)
{
	struct iio_dev *indio_dev;
	struct am335x_adc *adc;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*adc));
	if (!indio_dev)
		return -ENOMEM;

	adc = iio_priv(indio_dev);

	adc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(adc->base))
		return PTR_ERR(adc->base);

	mutex_init(&adc->lock);

	/* C?u h�nh IIO device */
	indio_dev->name = "am335x-adc";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &am335x_adc_info;
	indio_dev->channels = am335x_adc_channels;
	indio_dev->num_channels = ARRAY_SIZE(am335x_adc_channels);

	/* Clock divider: ADC clock = 24MHz / (clkdiv+1) = 3MHz */
	adc_write(adc, REG_CLKDIV, 7);

	/* Idle config */
	adc_write(adc, REG_IDLECONFIG, 0);

	dev_info(&pdev->dev, "AM335x ADC initialized, %d channels\n",
	         AM335X_ADC_CHANNELS);

	return devm_iio_device_register(&pdev->dev, indio_dev);
}

static const struct of_device_id am335x_adc_of_match[] = {
	{ .compatible = "learn,am335x-adc" },
	{ },
};
MODULE_DEVICE_TABLE(of, am335x_adc_of_match);

static struct platform_driver am335x_adc_driver = {
	.probe = am335x_adc_probe,
	.driver = {
		.name = "learn-am335x-adc",
		.of_match_table = am335x_adc_of_match,
	},
};

module_platform_driver(am335x_adc_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AM335x ADC driver with IIO (learning)");
```

---

## 5. Test

```bash
# Load driver
sudo insmod am335x_adc.ko

# Li?t k� IIO devices
ls /sys/bus/iio/devices/

# �?c raw value
cat /sys/bus/iio/devices/iio:device0/in_voltage0_raw
# 2048

# �?c scale
cat /sys/bus/iio/devices/iio:device0/in_voltage_scale
# 0.439453125

# T�nh voltage: 2048 * 0.439453125 = 900 mV
```

---

## 6. IIO Buffered Mode (n�ng cao)

�? d?c data li�n t?c, IIO h? tr? buffered mode:

```c
/* Khai b�o buffer support */
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>

static irqreturn_t am335x_adc_trigger_handler(int irq, void *p)
{
    struct iio_poll_func *pf = p;
    struct iio_dev *indio_dev = pf->indio_dev;
    /* �?c all channels, push to buffer */
    iio_push_to_buffers_with_timestamp(indio_dev, data, timestamp);
    iio_trigger_notify_done(indio_dev->trig);
    return IRQ_HANDLED;
}

/* Trong probe: */
devm_iio_triggered_buffer_setup(&pdev->dev, indio_dev, NULL,
    am335x_adc_trigger_handler, NULL);
```

Userspace d?c buffer:
```bash
# Enable channels
echo 1 > /sys/bus/iio/devices/iio:device0/scan_elements/in_voltage0_en
echo 1 > /sys/bus/iio/devices/iio:device0/scan_elements/in_voltage1_en

# Set buffer length
echo 100 > /sys/bus/iio/devices/iio:device0/buffer/length

# Enable buffer
echo 1 > /sys/bus/iio/devices/iio:device0/buffer/enable

# Read from /dev/iio:device0
cat /dev/iio:device0 | xxd | head
```

---

## 7. Ki?n th?c c?t l�i

1. **AM335x TSC_ADC**: 12-bit, 8 channel, step-based conversion
2. **IIO framework**: `iio_dev` + `iio_chan_spec` + `read_raw()` callback
3. **Scale**: `IIO_VAL_FRACTIONAL_LOG2` ? val/(1<<val2) = 1800/4096
4. **FIFO data**: bit[11:0] = ADC value, bit[19:16] = channel ID
5. **Step-based**: m?i step config: channel, averaging, delay
6. **Buffered mode**: continuous data v?i triggered_buffer

---

## 8. C�u h?i ki?m tra

1. AM335x ADC c� bao nhi�u channel? Range input l� bao nhi�u?
2. `IIO_VAL_FRACTIONAL_LOG2` v?i val=1800, val2=12 tr? v? g�?
3. FIFO0DATA register ch?a g�?
4. T?i sao ph?i disable ADC tru?c khi thay d?i STEPCONFIG?
5. S? kh�c nhau gi?a direct mode v� buffered mode trong IIO?

---

## 9. Chu?n b? cho b�i sau

B�i ti?p theo: **B�i 26 - Timer Driver**

Ta s? h?c clocksource, clockevent, hrtimer v� AM335x DMTIMER.
