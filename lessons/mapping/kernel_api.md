## 15. Kernel API nhanh

### 15.1 Peripheral → Kernel API Map

| Peripheral | Kernel Subsystem | Driver struct | Probe API |
|-----------|-----------------|--------------|----------|
| GPIO | gpiolib | `gpio_chip` | `devm_gpiochip_add_data` |
| GPIO consumer | gpiod | `gpio_desc` | `devm_gpiod_get` |
| Pinmux | pinctrl | `pinctrl_dev` | `devm_pinctrl_register` |
| Interrupt | irqchip | `irq_domain` | `irq_domain_add_linear` |
| I2C adapter | I2C core | `i2c_adapter` | `i2c_add_adapter` |
| I2C client | I2C core | `i2c_driver` | `i2c_register_driver` |
| SPI master | SPI core | `spi_master` | `devm_spi_register_master` |
| SPI client | SPI core | `spi_driver` | `spi_register_driver` |
| UART | serial core | `uart_port` | `uart_add_one_port` |
| PWM | pwm framework | `pwm_chip` | `devm_pwmchip_add` |
| Timer | clockevent | `clock_event_device` | `clockevents_register_device` |
| ADC (IIO) | IIO framework | `iio_dev` | `devm_iio_device_register` |
| DMA | dmaengine | `dma_chan` | `dma_request_chan` |
| Watchdog | watchdog core | `watchdog_device` | `devm_watchdog_register_device` |
| LED | LED class | `led_classdev` | `devm_led_classdev_register` |
| Input | input core | `input_dev` | `devm_input_allocate_device` |

### 15.2 Flow: AM335x Register → Linux Driver API

```
Bước 1: TRM tìm base address + IRQ
    GPIO1: base=0x4804C000, IRQ=98

Bước 2: Enable clock (CM_PER)
    -  Kernel: pm_runtime_get_sync(dev)
    -  Manual: writel(2, cm_base + CM_PER_GPIO1_CLKCTRL)

Bước 3: MMIO map
    - Kernel: devm_ioremap_resource(dev, res)
    - DT:     platform_get_resource(pdev, IORESOURCE_MEM, 0)

Bước 4: Config registers
    - readl(base + GPIO_OE), writel(val, base + GPIO_OE)

Bước 5: Request IRQ
    - devm_request_irq(dev, irq, handler, IRQF_*, name, priv)
    - DT:     platform_get_irq(pdev, 0)

Bước 6: Register với kernel subsystem
    - gpio: devm_gpiochip_add_data(dev, &chip, priv)
    - i2c:  i2c_add_adapter(&adapter)
    - spi:  devm_spi_register_master(dev, master)
    - uart: uart_add_one_port(&driver, &port)
    - pwm:  devm_pwmchip_add(dev, &chip)
    - iio:  devm_iio_device_register(dev, indio_dev)

Bước 7: Device Tree binding
    - compatible = "ti,am335x-gpio" (ví dụ)
    - reg = <0x4804C000 0x1000>
    - interrupts = <98>
    - clocks = <&l4ls_gclk>

Bước 8: Test trên BBB
    - dmesg | grep driver_name
    - cat /sys/class/gpio/gpio53/value
    - i2cdetect -y -r 2
    - cat /sys/bus/iio/devices/iio:device0/in_voltage0_raw
```

> **Tài liệu đầy đủ:** AM335x TRM `spruh73q.pdf` cho register-level, Linux kernel docs (`Documentation/driver-api/`) cho API.

---

*File này được dùng như tài liệu tra cứu nhanh trong mọi bài học. Mỗi bài peripheral sẽ tham chiếu back về đây cho thông tin pin/register cụ thể.*