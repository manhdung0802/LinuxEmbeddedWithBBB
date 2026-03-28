# Bài 31 - Tạo BSP Layer Riêng: meta-bbb-custom

## 1. Mục tiêu bài học

- Hiểu BSP layer là gì và tại sao cần tạo layer riêng
- Tạo hoàn chỉnh `meta-bbb-custom` với đúng cấu trúc
- Viết `layer.conf` với tất cả fields cần thiết
- Tạo custom machine configuration cho BBB variant
- Override U-Boot và kernel recipe qua bbappend
- Thêm layer custom vào build và verify hoạt động

---

## 2. BSP Layer Là Gì?

**BSP (Board Support Package) Layer** là tập hợp recipes và config files mô tả:
- Phần cứng của board (CPU, RAM, peripherals)
- Kernel configuration cho board đó
- Bootloader (U-Boot) configuration
- Device Trees
- Drivers và firmware đặc biệt

```
Cấu trúc layer trong Yocto:
─────────────────────────────────────────────────────
meta                ← OE-Core (base, luôn có)
meta-poky           ← Poky distro config
meta-yocto-bsp      ← Yocto reference BSPs
meta-ti             ← TI SoC BSP (AM335x, AM64x...)
meta-ti/meta-ti-bsp ← TI sub-layer (kernel, u-boot)
─────────────────────────────────────────────────────
meta-bbb-custom     ← Layer của BẠN (bài này tạo)
─────────────────────────────────────────────────────

Mục đích meta-bbb-custom:
  ✓ Override configs của meta-ti (không fork)
  ✓ Thêm recipes cho ứng dụng tự viết
  ✓ Custom machine configuration (variant BBB)
  ✓ Tất cả board-specific patches và configs
```

---

## 3. Cấu Trúc Thư Mục Layer

```
meta-bbb-custom/
│
├── conf/
│   ├── layer.conf                  ← BẮTBUỘC: Khai báo layer
│   ├── machine/
│   │   └── bbb-custom.conf         ← Custom machine config
│   └── distro/
│       └── bbb-production.conf     ← Custom distro (optional)
│
├── recipes-bsp/
│   ├── u-boot/
│   │   ├── u-boot-ti-staging_%.bbappend
│   │   └── files/
│   │       └── bbb-custom.env      ← Custom U-Boot env
│   └── am335x-fw/                  ← Firmware files nếu cần
│
├── recipes-kernel/
│   └── linux/
│       ├── linux-ti_%.bbappend
│       └── files/
│           ├── bbb-custom.cfg      ← Kernel config fragment
│           └── 0001-custom.patch   ← Kernel patch (nếu có)
│
├── recipes-bbb/
│   ├── images/
│   │   └── my-bbb-image.bb         ← Custom image
│   ├── led-blinker/
│   │   ├── led-blinker_1.0.bb
│   │   └── files/
│   │       ├── led_blinker.c
│   │       └── led-blinker.service
│   └── sensor-logger/
│       ├── sensor-logger_1.0.bb
│       └── files/
│           ├── sensor_logger.c
│           └── sensor-logger.service
│
└── README
```

---

## 4. Tạo layer.conf

```bash
mkdir -p ~/yocto-bbb/meta-bbb-custom/conf
```

```bitbake
# meta-bbb-custom/conf/layer.conf

# BBPATH: thêm thư mục này vào tìm kiếm classes/configs
BBPATH .= ":${LAYERDIR}"

# BBFILES: pattern để tìm recipe files
BBFILES += "${LAYERDIR}/recipes-*/*/*.bb \
             ${LAYERDIR}/recipes-*/*/*.bbappend"

# BBFILE_COLLECTIONS: tên duy nhất cho layer này
BBFILE_COLLECTIONS += "bbb-custom"

# BBFILE_PATTERN: regex để map file về collection này
BBFILE_PATTERN_bbb-custom = "^${LAYERDIR}/"

# BBFILE_PRIORITY: ưu tiên override recipe từ layers khác
# meta = 5, meta-oe = 6, meta-ti = 8 → dùng 10 để override tất cả
BBFILE_PRIORITY_bbb-custom = "10"

# LAYERVERSION: version của layer
LAYERVERSION_bbb-custom = "1"

# LAYERSERIES_COMPAT: khai báo tương thích với Yocto releases
# BitBake sẽ cảnh báo nếu build với release không tương thích
LAYERSERIES_COMPAT_bbb-custom = "kirkstone scarthgap"

# LAYERDEPENDS: layer này phụ thuộc vào layer nào
LAYERDEPENDS_bbb-custom = "core openembedded-layer meta-ti-bsp"
```

