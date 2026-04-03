# Bài 3 - Linux Boot Process & Kernel Build trên BeagleBone Black

## Mục tiêu
- Hiểu toàn bộ quy trình boot của BBB: ROM → MLO → U-Boot → Kernel → rootfs
- Biết cách lấy, cấu hình, và build Linux kernel cho AM335x
- Hiểu khi nào Device Tree được load và driver module được init
- Nắm thứ tự probe của driver — cơ sở cho việc viết DT overlay và module

---

## 1. Boot Sequence tổng quan trên AM335x/BBB

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    AM335x Boot Sequence                                  │
│                                                                          │
│  Power ON                                                                │
│     │                                                                    │
│     ▼                                                                    │
│  ┌─────────────┐   ROM code đọc SYSBOOT pins                           │
│  │ ROM Bootloader│   để xác định boot device                             │
│  │ (on-chip ROM) │   (eMMC, SD, UART, USB, Ethernet)                     │
│  │ 176KB SRAM    │   Nguồn: TRM spruh73q.pdf, Chapter 26                │
│  └──────┬────────┘                                                       │
│         │ Load MLO (SPL) vào internal SRAM (64KB usable)                │
│         ▼                                                                │
│  ┌─────────────┐   MLO = Minimal Loader (U-Boot SPL)                   │
│  │  MLO / SPL   │   Khởi tạo: PMIC, DDR3 (512MB), clock PLLs          │
│  │  (SRAM 64KB) │   Nguồn: BBB SRM, Chapter 7                          │
│  └──────┬────────┘                                                       │
│         │ Load u-boot.img vào DDR3                                      │
│         ▼                                                                │
│  ┌─────────────┐   U-Boot: full bootloader                              │
│  │   U-Boot      │   - Đọc uEnv.txt từ boot partition                   │
│  │   (DDR3)      │   - Load zImage (kernel) vào DDR3                    │
│  │               │   - Load am335x-boneblack.dtb vào DDR3               │
│  │               │   - Truyền DTB address cho kernel qua r2             │
│  └──────┬────────┘                                                       │
│         │ bootz ${loadaddr} - ${fdtaddr}                                │
│         ▼                                                                │
│  ┌─────────────┐   Kernel:                                              │
│  │ Linux Kernel  │   1. Parse DTB → tạo device nodes (of_platform)       │
│  │ + DTB         │   2. Init core subsystems (clocksource, irqchip...)  │
│  │ (DDR3)        │   3. Probe built-in drivers theo of_match_table       │
│  │               │   4. Mount rootfs (eMMC hoặc SD)                     │
│  │               │   5. Exec /sbin/init → systemd/SysVinit              │
│  └──────┬────────┘                                                       │
│         │ modprobe / insmod (sau khi rootfs mount)                       │
│         ▼                                                                │
│  ┌─────────────┐   External modules (.ko):                              │
│  │  Loadable     │   - Platform driver match DT node → probe()          │
│  │  Modules .ko  │   - i2c_driver, spi_driver, ... match compatible     │
│  └──────────────┘                                                        │
└──────────────────────────────────────────────────────────────────────────┘
```

### 1.1 Boot Device Selection trên BBB

BBB mặc định boot từ eMMC. Nhấn giữ **S2 (User/Boot button)** khi cấp nguồn → boot từ microSD.

| SYSBOOT[4:0] | Boot Device | BBB Config |
|---|---|---|
| 11100 | eMMC (MMC1) | Mặc định |
| 11000 | microSD (MMC0) | Giữ S2 button |

> Nguồn: TRM spruh73q.pdf, Chapter 26 "Initialization", Table 26-7

### 1.2 Memory Layout khi boot

```
DDR3 Address Map (BBB 512MB):
0x80000000 ─────────── DDR3 start
    │
    │  0x80008000       zImage (kernel) — loaded by U-Boot
    │  
    │  0x88000000       am335x-boneblack.dtb — loaded by U-Boot
    │
    │  0x88080000       ramdisk (nếu dùng initramfs)
    │
