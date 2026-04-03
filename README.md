# 📘 Ghi Chú Kiến Thức - AM335x Linux Device Driver

> File này được cập nhật sau mỗi buổi học để ôn tập lại kiến thức.
> Mỗi peripheral: **Hardware registers** (từ TRM) + **Linux Driver** (kernel API) + **Device Tree** + **Thực hành**.

---

## 📋 Mục lục — 35 Bài
- [Giai đoạn 1: Nền tảng Linux Kernel (Bài 1-9)](#giai-đoạn-1-nền-tảng)
- [Giai đoạn 2: Char Device Driver (Bài 10-11)](#giai-đoạn-2-char-device-driver)
- [Giai đoạn 3: Phương pháp Register→Driver (Bài 12)](#giai-đoạn-3-register-to-driver)
- [Giai đoạn 4: GPIO + Pinmux + IRQ (Bài 13-17)](#giai-đoạn-4-gpio--pinmux--irq)
- [Giai đoạn 5: regmap API (Bài 18)](#giai-đoạn-5-regmap)
- [Giai đoạn 6: Timer + PWM (Bài 19-20)](#giai-đoạn-6-timer--pwm)
- [Giai đoạn 7: I2C (Bài 21-22)](#giai-đoạn-7-i2c)
- [Giai đoạn 8: SPI (Bài 23-24)](#giai-đoạn-8-spi)
- [Giai đoạn 9: UART (Bài 25)](#giai-đoạn-9-uart)
- [Giai đoạn 10: ADC + DMA (Bài 26-27)](#giai-đoạn-10-adc--dma)
- [Giai đoạn 11: Advanced Peripherals (Bài 28-30)](#giai-đoạn-11-advanced-peripherals)
- [Giai đoạn 12: Kernel Interfaces & Debug (Bài 31-33)](#giai-đoạn-12-kernel-interfaces--debug)
- [Giai đoạn 13: Integration & Capstone (Bài 34-35)](#giai-đoạn-13-integration--capstone)
- [Bảng thanh ghi quan trọng AM335x](#bảng-thanh-ghi-quan-trọng)
- [Kernel API thường dùng](#kernel-api-thường-dùng)

---

## Giai đoạn 1: Nền tảng

### Bài 1 - Tổng quan AM335x SoC

- AM335x là một SoC: bên trong có CPU Cortex-A8, interconnect, bộ nhớ, clock/reset, interrupt và nhiều peripheral.
- Peripheral được điều khiển bằng memory-mapped I/O: CPU đọc/ghi vào địa chỉ nhớ đặc biệt để tác động phần cứng.
- Công thức cơ bản khi làm việc với thanh ghi:

```text
register_address = base_address + offset
```

- Khi debug một peripheral không hoạt động, luôn kiểm tra theo thứ tự:
	1. Clock của module
	2. Pinmux trong Control Module
	3. Đúng base address, offset và bit-field

- So sánh nhanh:
	- Bare-metal: truy cập trực tiếp phần cứng, timing tốt hơn, nhưng phải tự xử lý nhiều thứ.
	- Linux: có kernel quản lý driver và virtual memory, thuận tiện hơn cho hệ thống phức tạp.

### Bài 2 - Tổng quan phần cứng BeagleBone Black

- BBB là một board hoàn chỉnh, không chỉ là AM335x: có DDR3, eMMC, PMIC, Ethernet PHY, USB PHY, oscillator và header mở rộng P8/P9.
- Hai header P8/P9 là giao diện để đưa tín hiệu nội của AM335x ra ngoài board.
- Khái niệm quan trọng nhất là **pinmux**: một chân vật lý có thể đảm nhiệm nhiều chức năng (GPIO/UART/I2C/SPI/PWM...).
- Control Module (`0x44E10000`) là nơi cấu hình pad/pinmux trước khi dùng GPIO hoặc peripheral tương ứng.
- Với GPIO:
	- `GPIOx_y` nghĩa là module `x`, bit `y`
	- Mỗi module GPIO có vùng địa chỉ riêng và layout thanh ghi giống nhau
	- Các thanh ghi dữ liệu quan trọng: `GPIO_OE`, `GPIO_DATAIN`, `GPIO_DATAOUT`, `GPIO_SETDATAOUT`, `GPIO_CLEARDATAOUT`
- Trình tự kiểm tra khi thao tác chân ngoài:
	1. Tra đúng mapping chân trong bảng P8/P9
	2. Cấu hình pinmux đúng mode
	3. Bật clock module
	4. Đọc/ghi đúng thanh ghi và bit

### Bài 3–9 — Boot Process, Môi trường, Kernel Module, Device Tree, MMIO, Platform Driver, devm

*(Sẽ được cập nhật khi học)*

---

## Giai đoạn 2: Char Device Driver

### Bài 10-11 — cdev, file_operations, ioctl

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 3: Register to Driver

### Bài 12 — Phương pháp luận: TRM → register set → kernel subsystem → driver code

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 4: GPIO + Pinmux + IRQ

### Bài 13-17 — GPIO hardware, gpiod, Pinctrl, INTC, Concurrency

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 5: regmap

### Bài 18 — regmap_config, regmap_read/write, regmap_update_bits, cache, debugfs

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 6: Timer + PWM

### Bài 19-20 — DMTIMER registers + driver, EHRPWM registers + pwm_chip

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 7: I2C

### Bài 21-22 — I2C registers + i2c_adapter, I2C client driver

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 8: SPI

### Bài 23-24 — McSPI registers + spi_master, SPI client driver

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 9: UART

### Bài 25 — UART registers + uart_port + serial core

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 10: ADC + DMA

### Bài 26-27 — TSC_ADC + IIO, EDMA + dmaengine

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 11: Advanced Peripherals

### Bài 28-30 — Watchdog, Clock/PM, Input/LED subsystem

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 12: Kernel Interfaces & Debug

### Bài 31-33 — sysfs/debugfs, debug techniques, DT overlay

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Giai đoạn 13: Integration & Capstone

### Bài 34-35 — Complete driver integration, Capstone project

*(Sẽ được cập nhật sau mỗi bài học)*

---

## Bảng thanh ghi quan trọng

| Peripheral | Base Address | TRM Section | Thanh ghi chính |
|------------|-------------|-------------|-----------------|
| Control Module (Pinmux) | `0x44E10000` | Ch.9 | `conf_xxx` (pad config) |
| CM_PER (Clock) | `0x44E00000` | Ch.8 | `CM_PER_GPIOx_CLKCTRL` |
| CM_WKUP (Clock) | `0x44E00400` | Ch.8 | `CM_WKUP_xxx_CLKCTRL` |
| GPIO0 | `0x44E07000` | Ch.25 | `GPIO_OE`, `GPIO_SETDATAOUT`, `GPIO_CLEARDATAOUT`, `GPIO_DATAIN` |
| GPIO1 | `0x4804C000` | Ch.25 | (same layout) |
| GPIO2 | `0x481AC000` | Ch.25 | (same layout) |
| GPIO3 | `0x481AE000` | Ch.25 | (same layout) |
| UART0 | `0x44E09000` | Ch.19 | `DLL`, `DLH`, `LCR`, `FCR`, `LSR` |
| UART1 | `0x48022000` | Ch.19 | (same layout) |
| I2C0 | `0x44E0B000` | Ch.21 | `I2C_CON`, `I2C_SA`, `I2C_DATA`, `I2C_IRQSTATUS` |
| I2C1 | `0x4802A000` | Ch.21 | (same layout) |
| I2C2 | `0x4819C000` | Ch.21 | (same layout) |
| McSPI0 | `0x48030000` | Ch.24 | `MCSPI_MODULCTRL`, `CH0CONF`, `TX0`, `RX0` |
| DMTIMER0 | `0x44E05000` | Ch.20 | `TCLR`, `TCRR`, `TLDR`, `TIOCP_CFG` |
| DMTIMER2 | `0x48040000` | Ch.20 | (same layout) |
| EHRPWM0 | `0x48300200` | Ch.15 | `TBCTL`, `TBPRD`, `CMPA`, `AQCTLA` |
| EHRPWM1 | `0x48302200` | Ch.15 | (same layout) |
| TSC_ADC | `0x44E0D000` | Ch.12 | `CTRL`, `STEPCONFIG`, `FIFO0DATA` |
| EDMA3CC | `0x49000000` | Ch.11 | PaRAM Set, `DCHMAP`, `EESR` |
| WDT1 | `0x44E35000` | Ch.26 | `WCLR`, `WCRR`, `WLDR`, `WSPR` |
| INTC | `0x48200000` | Ch.6 | `INTC_MIR_SET`, `INTC_PENDING_IRQ` |

---

## Kernel API thường dùng

| Category | API | Mô tả |
|----------|-----|--------|
| **Module** | `module_init/exit` | Đăng ký init/cleanup |
| **Platform** | `platform_driver_register` | Đăng ký platform driver |
| **MMIO** | `devm_ioremap_resource` | Map MMIO region (managed) |
| **MMIO** | `readl / writel` | Đọc/ghi 32-bit register |
| **regmap** | `regmap_read / regmap_write` | Đọc/ghi register qua regmap abstraction |
| **regmap** | `regmap_update_bits` | Atomic read-modify-write trên bit fields |
| **regmap** | `devm_regmap_init_mmio` | Khởi tạo regmap cho MMIO backend |
| **IRQ** | `devm_request_irq` | Đăng ký IRQ handler |
| **GPIO** | `gpiochip_add_data` | Đăng ký gpio_chip |
| **GPIO** | `devm_gpiod_get` | Lấy GPIO descriptor |
| **I2C** | `i2c_add_adapter` | Đăng ký i2c_adapter |
| **SPI** | `spi_register_master` | Đăng ký spi_controller |
| **PWM** | `pwmchip_add` | Đăng ký pwm_chip |
| **IIO** | `iio_device_register` | Đăng ký IIO device |
| **DMA** | `dma_request_slave_channel` | Lấy DMA channel |
| **Clock** | `devm_clk_get / clk_prepare_enable` | Bật clock |
| **PM** | `pm_runtime_enable/get_sync` | Runtime power management |
| **Userspace** | `copy_to/from_user` | Truyền data kernel ↔ user |
| **Sysfs** | `DEVICE_ATTR_RW` | Tạo sysfs attribute |
| **Concurrency** | `mutex_lock/unlock` | Mutual exclusion (sleepable) |
| **Concurrency** | `spin_lock/unlock` | Spinlock (non-sleepable, IRQ context) |

---

> **Tip**: Luôn mở `BBB_docs/datasheets/spruh73q.pdf` khi học để tra cứu thanh ghi. Mỗi bài hardware đều có số trang TRM tương ứng.