---

## 5. Tạo Custom Machine Configuration

Machine configuration mô tả phần cứng board cụ thể:

```bash
mkdir -p ~/yocto-bbb/meta-bbb-custom/conf/machine
```

```bitbake
# meta-bbb-custom/conf/machine/bbb-custom.conf
#
# Custom BBB machine với tweaks cho sản phẩm của chúng ta
# Kế thừa từ beaglebone (meta-ti)

require conf/machine/beaglebone.conf

MACHINEOVERRIDES =. "beaglebone:"

# ──── Override machine name ────────────────────────────────────
MACHINE_FEATURES:append = " can wifi"
# MACHINE_FEATURES mặc định: usbgadget usbhost vfat ext2 alsa

# ──── Custom kernel ────────────────────────────────────────────
# Giữ kernel của meta-ti (linux-ti), chỉ thêm config fragment
# qua linux-ti_%.bbappend trong layer này

# ──── Serial console ───────────────────────────────────────────
SERIAL_CONSOLES = "115200;ttyS0 115200;ttyO0"

# ──── Custom U-Boot environment ────────────────────────────────
# Không cần thay đổi nếu hardware giống BBB chuẩn

# ──── Image formats ────────────────────────────────────────────
IMAGE_FSTYPES:append = " wic"

# ──── Extra packages cho machine này ──────────────────────────
MACHINE_EXTRA_RRECOMMENDS = " \
    kernel-modules \
    linux-firmware-carl9170 \
    "
```

---

## 6. Override U-Boot Configuration

```bash
mkdir -p ~/yocto-bbb/meta-bbb-custom/recipes-bsp/u-boot/files
```

```bitbake
# recipes-bsp/u-boot/u-boot-ti-staging_%.bbappend

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI:append = " file://bbb-custom-env.h"

# Override U-Boot config để dùng custom defconfig
# UBOOT_MACHINE = "am335x_bbb_defconfig"

# Thêm do_configure hook
do_configure:append() {
    # Copy custom env file vào source
    cp ${WORKDIR}/bbb-custom-env.h \
        ${S}/include/configs/bbb-custom-env.h
}
```

```c
/* files/bbb-custom-env.h — Custom U-Boot environment */
#ifndef __BBB_CUSTOM_ENV_H
#define __BBB_CUSTOM_ENV_H

/* Boot delay 2 giây (mặc định 0 trong meta-ti) */
#define CONFIG_BOOTDELAY  2

/* Custom boot arguments */
#define CUSTOM_BOOTARGS \
    "console=ttyO0,115200n8 " \
    "root=/dev/mmcblk0p2 rw " \
    "rootfstype=ext4 " \
    "rootwait " \
    "quiet"

#endif
```

---

## 7. Override Kernel Configuration

```bash
mkdir -p ~/yocto-bbb/meta-bbb-custom/recipes-kernel/linux/files
```

```bitbake
# recipes-kernel/linux/linux-ti_%.bbappend

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Thêm kernel config fragment
SRC_URI:append = " file://bbb-custom.cfg"

# Thêm device tree custom nếu cần
# SRC_URI:append = " file://am335x-bbb-custom.dts"

# do_configure:append() {
#     cp ${WORKDIR}/am335x-bbb-custom.dts \
#         ${S}/arch/arm/boot/dts/
# }
```