0x9FFFFFFF ─────────── DDR3 end (512MB)
```

> Nguồn: U-Boot environment, `printenv` trên BBB UART console

---

## 2. Kernel Source Tree cho AM335x

### 2.1 Lấy kernel source

```bash
# Clone kernel source (TI SDK hoặc mainline)
# Cách 1: TI SDK kernel (recommended cho BBB)
git clone https://github.com/beagleboard/linux.git -b 5.10 --depth=1
cd linux

# Cách 2: Mainline kernel (nếu muốn upstream)
git clone https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git --depth=1
```

### 2.2 Cấu trúc source tree liên quan AM335x

```
linux/
├── arch/arm/
│   ├── boot/dts/
│   │   ├── am335x-bone-common.dtsi   ← DT chung cho tất cả BeagleBone
│   │   ├── am335x-boneblack.dts      ← DT riêng cho BBB (include common)
│   │   └── am33xx.dtsi               ← SoC-level DT (periph addresses, IRQs)
│   ├── configs/
│   │   └── omap2plus_defconfig       ← Default config cho AM335x
│   └── mach-omap2/                   ← AM335x-specific machine code
├── drivers/
│   ├── gpio/gpio-omap.c              ← TI GPIO driver (gpio_chip)
│   ├── pinctrl/pinctrl-single.c      ← Pinmux driver cho AM335x
│   ├── i2c/busses/i2c-omap.c         ← TI I2C adapter driver
│   ├── spi/spi-omap2-mcspi.c         ← TI McSPI driver
│   ├── serial/omap-serial.c          ← TI UART driver
│   ├── pwm/pwm-tipwmss.c             ← TI PWM subsystem
│   ├── iio/adc/ti_am335x_adc.c       ← TI ADC IIO driver
│   ├── dma/ti/edma.c                 ← EDMA3 DMA engine
│   └── watchdog/omap_wdt.c           ← TI Watchdog driver
├── include/
│   └── dt-bindings/                   ← DT binding constants
└── Documentation/
    └── devicetree/bindings/           ← DT binding documentation
```

### 2.3 Cấu hình & Build kernel

```bash
# Thiết lập cross-compile environment
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

# Bước 1: Cấu hình kernel (dùng defconfig cho AM335x)
make omap2plus_defconfig

# Bước 2: Tùy chỉnh (nếu cần)
make menuconfig
# Ví dụ: bật CONFIG_GPIO_SYSFS, CONFIG_I2C_CHARDEV, CONFIG_IIO...

# Bước 3: Build kernel image
make -j$(nproc) zImage

# Bước 4: Build Device Tree Blobs
make -j$(nproc) dtbs
# Output: arch/arm/boot/dts/am335x-boneblack.dtb

# Bước 5: Build modules
make -j$(nproc) modules

# Bước 6: Install modules vào staging directory
make INSTALL_MOD_PATH=./modules_install modules_install
```

### 2.4 Deploy kernel lên BBB

```bash
# Giả sử BBB chạy và kết nối qua USB-Ethernet (10.42.0.206)

# Copy kernel image
scp arch/arm/boot/zImage debian@10.42.0.206:/boot/

# Copy DTB
scp arch/arm/boot/dts/am335x-boneblack.dtb debian@10.42.0.206:/boot/dtbs/

# Copy modules
scp -r modules_install/lib/modules/5.10.* debian@10.42.0.206:/lib/modules/