```
# files/bbb-custom.cfg — Kernel config cho BBB custom product
# Các CONFIG phải theo định dạng: CONFIG_XXX=y/m/n
# Hoặc: # CONFIG_XXX is not set

## I2C
CONFIG_I2C=y
CONFIG_I2C_CHARDEV=y                # /dev/i2c-* user access
CONFIG_I2C_OMAP=y                   # OMAP I2C master driver

## SPI
CONFIG_SPI=y
CONFIG_SPI_MASTER=y
CONFIG_SPI_SPIDEV=y                 # /dev/spidev* user access
CONFIG_SPI_OMAP24XX=y               # OMAP McSPI driver

## CAN Bus
CONFIG_CAN=y
CONFIG_CAN_RAW=y
CONFIG_CAN_BCM=y
CONFIG_CAN_VCAN=y
CONFIG_CAN_C_CAN=y
CONFIG_CAN_C_CAN_PLATFORM=y        # AM335x DCAN

## Watchdog
CONFIG_WATCHDOG=y
CONFIG_OMAP_WATCHDOG=y             # AM335x hardware WDT
CONFIG_WATCHDOG_NOWAYOUT=y         # Không thể tắt sau khi bật

## GPIO
CONFIG_GPIO_SYSFS=y                # /sys/class/gpio (deprecated nhưng vẫn dùng)

## Real-time clock
CONFIG_RTC_DRV_OMAP=y

## Disable không cần
# CONFIG_SOUND is not set
# CONFIG_USB_AUDIO is not set
# CONFIG_VIDEO_DEV is not set
```

---

## 8. Thêm Layer vào Build

```bash
# Cách 1: Sửa bblayers.conf
nano ~/yocto-bbb/build-bbb/conf/bblayers.conf

# Thêm dòng:
# /home/user/yocto-bbb/meta-bbb-custom \

# Cách 2: Dùng bitbake-layers (tự động)
cd ~/yocto-bbb/build-bbb
bitbake-layers add-layer ~/yocto-bbb/meta-bbb-custom

# Verify
bitbake-layers show-layers
# Phải thấy: bbb-custom  .../meta-bbb-custom  10

# Kiểm tra machine mới có trong list
bitbake-layers show-recipes "*boneblack*"
```

---

## 9. Chuyển sang Machine Mới

```bash
# Trong local.conf
# MACHINE = "beaglebone"         ← Comment/xóa
MACHINE = "bbb-custom"           ← Dùng machine mới

# Build lại
bitbake my-bbb-image

# Kiểm tra machine config được đọc
bitbake -e | grep "^MACHINE ="
# MACHINE = "bbb-custom"
```

---

## 10. Kiểm Tra Layer Priority

Khi nhiều layers có recipe cùng tên, layer có priority cao hơn sẽ được dùng:

```bash
# Xem recipe nào đang được dùng
bitbake-layers show-recipes linux-ti

# Output:
# linux-ti:
#   meta-ti-bsp    5.10.168-r0
# (không bị override vì chỉ dùng bbappend)

# Debug: Xem giá trị biến sau khi tất cả layers merge
bitbake -e linux-ti | grep "^SRC_URI"
bitbake -e my-bbb-image | grep "^IMAGE_INSTALL"
```

---

## 11. Quản Lý Layer Qua Git

```bash
# Khởi tạo git repo cho layer custom
cd ~/yocto-bbb/meta-bbb-custom
git init
echo "# meta-bbb-custom" > README
git add -A
git commit -m "Initial layer structure"

# Best practice: đặt tên tag theo Yocto release
git tag "kirkstone-1.0"

# .gitignore cho layer
cat > .gitignore << 'EOF'
*.pyc
__pycache__/
*.swp
EOF
```

---

## 12. Câu hỏi kiểm tra

1. `BBFILE_PRIORITY_bbb-custom = "10"` nghĩa là gì? Nếu đặt priority = 5 (bằng OE-Core) thì điều gì xảy ra?
2. `LAYERSERIES_COMPAT_bbb-custom = "kirkstone scarthgap"` — Nếu thiếu field này có sao không?
3. Tại sao `recipes-kernel/linux/linux-ti_%.bbappend` dùng `%` thay vì version cụ thể?
4. `LAYERDEPENDS_bbb-custom = "core meta-ti-bsp"` làm gì? Nếu thiếu `meta-ti-bsp` thì sao?
5. Khi `bbb-custom.conf` có `require conf/machine/beaglebone.conf`, điều gì xảy ra với MACHINE_FEATURES đã được define trong `beaglebone.conf`?
6. Cách tìm xem layer nào cung cấp recipe `u-boot-ti-staging` và version nào đang được dùng?