# Trên BBB: update boot config (nếu cần)
ssh debian@10.42.0.206
sudo nano /boot/uEnv.txt
# Đảm bảo: dtb=am335x-boneblack.dtb
sudo reboot
```

---

## 3. Driver Loading Order — Tại sao quan trọng

### 3.1 Built-in vs Loadable Module

| Đặc điểm | Built-in (=y) | Module (=m) |
|---|---|---|
| Khi nào load | Khi kernel init | Sau rootfs mount, khi `insmod`/`modprobe` |
| DT matching | Probe ngay khi parse DTB | Probe khi module load + DT node match |
| Phụ thuộc | Phải có đúng thứ tự trong Kconfig | `depmod` + `modules.dep` quản lý |
| Debug | `dmesg` ngay lúc boot | `dmesg` sau `insmod` |
| Ví dụ | gpio-omap, pinctrl-single | Module học viên viết (.ko) |

### 3.2 Thứ tự probe trên BBB

```
Kernel boot
    │
    ├─ 1. clocksource_probe() ← am33xx timer
    ├─ 2. irqchip_init()      ← INTC driver (omap-intc)
    ├─ 3. pinctrl probe        ← pinctrl-single (Control Module)
    ├─ 4. gpio_chip probe      ← gpio-omap (GPIO0-3)
    ├─ 5. clock framework      ← CM_PER/CM_WKUP clocks
    ├─ 6. i2c_adapter probe    ← i2c-omap (I2C0: PMIC, I2C2: user)
    ├─ 7. spi_master probe     ← spi-omap2-mcspi (SPI0)
    ├─ 8. serial probe         ← omap-serial (UART0: console)
    ├─ 9. mmc probe            ← omap_hsmmc (eMMC + SD)
    │
    ├─ Mount rootfs
    │
    └─ 10. modprobe / insmod   ← Out-of-tree modules (.ko)
         └─ platform_driver match DT node → probe()
```

### 3.3 Tại sao bạn cần biết điều này

1. **Khi viết driver**: driver của bạn phụ thuộc vào clock, pinctrl, GPIO đã probe trước — nếu chưa probe → `EPROBE_DEFER`
2. **Khi viết DT overlay**: overlay apply **sau** kernel boot → chỉ có effect nếu driver là loadable module
3. **Khi debug**: nếu driver probe fail, kiểm tra dependency chain (clock → pinctrl → gpio → i2c → your_driver)
4. **`EPROBE_DEFER`**: kernel sẽ retry probe sau — nếu dependency cuối cùng vẫn thiếu → driver không hoạt động

---

## 4. uEnv.txt — Cấu hình boot trên BBB

```bash
# /boot/uEnv.txt trên BBB
# Đây là file U-Boot đọc để biết load gì

# Kernel image
uname_r=5.10.168-ti-r71

# Device Tree
dtb=am335x-boneblack.dtb

# Device Tree Overlays (cape, custom hardware)
#dtb_overlay=/lib/firmware/BB-UART1-00A0.dtbo
#dtb_overlay=/lib/firmware/BB-SPI0-00A0.dtbo

# Kernel command line
cmdline=coherent_pool=1M net.ifnames=0 lpj=1990656 rng_core.default_quality=100 quiet

# Enable Cape Universal (cho phép runtime pinmux)
enable_uboot_cape_universal=1
```

### 4.1 Bật/tắt peripheral qua uEnv.txt

```bash
# Bật UART1 overlay
dtb_overlay=/lib/firmware/BB-UART1-00A0.dtbo

# Bật I2C2 (mặc định đã bật)
# Bật SPI0
dtb_overlay=/lib/firmware/BB-SPI0-00A0.dtbo

# Tắt HDMI (giải phóng pin cho GPIO/SPI1)
disable_uboot_overlay_video=1

# Tắt eMMC flasher
disable_uboot_overlay_emmc=1
```

---

## 5. Build Out-of-Tree Module — Quy trình chuẩn

Đây là quy trình bạn sẽ dùng cho tất cả bài học từ bài 5 trở đi:

### 5.1 Makefile chuẩn

```makefile
# Kernel source directory trên host (cross-compile)
KDIR ?= /path/to/linux-source

# Hoặc header đã cài trên BBB
# KDIR ?= /lib/modules/$(shell uname -r)/build

obj-m += my_driver.o

all:
	$(MAKE) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
		-C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
		-C $(KDIR) M=$(PWD) clean

deploy:
	scp my_driver.ko debian@10.42.0.206:/home/debian/
```

### 5.2 Build & Test workflow

```bash
# Trên host (cross-compile)
make                           # Build .ko
make deploy                    # SCP tới BBB

# Trên BBB (SSH)
ssh debian@10.42.0.206
sudo insmod my_driver.ko       # Load module
dmesg | tail -20               # Kiểm tra kernel log
lsmod | grep my_driver         # Xác nhận loaded
cat /proc/modules | grep my    # Chi tiết module

# Gỡ module
sudo rmmod my_driver
dmesg | tail -5                # Kiểm tra cleanup log
```

---

## 6. Am335x-specific: Clock & PLL khi boot

### 6.1 PLL Configuration (do MLO/U-Boot thiết lập)

| PLL | Frequency | Mục đích |
|-----|----------|---------|
| MPU PLL | 1 GHz | CPU Cortex-A8 clock |
| CORE PLL | 1 GHz → dividers | L3/L4 interconnect, peripherals |
| PER PLL | 960 MHz | USB, UART, timers |
| DDR PLL | 400 MHz | DDR3 memory clock (200MHz DDR) |
| DISP PLL | variable | LCD/HDMI pixel clock |

> Nguồn: TRM spruh73q.pdf, Chapter 8 "PRCM" (Power, Reset, Clock Management)

### 6.2 Peripheral Clock Domains (quan trọng cho driver)

```
L4_PER clock domain (100 MHz):
  ├── GPIO1, GPIO2, GPIO3
  ├── I2C1, I2C2
  ├── McSPI0, McSPI1
  ├── UART1-5
  ├── DMTIMER2-7
  └── EHRPWM0-2

L4_WKUP clock domain (100 MHz):
  ├── GPIO0
  ├── I2C0 (PMIC)
  ├── UART0 (debug console)
  ├── DMTIMER0, DMTIMER1ms
  └── ADC_TSC (TSC_ADC)
```

Khi viết driver, bạn phải **enable clock cho peripheral** trước khi truy cập register:
```c
/* Trong probe() */
struct clk *fclk = devm_clk_get(&pdev->dev, "fck");
clk_prepare_enable(fclk);
/* Hoặc dùng runtime PM (khuyến khích) */
pm_runtime_enable(&pdev->dev);
pm_runtime_get_sync(&pdev->dev);
```

---

## 7. Thực hành: Kiểm tra boot trên BBB

### 7.1 Đọc boot log

```bash
# Trên BBB
dmesg | head -100                    # Kernel boot messages
dmesg | grep -i "device tree"        # DT parsing messages
dmesg | grep -i "probe"             # Driver probe messages
dmesg | grep -i "omap"              # TI/OMAP driver messages

# Xem kernel version
uname -a

# Xem DT đang dùng
cat /proc/device-tree/model
# => "TI AM335x BeagleBone Black"

# Xem DT nodes
ls /proc/device-tree/
ls /proc/device-tree/ocp/           # On-Chip Peripherals

# Xem loaded modules
lsmod

# Xem kernel config
zcat /proc/config.gz | grep GPIO
```

### 7.2 Kiểm tra probe order

```bash
# Xem thứ tự probe của các driver
dmesg | grep -E "(registered|probed|driver)" | head -30

# Xem tất cả platform devices
ls /sys/bus/platform/devices/

# Xem GPIO controllers đã probe
ls /sys/class/gpio/
cat /sys/kernel/debug/gpio          # (cần debugfs mounted)

# Xem I2C adapters
ls /sys/bus/i2c/devices/
i2cdetect -l                        # Liệt kê I2C buses

# Xem clock tree
cat /sys/kernel/debug/clk/clk_summary  # (cần debugfs)
```

---

## Câu hỏi kiểm tra

1. **Thứ tự boot**: Liệt kê đúng thứ tự: ROM → ??? → ??? → ??? → rootfs → modules
2. **SYSBOOT**: Trên BBB, giữ nút nào để boot từ SD card thay vì eMMC?
3. **DTB**: Ai load DTB vào memory và kernel nhận DTB qua cơ chế nào?
4. **EPROBE_DEFER**: Nếu driver I2C client probe trước khi I2C adapter driver, chuyện gì xảy ra?
5. **Clock**: Trước khi readl() thanh ghi GPIO1, bạn cần enable clock nào?
6. **Out-of-tree module**: Khi build out-of-tree module, `KDIR` phải trỏ tới đâu?
7. **DT Overlay**: Tại sao DT overlay chỉ hiệu quả với loadable module, không phải built-in?
